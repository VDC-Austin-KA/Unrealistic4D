// Copyright Earth4D. Licensed for project use.
#pragma once

#include "CoreMinimal.h"
#include "Earth4DTypes.h"
#include "Earth4DSchedule.generated.h"

/**
 * The serializable 4D project document: stages, tasks, the element registry, and
 * playback state. Held by UEarth4DSubsystem; can be saved as an asset / SaveGame.
 * A "scenario" is just a duplicate of this object with a different name.
 */
UCLASS(BlueprintType)
class EARTH4DRUNTIME_API UEarth4DSchedule : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D") FString ProjectName = TEXT("Untitled");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D") FString ProjectStartIso;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D") TArray<FEarth4DStage> Stages;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D") TArray<FEarth4DTask> Tasks;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D") TArray<FEarth4DElement> Elements;

	// ---- lookup helpers ----
	FEarth4DTask* FindTask(const FString& Id);
	FEarth4DStage* FindStage(const FString& Id);
	FEarth4DElement* FindElement(const FString& Id);
	const FEarth4DElement* FindElement(const FString& Id) const;

	/** [min,max] day range spanned by all tasks (stagger-aware on the end). */
	void GetBounds(float& OutMin, float& OutMax) const;

	/** Snapshot/restore the schedule data (for save/load + scenarios). */
	FEarth4DScheduleData CaptureData() const;
	void RestoreData(const FEarth4DScheduleData& Data);
};
