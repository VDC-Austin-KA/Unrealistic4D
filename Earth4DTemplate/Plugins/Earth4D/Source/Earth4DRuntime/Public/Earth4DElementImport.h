// Copyright Earth4D. Licensed for project use.
// Phase 3: import -> element registry. A one-call "ingest this actor/subtree into
// the schedule" entry that builds on UEarth4DDatasmithLibrary (so imported
// Datasmith/Navisworks/Revit metadata is carried onto each element) while also
// walking attached child-actor hierarchies that a single Datasmith import can
// produce. The subsystem exposes verbs over this (IngestActor / IngestActorByName
// / IngestSelectedActors) which are in turn surfaced as chat/MCP tools.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Earth4DElementImport.generated.h"

class UEarth4DSubsystem;
class AActor;

UCLASS()
class EARTH4DRUNTIME_API UEarth4DElementImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Register every mesh under Root as a schedulable Earth4D element (region-local
	 * ENU via the bound site), copying imported metadata. When bRecurseChildActors
	 * is true, also descends into attached child actors (Datasmith hierarchies).
	 * Components already registered are skipped, so re-ingesting is safe.
	 * @return number of NEW elements registered.
	 */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Import")
	static int32 IngestActor(UEarth4DSubsystem* Subsystem, AActor* Root, bool bRecurseChildActors = true, const FString& PathPrefix = TEXT(""));

	/** Ingest a set of root actors (e.g. an editor selection). @return total new elements. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Import")
	static int32 IngestActors(UEarth4DSubsystem* Subsystem, const TArray<AActor*>& Roots, bool bRecurseChildActors = true);
};
