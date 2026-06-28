// Copyright Earth4D. Licensed for project use.
#include "Earth4DDatasmithLibrary.h"
#include "Earth4DSubsystem.h"
#include "Earth4DSchedule.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DatasmithAssetUserData.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"

namespace
{
	/** Best-effort read of Datasmith metadata attached to a component / its owner. */
	void CopyDatasmithMetadata(USceneComponent* Comp, TMap<FString, FString>& OutMeta)
	{
		auto Harvest = [&OutMeta](UActorComponent* C)
		{
			if (!C) return;
			if (const UDatasmithAssetUserData* UD = Cast<UDatasmithAssetUserData>(C->GetAssetUserDataOfClass(UDatasmithAssetUserData::StaticClass())))
			{
				for (const TPair<FName, FString>& KV : UD->MetaData)
				{
					OutMeta.Add(KV.Key.ToString(), KV.Value);
				}
			}
		};
		Harvest(Comp);
		if (AActor* Owner = Comp->GetOwner())
		{
			Harvest(Owner->GetRootComponent());
			for (const FName& Tag : Owner->Tags) OutMeta.Add(TEXT("Tag:") + Tag.ToString(), TEXT("true"));
		}
		// NOTE: Datasmith also stores element metadata in a per-object UDatasmithMetaData
		// table accessible via the Datasmith scene at import time. For the richest
		// data, capture metadata during the import callback and pass it here.
	}

	/** Parse a CSV line honouring simple double-quote quoting. */
	void SplitCsvLine(const FString& Line, TArray<FString>& Out)
	{
		Out.Reset();
		FString Cur; bool bQuoted = false;
		for (int32 i = 0; i < Line.Len(); ++i)
		{
			const TCHAR Ch = Line[i];
			if (Ch == '"') { bQuoted = !bQuoted; }
			else if (Ch == ',' && !bQuoted) { Out.Add(Cur.TrimStartAndEnd()); Cur.Reset(); }
			else { Cur.AppendChar(Ch); }
		}
		Out.Add(Cur.TrimStartAndEnd());
	}

	int32 FindCol(const TArray<FString>& Header, const TArray<const TCHAR*>& Names)
	{
		for (const TCHAR* N : Names)
			for (int32 i = 0; i < Header.Num(); ++i)
				if (Header[i].Equals(N, ESearchCase::IgnoreCase) || Header[i].Contains(N))
					return i;
		return INDEX_NONE;
	}
}

int32 UEarth4DDatasmithLibrary::BuildElementsFromActor(UEarth4DSubsystem* Sub, AActor* Root, const FString& PathPrefix)
{
	if (!Sub || !Root) return 0;
	int32 Count = 0;
	TArray<USceneComponent*> Comps;
	Root->GetComponents<USceneComponent>(Comps);
	for (USceneComponent* Comp : Comps)
	{
		// Only register renderable mesh leaves as elements.
		if (!Comp->IsA<UStaticMeshComponent>()) continue;
		const FString Name = Comp->GetName();
		const FString Path = PathPrefix.IsEmpty() ? Name : (PathPrefix + TEXT("/") + Name);
		const FString Id = Sub->RegisterElement(Comp, Name, Path);
		if (Id.IsEmpty()) continue;
		if (FEarth4DElement* El = Sub->Schedule ? Sub->Schedule->FindElement(Id) : nullptr)
		{
			CopyDatasmithMetadata(Comp, El->Meta);
			if (const FString* Disp = El->Meta.Find(TEXT("Element"))) El->DisplayName = *Disp; // Revit family/type if present
		}
		Count++;
	}
	return Count;
}

int32 UEarth4DDatasmithLibrary::AutoCreateTasksFromMetadata(UEarth4DSubsystem* Sub, const FString& MetaKey, EEarth4DTaskType Type, float DaysPerTask)
{
	if (!Sub || !Sub->Schedule) return 0;
	TMap<FString, TArray<FString>> Groups;
	for (const FEarth4DElement& E : Sub->Schedule->Elements)
		if (const FString* V = E.Meta.Find(MetaKey)) Groups.FindOrAdd(*V).Add(E.Id);

	// Stable ordering so generated phases are deterministic.
	TArray<FString> Keys; Groups.GetKeys(Keys); Keys.Sort();
	float Cursor = 0.f; int32 Made = 0;
	const float Days = FMath::Max(1.f, DaysPerTask);
	for (const FString& K : Keys)
	{
		FString Id; Sub->AddTask(K, Type, Cursor, Days, Id);
		Sub->AssignElementsToTask(Id, Groups[K], false);
		Cursor += Days; Made++;
	}
	return Made;
}

