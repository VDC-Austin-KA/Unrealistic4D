// Copyright Earth4D. Licensed for project use.
#include "Earth4DElementImport.h"
#include "Earth4DSubsystem.h"
#include "Earth4DDatasmithLibrary.h"
#include "GameFramework/Actor.h"

int32 UEarth4DElementImportLibrary::IngestActor(UEarth4DSubsystem* Sub, AActor* Root, bool bRecurseChildActors, const FString& PathPrefix)
{
	if (!Sub || !Root) return 0;

	// Build on the Datasmith path: registers each static-mesh component on this actor
	// as an element AND copies its imported metadata (Revit category/family,
	// Navisworks item properties, phase, selection-set names, GUID, tags, ...).
	const FString Prefix = PathPrefix.IsEmpty() ? Root->GetActorNameOrLabel() : PathPrefix;
	int32 Count = UEarth4DDatasmithLibrary::BuildElementsFromActor(Sub, Root, Prefix);

	if (bRecurseChildActors)
	{
		TArray<AActor*> Children;
		Root->GetAttachedActors(Children);
		for (AActor* Child : Children)
		{
			if (Child && Child != Root)
			{
				Count += IngestActor(Sub, Child, /*bRecurseChildActors=*/true, Prefix + TEXT("/") + Child->GetActorNameOrLabel());
			}
		}
	}
	return Count;
}

int32 UEarth4DElementImportLibrary::IngestActors(UEarth4DSubsystem* Sub, const TArray<AActor*>& Roots, bool bRecurseChildActors)
{
	int32 Count = 0;
	for (AActor* Root : Roots)
	{
		Count += IngestActor(Sub, Root, bRecurseChildActors, FString());
	}
	return Count;
}
