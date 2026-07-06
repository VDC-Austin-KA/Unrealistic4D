// Copyright Earth4D. Licensed for project use.
#include "Earth4DMaterialApplier.h"
#include "Earth4DSite.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialTypes.h"   // FMaterialParameterInfo / FHashedMaterialParameterInfo

namespace Earth4DMaterial
{
	namespace Params
	{
		const FName Opacity(TEXT("Earth4D_Opacity"));
		const FName ClipEnable(TEXT("Earth4D_ClipEnable"));
		const FName ClipNormal(TEXT("Earth4D_ClipNormal"));
		const FName ClipConstant(TEXT("Earth4D_ClipConstant"));
		const FName GlowColor(TEXT("Earth4D_GlowColor"));
		const FName GlowIntensity(TEXT("Earth4D_GlowIntensity"));
		const FName TintColor(TEXT("Earth4D_TintColor"));
		const FName TintAmount(TEXT("Earth4D_TintAmount"));
	}

	namespace
	{
		/** True if the material exposes a scalar parameter by this name. */
		bool HasScalarParam(UMaterialInstanceDynamic* Mid, const FName& Name)
		{
			if (!Mid) return false;
			float Tmp = 0.f;
			// VERIFY: FMaterialParameterInfo -> FHashedMaterialParameterInfo implicit conversion
			// is the stable 5.x signature for UMaterialInterface::GetScalarParameterValue.
			return Mid->GetScalarParameterValue(FMaterialParameterInfo(Name), Tmp);
		}
	}

	void EnsureDynamicMaterials(UPrimitiveComponent* Comp, TArray<UMaterialInstanceDynamic*>& OutMids)
	{
		OutMids.Reset();
		if (!Comp) return;
		const int32 Num = Comp->GetNumMaterials();
		OutMids.Reserve(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			UMaterialInterface* Cur = Comp->GetMaterial(i);
			UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Cur);
			if (!Mid && Cur)
			{
				// CreateDynamicMaterialInstance returns the existing MID if the slot is
				// already dynamic, so repeated calls don't churn allocations.
				Mid = Comp->CreateDynamicMaterialInstance(i, Cur);
			}
			OutMids.Add(Mid);
		}
	}

	bool ComputeWorldClip(const FEarth4DObjectState& State, const AEarth4DSite* Site,
		FVector& OutNormalWorld, float& OutConstantWorld)
	{
		if (!State.bHasClip)
		{
			OutNormalWorld = FVector::UpVector;
			OutConstantWorld = 0.f;
			return false;
		}

		const FVector NEnu = State.ClipNormal.GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector);
		const float C = State.ClipConstant;
		// A point on the plane in region-local ENU metres: dot(P0, N) + C == 0  =>  P0 = -C * N.
		const FVector P0Enu = -C * NEnu;

		if (Site)
		{
			// Rotate the normal (direction) and place the on-plane point (position) into world cm.
			OutNormalWorld = Site->RegionLocalEnuOffsetToUnreal(NEnu).GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector);
			const FVector P0World = Site->RegionLocalEnuToUnreal(P0Enu);
			OutConstantWorld = (float)FVector::DotProduct(P0World, OutNormalWorld);
		}
		else
		{
			// No site: the rest of the pipeline treats ENU metres as world cm * 100.
			OutNormalWorld = NEnu;                 // direction unchanged by uniform scale
			OutConstantWorld = -C * 100.f;          // dot(P0World, N) with P0World = -C*N*100, |N|=1
		}
		return true;
	}

	void ApplyState(UPrimitiveComponent* Comp, const FEarth4DObjectState& State,
		const FEarth4DObjectEdit& Edit, const FVector& ClipNormalWorld, float ClipConstantWorld,
		const FApplyConfig& Config)
	{
		if (!Comp) return;
		TArray<UMaterialInstanceDynamic*> Mids;
		EnsureDynamicMaterials(Comp, Mids);

		const float OpacityVal = FMath::Clamp(State.Opacity, 0.f, 1.f);
		const float ClipEnableVal = State.bHasClip ? 1.f : 0.f;
		const float GlowVal = State.bHasGlow ? FMath::Max(0.f, State.GlowIntensity) : 0.f;
		const float TintVal = Edit.bHasColor ? 1.f : 0.f;
		const FLinearColor ClipNormalColor(ClipNormalWorld.X, ClipNormalWorld.Y, ClipNormalWorld.Z, 0.f);

		bool bAnyOpacityDriven = false;

		for (UMaterialInstanceDynamic* Mid : Mids)
		{
			if (!Mid) continue;

			if (Config.bApplyOpacity)
			{
				const bool bDrive = (Config.FadeFallback == EFadeFallback::MaterialParam)
					|| (Config.FadeFallback == EFadeFallback::Auto && HasScalarParam(Mid, Params::Opacity));
				if (bDrive)
				{
					Mid->SetScalarParameterValue(Params::Opacity, OpacityVal);
					bAnyOpacityDriven = true;
				}
			}

			if (Config.bApplyClip)
			{
				Mid->SetScalarParameterValue(Params::ClipEnable, ClipEnableVal);
				if (State.bHasClip)
				{
					Mid->SetVectorParameterValue(Params::ClipNormal, ClipNormalColor);
					Mid->SetScalarParameterValue(Params::ClipConstant, ClipConstantWorld);
				}
			}

			if (Config.bApplyGlow)
			{
				Mid->SetScalarParameterValue(Params::GlowIntensity, GlowVal);
				if (GlowVal > 0.f)
				{
					Mid->SetVectorParameterValue(Params::GlowColor, State.GlowColor);
				}
			}

			if (Config.bApplyTint)
			{
				Mid->SetScalarParameterValue(Params::TintAmount, TintVal);
				if (Edit.bHasColor)
				{
					Mid->SetVectorParameterValue(Params::TintColor, Edit.Color);
				}
			}
		}

		// Opacity fallback: no material could fade and the surface should be (near)
		// invisible -> hide the whole component so appear/disappear still reads.
		if (Config.bApplyOpacity
			&& Config.FadeFallback != EFadeFallback::MaterialParam
			&& !bAnyOpacityDriven
			&& OpacityVal < Config.VisibilityOpacityCutoff)
		{
			Comp->SetVisibility(false, true);
		}
	}

	void ResetState(UPrimitiveComponent* Comp, const FApplyConfig& Config)
	{
		if (!Comp) return;
		TArray<UMaterialInstanceDynamic*> Mids;
		EnsureDynamicMaterials(Comp, Mids);
		for (UMaterialInstanceDynamic* Mid : Mids)
		{
			if (!Mid) continue;
			if (Config.bApplyOpacity) Mid->SetScalarParameterValue(Params::Opacity, 1.f);
			if (Config.bApplyClip)    Mid->SetScalarParameterValue(Params::ClipEnable, 0.f);
			if (Config.bApplyGlow)    Mid->SetScalarParameterValue(Params::GlowIntensity, 0.f);
			if (Config.bApplyTint)    Mid->SetScalarParameterValue(Params::TintAmount, 0.f);
		}
	}
}