FEarth4DResult UEarth4DDatasmithLibrary::ImportTimeLinerCsv(UEarth4DSubsystem* Sub, const FString& CsvFilePath, const FString& MatchMetaKey)
{
	if (!Sub || !Sub->Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *CsvFilePath) || Lines.Num() < 2)
		return FEarth4DResult::Fail(TEXT("Could not read CSV (need a header + rows)"));

	TArray<FString> Header; SplitCsvLine(Lines[0], Header);
	const int32 ColName = FindCol(Header, { TEXT("Task Name"), TEXT("Name") });
	const int32 ColStart = FindCol(Header, { TEXT("Planned Start"), TEXT("Start") });
	const int32 ColEnd = FindCol(Header, { TEXT("Planned End"), TEXT("End"), TEXT("Finish") });
	const int32 ColType = FindCol(Header, { TEXT("Task Type"), TEXT("Type") });
	const int32 ColAttach = FindCol(Header, { TEXT("Attached"), TEXT("Selection"), TEXT("Search") });
	if (ColName == INDEX_NONE) return FEarth4DResult::Fail(TEXT("CSV missing a Task Name column"));

	// First pass: parse rows + find the earliest date so day indices start at 0.
	struct FRow { FString Name; FDateTime Start; FDateTime End; FString TypeStr; FString Attach; bool bHasDates; };
	TArray<FRow> Rows;
	FDateTime MinDate = FDateTime::MaxValue();
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		if (Lines[i].TrimStartAndEnd().IsEmpty()) continue;
		TArray<FString> C; SplitCsvLine(Lines[i], C);
		auto Get = [&C](int32 Idx) { return (Idx != INDEX_NONE && C.IsValidIndex(Idx)) ? C[Idx] : FString(); };
		FRow R;
		R.Name = Get(ColName);
		R.TypeStr = Get(ColType);
		R.Attach = Get(ColAttach);
		R.bHasDates = FDateTime::Parse(Get(ColStart), R.Start) && FDateTime::Parse(Get(ColEnd), R.End);
		if (R.bHasDates) MinDate = FMath::Min(MinDate, R.Start);
		Rows.Add(R);
	}

	// Rebuild schedule tasks from the TimeLiner rows.
	int32 Made = 0, Linked = 0;
	float Fallback = 0.f;
	for (const FRow& R : Rows)
	{
		const bool bDemo = R.TypeStr.Contains(TEXT("Demo")) || R.TypeStr.Contains(TEXT("Remove"));
		float Start, Dur;
		if (R.bHasDates && MinDate != FDateTime::MaxValue())
		{
			Start = (float)(R.Start - MinDate).GetTotalDays();
			Dur = FMath::Max(1.f, (float)(R.End - R.Start).GetTotalDays());
		}
		else { Start = Fallback; Dur = 5.f; Fallback += 5.f; }

		FString TaskId;
		Sub->AddTask(R.Name, bDemo ? EEarth4DTaskType::Demolition : EEarth4DTaskType::Construction, Start, Dur, TaskId);
		Made++;

		// Link elements: match the task's attachment (or name) against MatchMetaKey,
		// falling back to the task name appearing in any metadata value.
		const FString Needle = (R.Attach.IsEmpty() ? R.Name : R.Attach);
		TArray<FString> Ids;
		for (const FEarth4DElement& E : Sub->Schedule->Elements)
		{
			const FString* V = MatchMetaKey.IsEmpty() ? nullptr : E.Meta.Find(MatchMetaKey);
			bool bMatch = V && (V->Equals(Needle, ESearchCase::IgnoreCase) || V->Contains(Needle));
			if (!bMatch)
				for (const TPair<FString, FString>& KV : E.Meta)
					if (KV.Value.Equals(Needle, ESearchCase::IgnoreCase)) { bMatch = true; break; }
			if (bMatch) Ids.Add(E.Id);
		}
		if (Ids.Num() > 0) { Sub->AssignElementsToTask(TaskId, Ids, false); Linked += Ids.Num(); }
	}

	return FEarth4DResult::Ok(FString::Printf(TEXT("Imported %d TimeLiner tasks, linked %d elements."), Made, Linked));
}

TArray<FString> UEarth4DDatasmithLibrary::ListMetadataKeys(UEarth4DSubsystem* Sub)
{
	TSet<FString> Keys;
	if (Sub && Sub->Schedule)
		for (const FEarth4DElement& E : Sub->Schedule->Elements)
			for (const TPair<FString, FString>& KV : E.Meta) Keys.Add(KV.Key);
	return Keys.Array();
}
