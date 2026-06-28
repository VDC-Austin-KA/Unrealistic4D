// Copyright Earth4D. Licensed for project use.
// Data model for the 4D construction schedule. Mirrors the web app's store.ts /
// fourd/types.ts so the two stay conceptually in lockstep.
#pragma once

#include "CoreMinimal.h"
#include "Earth4DTypes.generated.h"

UENUM(BlueprintType)
enum class EEarth4DTaskType : uint8
{
	Construction,
	Demolition,
	Temporary
};

UENUM(BlueprintType)
enum class EEarth4DAnimStyle : uint8
{
	Fade,
	Drop,
	Rise,
	Slide,
	Grow,
	GrowUp,
	Spiral,
	Swoop,
	Assemble,
	Wipe		// section-plane reveal (Fuzor / CMBuilder style)
};

UENUM(BlueprintType)
enum class EEarth4DDirection : uint8
{
	North,
	South,
	East,
	West,
	Above,
	Below
};

UENUM(BlueprintType)
enum class EEarth4DSequence : uint8
{
	ByElevation,
	ByName,
	Together,
	Random
};

/** Per-element manual overrides layered on top of its authored transform/material. */
USTRUCT(BlueprintType)
struct FEarth4DObjectEdit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHidden = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Opacity = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasColor = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor::White;
	/** Region-local ENU metres (east, north, up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Offset = FVector::ZeroVector;
	/** Yaw about up, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RotationDeg = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Scale = 1.f;
	/** -1 = unset. Day the element appears / disappears (gates the schedule). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AppearDay = -1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DisappearDay = -1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StaggerDelay = 0.f;
	/** Shared pivot (region-local ENU) for group transforms; bUsePivot toggles it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUsePivot = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Pivot = FVector::ZeroVector;
};

/** A scheduled task (mirrors fourd Task). Times are inclusive day indices. */
USTRUCT(BlueprintType)
struct FEarth4DTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEarth4DTaskType Type = EEarth4DTaskType::Construction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Start = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float End = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEarth4DAnimStyle Style = EEarth4DAnimStyle::Rise;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEarth4DDirection Direction = EEarth4DDirection::Above;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EEarth4DSequence Sequence = EEarth4DSequence::ByElevation;
	/** 0 = strictly sequential sub-windows, 1 = all elements move together. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Overlap = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Distance = 60.f;
	/** Fuzor-style: extra days between consecutive elements' starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Stagger = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString StageId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> ObjectIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor(0.2f, 0.55f, 0.9f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasGlowColor = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor GlowColor = FLinearColor::Green;
};

/** A stage / phase grouping tasks (timeline layer). */
USTRUCT(BlueprintType)
struct FEarth4DStage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor::Gray;
};

/**
 * A scheduled scene element. Maps a stable id (so tasks/edits survive reload) to a
 * scene component, plus its base transform and bounds (for sequencing/animation).
 */
USTRUCT(BlueprintType)
struct FEarth4DElement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Path;        // tree path / source hierarchy
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FString, FString> Meta;
	/** The component this element drives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USceneComponent> Component;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform BaseTransform;
	/** Region-local ENU bounds centre + size (metres), used by sequencing/styles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector BoundsCenter = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector BoundsSize = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FEarth4DObjectEdit Edit;
};

/** Evaluator output for one element at one day (mirrors fourd ObjectState). */
USTRUCT(BlueprintType)
struct FEarth4DObjectState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bVisible = true;
	UPROPERTY(BlueprintReadOnly) FVector Offset = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector Scale = FVector::OneVector;
	UPROPERTY(BlueprintReadOnly) float Spin = 0.f;
	UPROPERTY(BlueprintReadOnly) float Opacity = 1.f;
	/** Section-reveal plane (region-local): normal + constant; bHasClip toggles. */
	UPROPERTY(BlueprintReadOnly) bool bHasClip = false;
	UPROPERTY(BlueprintReadOnly) FVector ClipNormal = FVector::UpVector;
	UPROPERTY(BlueprintReadOnly) float ClipConstant = 0.f;
	/** Optional action glow (emissive) while animating. */
	UPROPERTY(BlueprintReadOnly) bool bHasGlow = false;
	UPROPERTY(BlueprintReadOnly) FLinearColor GlowColor = FLinearColor::Black;
	UPROPERTY(BlueprintReadOnly) float GlowIntensity = 0.f;
};

/** Flat, serializable snapshot of a schedule (save/load + named scenarios). */
USTRUCT(BlueprintType)
struct FEarth4DScheduleData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ProjectName = TEXT("Untitled");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ProjectStartIso;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FEarth4DStage> Stages;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FEarth4DTask> Tasks;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FEarth4DElement> Elements;
};

/** Small result returned by command-API verbs so chat/MCP can report back. */
USTRUCT(BlueprintType)
struct FEarth4DResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bOk = true;
	UPROPERTY(BlueprintReadOnly) FString Message;

	static FEarth4DResult Ok(const FString& Msg = TEXT("ok")) { return { true, Msg }; }
	static FEarth4DResult Fail(const FString& Msg) { return { false, Msg }; }
};
