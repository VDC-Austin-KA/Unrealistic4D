// Copyright Earth4D. Licensed for project use.
// Datasmith integration: turn a Datasmith-imported scene (Navisworks / Revit /
// SketchUp / etc.) into Earth4D elements WITH their metadata, and auto-wire the
// 4D schedule — including ingesting a Navisworks TimeLiner schedule so model→task
// links come across with only "linking" required, not manual reassignment.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Earth4DTypes.h"
#include "Earth4DDatasmithLibrary.generated.h"

class UEarth4DSubsystem;
class AActor;

UCLASS()
class EARTH4DRUNTIME_API UEarth4DDatasmithLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Register every mesh component under a Datasmith-imported actor as a
	 * schedulable Earth4D element, copying its Datasmith metadata (Revit category,
	 * Navisworks properties, phase, selection-set names, GUID, etc.) onto the
	 * element so it can be searched and auto-assigned. Returns the count added.
	 */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Datasmith")
	static int32 BuildElementsFromActor(UEarth4DSubsystem* Subsystem, AActor* DatasmithRoot, const FString& PathPrefix);

	/**
	 * Group registered elements by an imported metadata key (e.g. "Phase",
	 * "Revit Category", or a Navisworks property) and create one task per group
	 * with the matching elements already assigned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Datasmith")
	static int32 AutoCreateTasksFromMetadata(UEarth4DSubsystem* Subsystem, const FString& MetaKey, EEarth4DTaskType Type, float DaysPerTask);

	/**
	 * Import a Navisworks TimeLiner schedule exported as CSV and rebuild it as
	 * Earth4D tasks, auto-assigning elements whose metadata value (under
	 * MatchMetaKey) matches each task's attachment (selection-set / task name).
	 * Expected columns (case-insensitive, flexible): Task Name, Planned Start,
	 * Planned End, Task Type, Attached (selection set / search name).
	 */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Datasmith")
	static FEarth4DResult ImportTimeLinerCsv(UEarth4DSubsystem* Subsystem, const FString& CsvFilePath, const FString& MatchMetaKey);

	/** List the distinct metadata keys present across registered elements (UI helper). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Datasmith")
	static TArray<FString> ListMetadataKeys(UEarth4DSubsystem* Subsystem);
};
