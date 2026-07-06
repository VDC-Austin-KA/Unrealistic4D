// Copyright Earth4D. Licensed for project use.
// Phase 3: the material-driven half of the apply loop.
//
// The schedule evaluator (Earth4DScheduleEvaluator) produces an FEarth4DObjectState
// per element per day. The TRANSFORM bits (visibility / ENU offset / scale / spin)
// are written straight onto the component by UEarth4DSubsystem::EvaluateAndApply.
// The remaining bits — opacity (fade), section-reveal clip plane, and action glow —
// are surface effects that must be driven through the element's MATERIALS. This
// helper owns that: it lazily promotes each element's material slots to Dynamic
// Material Instances (MIDs) and pushes a named-parameter contract onto them.
//
// Master-material parameter contract (author these on your construction master
// material so the effects render; see EnsureDynamicMaterials / ApplyState):
//   Scalar  Earth4D_Opacity        0..1  surface opacity (drive a dither/mask or translucency)
//   Scalar  Earth4D_ClipEnable     0/1   enable the section-reveal clip
//   Vector  Earth4D_ClipNormal     xyz   WORLD-space unit plane normal (w unused)
//   Scalar  Earth4D_ClipConstant   cm    WORLD-space plane constant: keep where
//                                        dot(AbsoluteWorldPosition, ClipNormal) - ClipConstant >= 0
//   Vector  Earth4D_GlowColor      rgb   emissive action-glow colour
//   Scalar  Earth4D_GlowIntensity  >=0   emissive action-glow strength (0 = off)
//   Vector  Earth4D_TintColor      rgb   per-element manual colour override
//   Scalar  Earth4D_TintAmount     0..1  blend toward TintColor (0 = base albedo)
//
// SAFE FALLBACK: if the assigned material does NOT expose Earth4D_Opacity, the
// element can't fade, so EFadeFallback::Auto (the default) hides the whole
// component once opacity drops below VisibilityOpacityCutoff. That keeps
// appear/disappear correct even with stock opaque materials.
#pragma once

#include "CoreMinimal.h"
#include "Earth4DTypes.h"

class UPrimitiveComponent;
class UMaterialInstanceDynamic;
class AEarth4DSite;

namespace Earth4DMaterial
{
	/** The named-parameter contract a master material must expose to render the 4D effects. */
	namespace Params
	{
		EARTH4DRUNTIME_API extern const FName Opacity;       // Scalar 0..1
		EARTH4DRUNTIME_API extern const FName ClipEnable;    // Scalar 0/1
		EARTH4DRUNTIME_API extern const FName ClipNormal;    // Vector (world unit normal)
		EARTH4DRUNTIME_API extern const FName ClipConstant;  // Scalar (world cm)
		EARTH4DRUNTIME_API extern const FName GlowColor;     // Vector (rgb)
		EARTH4DRUNTIME_API extern const FName GlowIntensity; // Scalar (>=0)
		EARTH4DRUNTIME_API extern const FName TintColor;     // Vector (rgb)
		EARTH4DRUNTIME_API extern const FName TintAmount;    // Scalar 0..1
	}

	/** How to handle opacity when the assigned material can't fade. */
	enum class EFadeFallback : uint8
	{
		/** Drive Earth4D_Opacity when the material exposes it; otherwise hide below the cutoff. */
		Auto,
		/** Always drive the material parameter (assumes an Earth4D-aware master material). */
		MaterialParam,
		/** Never touch material opacity; hide the component below the cutoff. */
		VisibilityCutoff
	};

	/** Tunables for one apply pass (constructed by the subsystem each EvaluateAndApply). */
	struct FApplyConfig
	{
		EFadeFallback FadeFallback = EFadeFallback::Auto;
		float VisibilityOpacityCutoff = 0.5f;
		bool bApplyOpacity = true;
		bool bApplyClip = true;
		bool bApplyGlow = true;
		bool bApplyTint = true;
	};

	/**
	 * Ensure every material slot on Comp is a unique Dynamic Material Instance and
	 * collect them (null entries for empty slots). Idempotent: a slot that is
	 * already a MID is reused, so this is cheap to call every tick.
	 */
	EARTH4DRUNTIME_API void EnsureDynamicMaterials(UPrimitiveComponent* Comp, TArray<UMaterialInstanceDynamic*>& OutMids);

	/**
	 * Convert the evaluator's region-local ENU section plane (State.ClipNormal /
	 * State.ClipConstant, metres) into a WORLD-space plane the material can test:
	 * OutNormalWorld (unit) + OutConstantWorld (cm). Uses the bound site for the
	 * ENU->Cesium-world rotation; with no site falls back to the metres*100 path
	 * used everywhere else. Returns false when the state has no clip.
	 */
	EARTH4DRUNTIME_API bool ComputeWorldClip(const FEarth4DObjectState& State, const AEarth4DSite* Site,
		FVector& OutNormalWorld, float& OutConstantWorld);

	/**
	 * Push the evaluated surface state (+ the element's manual edit, for tint) onto
	 * Comp's materials. Pass the already-world-space clip from ComputeWorldClip.
	 * May hide Comp as the opacity fallback (see EFadeFallback).
	 */
	EARTH4DRUNTIME_API void ApplyState(UPrimitiveComponent* Comp, const FEarth4DObjectState& State,
		const FEarth4DObjectEdit& Edit, const FVector& ClipNormalWorld, float ClipConstantWorld,
		const FApplyConfig& Config);

	/** Reset Comp's materials to neutral (opacity 1, clip off, glow off, no tint). */
	EARTH4DRUNTIME_API void ResetState(UPrimitiveComponent* Comp, const FApplyConfig& Config);
}
