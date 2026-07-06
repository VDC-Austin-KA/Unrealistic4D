// Copyright Earth4D. Licensed for project use.
#include "Earth4DSubsystem.h"
#include "Earth4DSchedule.h"
#include "Earth4DScheduleEvaluator.h"
#include "Earth4DSite.h"
#include "Earth4DMaterialApplier.h"
#include "Earth4DElementImport.h"
#include "Earth4DDayDriven.h"
#include "Earth4DAnnotation.h"
#include "Earth4DExcavation.h"
#include "Earth4DVehicle.h"
#include "Earth4DCameraDirector.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"               // TActorIterator
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"  // UrlEncode
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_EDITOR
#include "Editor.h"                    // GEditor
#include "Selection.h"                 // USelection (IngestSelectedActors)
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Exporters/Exporter.h"        // UExporter::RunAssetExportTask (glTF export)
#include "AssetExportTask.h"
#endif

void UEarth4DSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GetOrCreateSchedule();
	// Late-bind any site already in the level (also called explicitly by AEarth4DSite).
	FindAndBindSite();
}

// ---- Site / georeference ----
void UEarth4DSubsystem::SetSite(AEarth4DSite* InSite)
{
	ActiveSite = InSite;
	EvaluateAndApply(CurrentDay);
	NotifyChanged();
}

AEarth4DSite* UEarth4DSubsystem::FindAndBindSite()
{
	if (ActiveSite) return ActiveSite;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AEarth4DSite> It(World); It; ++It) { SetSite(*It); break; }
	}
	return ActiveSite;
}

FString UEarth4DSubsystem::NewId(const TCHAR* Prefix)
{
	return FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Short));
}

UEarth4DSchedule* UEarth4DSubsystem::GetOrCreateSchedule()
{
	if (!Schedule)
	{
		Schedule = NewObject<UEarth4DSchedule>(this, TEXT("Earth4DSchedule"));
	}
	return Schedule;
}

void UEarth4DSubsystem::SetSchedule(UEarth4DSchedule* InSchedule)
{
	Schedule = InSchedule;
	EvaluateAndApply(CurrentDay);
	NotifyChanged();
}

void UEarth4DSubsystem::Tick(float DeltaTime)
{
	if (bPlaying && Schedule)
	{
		float Min, Max; Schedule->GetBounds(Min, Max);
		CurrentDay += DeltaTime * DaysPerSecond;
		if (CurrentDay >= Max) { CurrentDay = Max; bPlaying = false; }
		EvaluateAndApply(CurrentDay);
	}
}

// ---- Playback ----
void UEarth4DSubsystem::SetCurrentDay(float Day) { CurrentDay = Day; EvaluateAndApply(Day); }
void UEarth4DSubsystem::Play() { bPlaying = true; }
void UEarth4DSubsystem::Pause() { bPlaying = false; }
void UEarth4DSubsystem::SetSpeed(float DaysPerSec) { DaysPerSecond = FMath::Max(0.01f, DaysPerSec); }

// ---- Tasks ----
FEarth4DResult UEarth4DSubsystem::AddTask(const FString& Name, EEarth4DTaskType Type, float Start, float Duration, FString& OutTaskId)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	FEarth4DTask T;
	T.Id = NewId(TEXT("task"));
	T.Name = Name.IsEmpty() ? FString::Printf(TEXT("Task %d"), Schedule->Tasks.Num() + 1) : Name;
	T.Type = Type;
	T.Start = Start;
	T.End = Start + FMath::Max(1.f, Duration);
	T.Style = (Type == EEarth4DTaskType::Demolition) ? EEarth4DAnimStyle::Drop : EEarth4DAnimStyle::Rise;
	Schedule->Tasks.Add(T);
	OutTaskId = T.Id;
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok(FString::Printf(TEXT("Added task '%s' (%s)"), *T.Name, *T.Id));
}

FEarth4DResult UEarth4DSubsystem::RemoveTask(const FString& TaskId)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	const int32 Removed = Schedule->Tasks.RemoveAll([&](const FEarth4DTask& T) { return T.Id == TaskId; });
	if (Removed == 0) return FEarth4DResult::Fail(TEXT("Task not found"));
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok(TEXT("Task removed"));
}

#define E4D_TASK(TaskId) FEarth4DTask* T = Schedule ? Schedule->FindTask(TaskId) : nullptr; if (!T) return FEarth4DResult::Fail(TEXT("Task not found"));

FEarth4DResult UEarth4DSubsystem::RenameTask(const FString& TaskId, const FString& NewName) { E4D_TASK(TaskId); T->Name = NewName; NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskDuration(const FString& TaskId, float Days) { E4D_TASK(TaskId); T->End = T->Start + FMath::Max(1.f, Days); EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(FString::Printf(TEXT("'%s' now %.0f days"), *T->Name, Days)); }
FEarth4DResult UEarth4DSubsystem::SetTaskWindow(const FString& TaskId, float Start, float End) { E4D_TASK(TaskId); T->Start = Start; T->End = FMath::Max(Start + 1.f, End); EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskType(const FString& TaskId, EEarth4DTaskType Type) { E4D_TASK(TaskId); T->Type = Type; EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskStyle(const FString& TaskId, EEarth4DAnimStyle Style) { E4D_TASK(TaskId); T->Style = Style; EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskDirection(const FString& TaskId, EEarth4DDirection Direction) { E4D_TASK(TaskId); T->Direction = Direction; EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskStagger(const FString& TaskId, float Stagger) { E4D_TASK(TaskId); T->Stagger = FMath::Max(0.f, Stagger); EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskStage(const FString& TaskId, const FString& StageId) { E4D_TASK(TaskId); T->StageId = StageId; NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskSequence(const FString& TaskId, EEarth4DSequence Sequence) { E4D_TASK(TaskId); T->Sequence = Sequence; EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskDistance(const FString& TaskId, float Distance) { E4D_TASK(TaskId); T->Distance = FMath::Max(0.f, Distance); EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskOverlap(const FString& TaskId, float Overlap) { E4D_TASK(TaskId); T->Overlap = FMath::Clamp(Overlap, 0.f, 1.f); EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskColor(const FString& TaskId, FLinearColor Color) { E4D_TASK(TaskId); T->Color = Color; EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(); }
FEarth4DResult UEarth4DSubsystem::SetTaskGlowColor(const FString& TaskId, FLinearColor Color, bool bEnable) { E4D_TASK(TaskId); T->bHasGlowColor = bEnable; T->GlowColor = Color; EvaluateAndApply(CurrentDay); NotifyChanged(); return FEarth4DResult::Ok(); }

FEarth4DResult UEarth4DSubsystem::AlignTask(const FString& TaskId, const FString& RelativeToTaskId, const FString& Mode, float Gap)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	FEarth4DTask* T = Schedule->FindTask(TaskId);
	FEarth4DTask* Ref = Schedule->FindTask(RelativeToTaskId);
	if (!T || !Ref) return FEarth4DResult::Fail(TEXT("Both task and relative-to task must exist"));
	const float Dur = FMath::Max(T->End - T->Start, 0.f);
	float Start = T->Start;
	if (Mode == TEXT("after")) Start = Ref->End + Gap;
	else if (Mode == TEXT("before")) Start = Ref->Start - Gap - Dur;
	else if (Mode == TEXT("start-with")) Start = Ref->Start + Gap;
	else if (Mode == TEXT("end-with")) Start = Ref->End - Dur + Gap;
	else return FEarth4DResult::Fail(TEXT("Mode must be after|before|start-with|end-with"));
	T->Start = Start; T->End = Start + Dur;
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok(FString::Printf(TEXT("'%s' aligned %s '%s' (start %.1f)"), *T->Name, *Mode, *Ref->Name, Start));
}

FEarth4DResult UEarth4DSubsystem::SequenceTasks(const TArray<FString>& TaskIds, float Start, float Gap)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	if (TaskIds.Num() < 2) return FEarth4DResult::Fail(TEXT("Provide at least two task ids in order"));
	float Cursor = Start;
	int32 N = 0;
	for (const FString& Id : TaskIds)
	{
		FEarth4DTask* T = Schedule->FindTask(Id);
		if (!T) continue;
		const float Dur = FMath::Max(T->End - T->Start, 1.f);
		T->Start = Cursor; T->End = Cursor + Dur;
		Cursor += Dur + Gap; ++N;
	}
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok(FString::Printf(TEXT("Sequenced %d tasks from day %.1f"), N, Start));
}

FEarth4DResult UEarth4DSubsystem::SetProjectStart(const FString& IsoDate)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	Schedule->ProjectStartIso = IsoDate;
	NotifyChanged();
	return FEarth4DResult::Ok(FString::Printf(TEXT("Project start set to %s"), *IsoDate));
}

// ---- Assignment ----
FEarth4DResult UEarth4DSubsystem::AssignElementsToTask(const FString& TaskId, const TArray<FString>& ElementIds, bool bAdd)
{
	E4D_TASK(TaskId);
	if (bAdd)
	{
		for (const FString& Id : ElementIds) T->ObjectIds.AddUnique(Id);
	}
	else
	{
		T->ObjectIds = ElementIds;
	}
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok(FString::Printf(TEXT("'%s' now has %d elements"), *T->Name, T->ObjectIds.Num()));
}

FEarth4DResult UEarth4DSubsystem::RemoveElementsFromTask(const FString& TaskId, const TArray<FString>& ElementIds)
{
	E4D_TASK(TaskId);
	for (const FString& Id : ElementIds) T->ObjectIds.Remove(Id);
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok();
}

// ---- Elements ----
FEarth4DResult UEarth4DSubsystem::SetElementEdit(const FString& ElementId, const FEarth4DObjectEdit& Edit)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	FEarth4DElement* El = Schedule->FindElement(ElementId);
	if (!El) return FEarth4DResult::Fail(TEXT("Element not found"));
	El->Edit = Edit;
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok();
}

FEarth4DResult UEarth4DSubsystem::ResetElementEdit(const FString& ElementId)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	FEarth4DElement* El = Schedule->FindElement(ElementId);
	if (!El) return FEarth4DResult::Fail(TEXT("Element not found"));
	El->Edit = FEarth4DObjectEdit();
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok();
}

FEarth4DResult UEarth4DSubsystem::SetElementColor(const FString& ElementId, FLinearColor Color, bool bEnable)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	FEarth4DElement* El = Schedule->FindElement(ElementId);
	if (!El) return FEarth4DResult::Fail(TEXT("Element not found"));
	El->Edit.bHasColor = bEnable;
	El->Edit.Color = Color;
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok();
}

// ---- Selection ----
FEarth4DResult UEarth4DSubsystem::SelectElements(const TArray<FString>& ElementIds, bool bAdd)
{
	if (!bAdd) SelectedElementIds.Reset();
	for (const FString& Id : ElementIds) SelectedElementIds.AddUnique(Id);
	NotifyChanged();
	return FEarth4DResult::Ok(FString::Printf(TEXT("%d element(s) selected"), SelectedElementIds.Num()));
}

FEarth4DResult UEarth4DSubsystem::SelectByName(const FString& Query, bool bAdd)
{
	return SelectElements(FindElements(Query), bAdd);
}

void UEarth4DSubsystem::ClearSelection()
{
	SelectedElementIds.Reset();
	NotifyChanged();
}

// ---- Selection sets ----
FEarth4DResult UEarth4DSubsystem::SaveSelectionSet(const FString& Name, const TArray<FString>& ElementIds, FString& OutSetId)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	const TArray<FString>& Ids = ElementIds.Num() ? ElementIds : SelectedElementIds;
	if (Ids.Num() == 0) return FEarth4DResult::Fail(TEXT("Nothing to save (pass ids or select some first)"));
	// Overwrite an existing set with the same name, else append.
	FEarth4DSelectionSet* Set = Schedule->SelectionSets.FindByPredicate([&](const FEarth4DSelectionSet& S){ return S.Name.Equals(Name, ESearchCase::IgnoreCase); });
	if (!Set) { FEarth4DSelectionSet New; New.Id = NewId(TEXT("set")); New.Name = Name; Set = &Schedule->SelectionSets.Add_GetRef(New); }
	Set->ObjectIds = Ids;
	OutSetId = Set->Id;
	NotifyChanged();
	return FEarth4DResult::Ok(FString::Printf(TEXT("Saved set '%s' (%d elements)"), *Name, Ids.Num()));
}

FEarth4DResult UEarth4DSubsystem::ApplySelectionSet(const FString& IdOrName)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	const FEarth4DSelectionSet* Set = Schedule->SelectionSets.FindByPredicate([&](const FEarth4DSelectionSet& S){ return S.Id == IdOrName || S.Name.Equals(IdOrName, ESearchCase::IgnoreCase); });
	if (!Set) return FEarth4DResult::Fail(TEXT("Selection set not found"));
	return SelectElements(Set->ObjectIds, /*bAdd=*/false);
}

FEarth4DResult UEarth4DSubsystem::DeleteSelectionSet(const FString& IdOrName)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	const int32 Removed = Schedule->SelectionSets.RemoveAll([&](const FEarth4DSelectionSet& S){ return S.Id == IdOrName || S.Name.Equals(IdOrName, ESearchCase::IgnoreCase); });
	if (Removed == 0) return FEarth4DResult::Fail(TEXT("Selection set not found"));
	NotifyChanged();
	return FEarth4DResult::Ok(TEXT("Selection set removed"));
}

TArray<FString> UEarth4DSubsystem::ListSelectionSetNames() const
{
	TArray<FString> Out;
	if (Schedule) for (const FEarth4DSelectionSet& S : Schedule->SelectionSets) Out.Add(FString::Printf(TEXT("%s (%d)"), *S.Name, S.ObjectIds.Num()));
	return Out;
}

FString UEarth4DSubsystem::RegisterElement(USceneComponent* Component, const FString& DisplayName, const FString& Path)
{
	if (!Schedule || !Component) return FString();
	// Idempotent: if this component is already a registered element, reuse its id so
	// re-ingesting an actor/subtree never produces duplicates (preserves task links).
	for (const FEarth4DElement& Existing : Schedule->Elements)
	{
		if (Existing.Component.Get() == Component) return Existing.Id;
	}
	FEarth4DElement El;
	El.Id = NewId(TEXT("el"));
	El.DisplayName = DisplayName;
	El.Path = Path;
	El.Component = Component;
	El.BaseTransform = Component->GetRelativeTransform();
	const FBoxSphereBounds B = Component->Bounds;
	// Store bounds centre in region-local ENU metres when a site is bound (so the
	// evaluator's elevation/name sequencing is in real metres); else raw world cm.
	El.BoundsCenter = ActiveSite ? ActiveSite->UnrealToRegionLocalEnu(B.Origin) : B.Origin;
	El.BoundsSize = B.BoxExtent * 2.f;
	Schedule->Elements.Add(El);
	NotifyChanged();
	return El.Id;
}

// ---- Import → elements ----
FEarth4DResult UEarth4DSubsystem::IngestActor(AActor* Root, bool bRecurse)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	if (!Root) return FEarth4DResult::Fail(TEXT("No actor to ingest"));
	const int32 N = UEarth4DElementImportLibrary::IngestActor(this, Root, bRecurse, FString());
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok(FString::Printf(TEXT("Ingested %d element(s) from '%s'"), N, *Root->GetActorNameOrLabel()));
}

FEarth4DResult UEarth4DSubsystem::IngestActorByName(const FString& ActorName, bool bRecurse)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	UWorld* World = GetWorld();
	if (!World) return FEarth4DResult::Fail(TEXT("No world"));
	const FString Needle = ActorName.TrimStartAndEnd();
	if (Needle.IsEmpty()) return FEarth4DResult::Fail(TEXT("Empty actor name"));

	TArray<AActor*> Matches;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const FString Label = It->GetActorNameOrLabel();
		if (Label.Equals(Needle, ESearchCase::IgnoreCase) || Label.Contains(Needle))
		{
			Matches.Add(*It);
		}
	}
	if (Matches.Num() == 0) return FEarth4DResult::Fail(FString::Printf(TEXT("No actor matching '%s'"), *Needle));

	const int32 N = UEarth4DElementImportLibrary::IngestActors(this, Matches, bRecurse);
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok(FString::Printf(TEXT("Ingested %d element(s) from %d actor(s) matching '%s'"), N, Matches.Num(), *Needle));
}

FEarth4DResult UEarth4DSubsystem::IngestSelectedActors(bool bRecurse)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
#if WITH_EDITOR
	if (GEditor)
	{
		TArray<AActor*> Selected;
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(Selected);
		if (Selected.Num() == 0) return FEarth4DResult::Fail(TEXT("Nothing selected in the editor"));
		const int32 N = UEarth4DElementImportLibrary::IngestActors(this, Selected, bRecurse);
		EvaluateAndApply(CurrentDay); NotifyChanged();
		return FEarth4DResult::Ok(FString::Printf(TEXT("Ingested %d element(s) from %d selected actor(s)"), N, Selected.Num()));
	}
#endif
	return FEarth4DResult::Fail(TEXT("Selection ingest is only available in the editor"));
}

// ---- Stages ----
FEarth4DResult UEarth4DSubsystem::AddStage(const FString& Name, FString& OutStageId)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	FEarth4DStage S; S.Id = NewId(TEXT("stage")); S.Name = Name.IsEmpty() ? TEXT("Stage") : Name;
	Schedule->Stages.Add(S); OutStageId = S.Id; NotifyChanged();
	return FEarth4DResult::Ok();
}

FEarth4DResult UEarth4DSubsystem::RemoveStage(const FString& StageId)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	Schedule->Stages.RemoveAll([&](const FEarth4DStage& S) { return S.Id == StageId; });
	for (FEarth4DTask& T : Schedule->Tasks) if (T.StageId == StageId) T.StageId.Empty();
	NotifyChanged();
	return FEarth4DResult::Ok();
}

FEarth4DResult UEarth4DSubsystem::RenameStage(const FString& StageId, const FString& NewName)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	FEarth4DStage* S = Schedule->FindStage(StageId);
	if (!S) return FEarth4DResult::Fail(TEXT("Stage not found"));
	S->Name = NewName; NotifyChanged();
	return FEarth4DResult::Ok();
}

FEarth4DResult UEarth4DSubsystem::SetStageColor(const FString& StageId, FLinearColor Color)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	FEarth4DStage* S = Schedule->FindStage(StageId);
	if (!S) return FEarth4DResult::Fail(TEXT("Stage not found"));
	S->Color = Color; NotifyChanged();
	return FEarth4DResult::Ok();
}

// ---- Save / load + scenarios ----
namespace
{
	FString Earth4DDir() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Earth4D")); }
	FString DefaultProjectFile() { return FPaths::Combine(Earth4DDir(), TEXT("Project.e4d.json")); }
	FString ScenarioDir() { return FPaths::Combine(Earth4DDir(), TEXT("Scenarios")); }
	FString ScenarioFile(const FString& Name)
	{
		FString Safe = Name; Safe.ReplaceInline(TEXT("/"), TEXT("_")); Safe.ReplaceInline(TEXT("\\"), TEXT("_"));
		return FPaths::Combine(ScenarioDir(), Safe + TEXT(".e4d.json"));
	}
}

FEarth4DResult UEarth4DSubsystem::SaveProject(const FString& FilePath)
{
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	const FString Path = FilePath.IsEmpty() ? DefaultProjectFile() : FilePath;
	const FEarth4DScheduleData Data = Schedule->CaptureData();
	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(Data, Json))
		return FEarth4DResult::Fail(TEXT("Serialize failed"));
	if (!FFileHelper::SaveStringToFile(Json, *Path))
		return FEarth4DResult::Fail(FString::Printf(TEXT("Could not write %s"), *Path));
	return FEarth4DResult::Ok(FString::Printf(TEXT("Saved project to %s"), *Path));
}

FEarth4DResult UEarth4DSubsystem::LoadProject(const FString& FilePath)
{
	const FString Path = FilePath.IsEmpty() ? DefaultProjectFile() : FilePath;
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
		return FEarth4DResult::Fail(FString::Printf(TEXT("Could not read %s"), *Path));
	FEarth4DScheduleData Data;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Data, 0, 0))
		return FEarth4DResult::Fail(TEXT("Parse failed"));
	GetOrCreateSchedule()->RestoreData(Data);
	EvaluateAndApply(CurrentDay); NotifyChanged();
	return FEarth4DResult::Ok(FString::Printf(TEXT("Loaded %d tasks / %d elements from %s"), Data.Tasks.Num(), Data.Elements.Num(), *Path));
}

FEarth4DResult UEarth4DSubsystem::SaveScenario(const FString& Name)
{
	if (Name.TrimStartAndEnd().IsEmpty()) return FEarth4DResult::Fail(TEXT("Scenario needs a name"));
	return SaveProject(ScenarioFile(Name));
}

FEarth4DResult UEarth4DSubsystem::LoadScenario(const FString& Name)
{
	return LoadProject(ScenarioFile(Name));
}

TArray<FString> UEarth4DSubsystem::ListScenarios() const
{
	TArray<FString> Names;
	IFileManager& FM = IFileManager::Get();
	TArray<FString> Files;
	FM.FindFiles(Files, *FPaths::Combine(ScenarioDir(), TEXT("*.e4d.json")), true, false);
	for (FString& F : Files) Names.Add(FPaths::GetBaseFilename(F).Replace(TEXT(".e4d"), TEXT("")));
	return Names;
}

// ---- Query ----
FString UEarth4DSubsystem::GetScheduleSummary() const
{
	if (!Schedule) return TEXT("No schedule loaded.");
	FString Out = FString::Printf(TEXT("Project '%s' — %d tasks, %d stages, %d elements. Current day %.1f.\n"),
		*Schedule->ProjectName, Schedule->Tasks.Num(), Schedule->Stages.Num(), Schedule->Elements.Num(), CurrentDay);
	for (const FEarth4DTask& T : Schedule->Tasks)
	{
		const UEnum* TypeEnum = StaticEnum<EEarth4DTaskType>();
		Out += FString::Printf(TEXT("- [%s] '%s' days %.0f-%.0f, %d elements (id=%s)\n"),
			*TypeEnum->GetNameStringByValue((int64)T.Type), *T.Name, T.Start, T.End, T.ObjectIds.Num(), *T.Id);
	}
	return Out;
}

TArray<FString> UEarth4DSubsystem::FindElements(const FString& Query) const
{
	TArray<FString> Out;
	if (!Schedule) return Out;
	const FString Q = Query.TrimStartAndEnd().ToLower();
	for (const FEarth4DElement& E : Schedule->Elements)
	{
		if (Q.IsEmpty() || E.DisplayName.ToLower().Contains(Q) || E.Path.ToLower().Contains(Q))
			Out.Add(E.Id);
	}
	return Out;
}

// ---- Location / region ----
FEarth4DResult UEarth4DSubsystem::SetRegionOrigin(double Lat, double Lon, double Height)
{
	if (!ActiveSite && !FindAndBindSite()) return FEarth4DResult::Fail(TEXT("No Earth4D site in level"));
	ActiveSite->SetRegionOrigin(Lat, Lon, Height);
	// Re-place elements against the new origin.
	EvaluateAndApply(CurrentDay);
	NotifyChanged();
	OnLocationResult.Broadcast(true, FString::Printf(TEXT("Region origin set to %.6f, %.6f"), Lat, Lon), Lat, Lon);
	return FEarth4DResult::Ok(FString::Printf(TEXT("Region origin → %.6f, %.6f, %.1fm"), Lat, Lon, Height));
}

FEarth4DResult UEarth4DSubsystem::FlyToLatLon(double Lat, double Lon, double Height, double ViewDistanceMeters)
{
	if (!ActiveSite && !FindAndBindSite()) return FEarth4DResult::Fail(TEXT("No Earth4D site in level"));
	const FVector Target = ActiveSite->GeodeticToUnreal(Lat, Lon, Height);
	MoveViewToUnreal(Target, ViewDistanceMeters);
	OnLocationResult.Broadcast(true, FString::Printf(TEXT("Flew to %.6f, %.6f"), Lat, Lon), Lat, Lon);
	return FEarth4DResult::Ok(FString::Printf(TEXT("Flew to %.6f, %.6f"), Lat, Lon));
}

FEarth4DResult UEarth4DSubsystem::FrameRegion()
{
	if (!ActiveSite && !FindAndBindSite()) return FEarth4DResult::Fail(TEXT("No Earth4D site in level"));
	const FVector Center = ActiveSite->GetRegionOriginUnreal();
	// Frame from a distance proportional to the region extent.
	MoveViewToUnreal(Center, ActiveSite->RegionHalfExtentMeters * 2.5);
	return FEarth4DResult::Ok(TEXT("Framed project region"));
}

void UEarth4DSubsystem::MoveViewToUnreal(const FVector& TargetUnrealCm, double ViewDistanceMeters)
{
	const double DistCm = FMath::Max(50.0, ViewDistanceMeters) * 100.0;
	// Pull back to the south-and-up so the target sits in frame (region-local ENU: -Y south, +Z up).
	const FVector EyeOffset(0.0, -DistCm * 0.7, DistCm * 0.7);
	const FVector EyePos = TargetUnrealCm + EyeOffset;
	const FRotator LookAt = (TargetUnrealCm - EyePos).Rotation();

	UWorld* World = GetWorld();
	const bool bIsGameWorld = World && (World->IsGameWorld());
	if (bIsGameWorld)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->SetActorLocation(EyePos, false, nullptr, ETeleportType::TeleportPhysics);
				PC->SetControlRotation(LookAt);
			}
			else
			{
				PC->SetControlRotation(LookAt);
			}
		}
	}
#if WITH_EDITOR
	else if (GEditor)
	{
		if (FLevelEditorViewportClient* VC = (GCurrentLevelEditingViewportClient
			? GCurrentLevelEditingViewportClient
			: (GEditor->GetLevelViewportClients().Num() > 0 ? GEditor->GetLevelViewportClients()[0] : nullptr)))
		{
			VC->SetViewLocation(EyePos);
			VC->SetViewRotation(LookAt);
			VC->Invalidate();
		}
	}
#endif
}

bool UEarth4DSubsystem::GetCurrentViewPose(FVector& OutLocation, FRotator& OutRotation) const
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (PC->PlayerCameraManager)
			{
				OutLocation = PC->PlayerCameraManager->GetCameraLocation();
				OutRotation = PC->PlayerCameraManager->GetCameraRotation();
				return true;
			}
			OutLocation = PC->GetFocalLocation();
			OutRotation = PC->GetControlRotation();
			return true;
		}
	}
#if WITH_EDITOR
	if (GEditor)
	{
		if (FLevelEditorViewportClient* VC = GCurrentLevelEditingViewportClient
			? GCurrentLevelEditingViewportClient
			: (GEditor->GetLevelViewportClients().Num() > 0 ? GEditor->GetLevelViewportClients()[0] : nullptr))
		{
			OutLocation = VC->GetViewLocation();
			OutRotation = VC->GetViewRotation();
			return true;
		}
	}
#endif
	return false;
}

void UEarth4DSubsystem::GeocodeAndGoTo(const FString& Query, bool bSetRegionOrigin)
{
	if (Query.TrimStartAndEnd().IsEmpty())
	{
		OnLocationResult.Broadcast(false, TEXT("Empty query"), 0.0, 0.0);
		return;
	}
	// Nominatim (OpenStreetMap) — same approach as the web app. Free, no key. A
	// User-Agent is required by Nominatim's usage policy. VERIFY: for production
	// volume swap to the Cesium ion geocoder or a keyed provider.
	const FString Url = FString::Printf(
		TEXT("https://nominatim.openstreetmap.org/search?format=json&limit=1&q=%s"),
		*FGenericPlatformHttp::UrlEncode(Query));

	FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("GET"));
	Req->SetHeader(TEXT("User-Agent"), TEXT("Earth4D-UE/0.1 (+https://github.com/VDC-Austin-KA/EarthEditor)"));
	Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Req->OnProcessRequestComplete().BindUObject(this, &UEarth4DSubsystem::OnGeocodeResponse, bSetRegionOrigin);
	Req->ProcessRequest();
}

void UEarth4DSubsystem::OnGeocodeResponse(FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk, bool bSetRegionOrigin)
{
	if (!bOk || !Resp.IsValid() || Resp->GetResponseCode() >= 300)
	{
		OnLocationResult.Broadcast(false, TEXT("Geocode network error"), 0.0, 0.0);
		return;
	}
	TArray<TSharedPtr<FJsonValue>> Results;
	TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
	if (!FJsonSerializer::Deserialize(R, Results) || Results.Num() == 0)
	{
		OnLocationResult.Broadcast(false, TEXT("No location found"), 0.0, 0.0);
		return;
	}
	const TSharedPtr<FJsonObject> First = Results[0]->AsObject();
	if (!First.IsValid())
	{
		OnLocationResult.Broadcast(false, TEXT("Bad geocode response"), 0.0, 0.0);
		return;
	}
	// Nominatim returns lat/lon as strings.
	const double Lat = FCString::Atod(*First->GetStringField(TEXT("lat")));
	const double Lon = FCString::Atod(*First->GetStringField(TEXT("lon")));
	FString DisplayName; First->TryGetStringField(TEXT("display_name"), DisplayName);

	if (bSetRegionOrigin)
	{
		SetRegionOrigin(Lat, Lon, 0.0); // broadcasts its own result
		FlyToLatLon(Lat, Lon, 0.0);
	}
	else
	{
		FlyToLatLon(Lat, Lon, 0.0);
	}
	OnLocationResult.Broadcast(true, FString::Printf(TEXT("Found: %s"), *DisplayName), Lat, Lon);
}

// ---- Apply ----
void UEarth4DSubsystem::EvaluateAndApply(float Day)
{
	if (!Schedule) return;
	TMap<FString, FEarth4DObjectState> States;
	Earth4DScheduleEvaluator::Evaluate(*Schedule, Day, States);

	// Shared material-apply config for this pass.
	Earth4DMaterial::FApplyConfig MatCfg;
	MatCfg.VisibilityOpacityCutoff = MaterialFadeVisibilityCutoff;

	for (const FEarth4DElement& Elem : Schedule->Elements)
	{
		USceneComponent* Comp = Elem.Component.Get();
		if (!Comp) continue;
		const FEarth4DObjectState* St = States.Find(Elem.Id);
		if (!St)
		{
			Comp->SetVisibility(true, true);
			Comp->SetRelativeTransform(Elem.BaseTransform);
			if (bDriveMaterials)
			{
				if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
					Earth4DMaterial::ResetState(Prim, MatCfg);
			}
			continue;
		}

		Comp->SetVisibility(St->bVisible, true);
		if (!St->bVisible) continue;

		// Compose base transform with animation offset/scale/spin + manual edit.
		const FEarth4DObjectEdit& Ed = Elem.Edit;
		FTransform X = Elem.BaseTransform;
		const float EditYaw = Ed.RotationDeg;
		FQuat Spin = FQuat(FVector::UpVector, FMath::DegreesToRadians(EditYaw) + St->Spin);
		X.SetRotation(Spin * X.GetRotation());
		X.SetScale3D(Elem.BaseTransform.GetScale3D() * Ed.Scale * St->Scale);
		// Offsets are in region-local ENU metres. With a site, rotate them into the
		// Cesium-georeferenced world (handles ESU/handedness + georeference rotation);
		// without a site, fall back to the naive per-axis metres→centimetres path.
		const FVector EnuOffsetM = St->Offset + Ed.Offset;
		const FVector OffsetCm = ActiveSite
			? ActiveSite->RegionLocalEnuOffsetToUnreal(EnuOffsetM)
			: EnuOffsetM * 100.f;
		X.SetLocation(Elem.BaseTransform.GetLocation() + OffsetCm);
		Comp->SetRelativeTransform(X);

		// Material pass: drive opacity (fade), section-reveal clip plane and action
		// glow via cached Dynamic Material Instances on the element's materials.
		// St->Opacity already folds in Ed.Opacity (see the evaluator). The clip plane
		// is converted from region-local ENU into the Cesium-georeferenced world here.
		if (bDriveMaterials)
		{
			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
			{
				FVector ClipNormalWorld(0.f, 0.f, 1.f);
				float ClipConstantWorld = 0.f;
				Earth4DMaterial::ComputeWorldClip(*St, ActiveSite, ClipNormalWorld, ClipConstantWorld);
				Earth4DMaterial::ApplyState(Prim, *St, Ed, ClipNormalWorld, ClipConstantWorld, MatCfg);
			}
		}
	}

	// Drive the day-driven feature actors (vehicles / annotations / excavation / film)
	// from the SAME day so the whole scene shares one clock. Prune dead weak refs.
	for (int32 i = DayDrivenActors.Num() - 1; i >= 0; --i)
	{
		UObject* Obj = DayDrivenActors[i].Get();
		if (!Obj) { DayDrivenActors.RemoveAtSwap(i); continue; }
		if (IEarth4DDayDriven* DD = Cast<IEarth4DDayDriven>(Obj)) DD->ApplyDay(Day, ActiveSite);
	}
}

// ---- Day-driven feature registry ----
void UEarth4DSubsystem::RegisterDayDriven(UObject* Actor)
{
	if (Actor && Actor->GetClass()->ImplementsInterface(UEarth4DDayDriven::StaticClass()))
		DayDrivenActors.AddUnique(Actor);
}

void UEarth4DSubsystem::UnregisterDayDriven(UObject* Actor)
{
	DayDrivenActors.RemoveAll([&](const TWeakObjectPtr<UObject>& W) { return W.Get() == Actor; });
}

// ---- Phase 6 command verbs ----
FEarth4DResult UEarth4DSubsystem::AddAnnotation(const FString& Text, FVector EnuMeters, float AppearDay, float DisappearDay)
{
	UWorld* World = GetWorld();
	if (!World) return FEarth4DResult::Fail(TEXT("No world"));
	AEarth4DAnnotation* A = World->SpawnActor<AEarth4DAnnotation>();
	if (!A) return FEarth4DResult::Fail(TEXT("Spawn failed"));
	A->EnuMeters = EnuMeters;
	A->AppearDay = AppearDay;
	A->DisappearDay = DisappearDay;
	A->SetText(Text);
	A->ApplyDay(CurrentDay, ActiveSite);
	return FEarth4DResult::Ok(FString::Printf(TEXT("Annotation '%s' placed."), *Text));
}

FEarth4DResult UEarth4DSubsystem::AddExcavation(FVector EnuMeters, FVector SizeMeters, float StartDay, float Days)
{
	UWorld* World = GetWorld();
	if (!World) return FEarth4DResult::Fail(TEXT("No world"));
	AEarth4DExcavation* E = World->SpawnActor<AEarth4DExcavation>();
	if (!E) return FEarth4DResult::Fail(TEXT("Spawn failed"));
	E->EnuMeters = EnuMeters; E->SizeMeters = SizeMeters; E->StartDay = StartDay; E->Days = FMath::Max(1.f, Days);
	E->ApplyDay(CurrentDay, ActiveSite);
	return FEarth4DResult::Ok(TEXT("Excavation added."));
}

FEarth4DResult UEarth4DSubsystem::AddVehicle(const FString& Name, FVector FromEnuMeters, FVector ToEnuMeters, float StartDay, float Days, bool bLoop)
{
	UWorld* World = GetWorld();
	if (!World) return FEarth4DResult::Fail(TEXT("No world"));
	AEarth4DVehicle* V = World->SpawnActor<AEarth4DVehicle>();
	if (!V) return FEarth4DResult::Fail(TEXT("Spawn failed"));
#if WITH_EDITOR
	if (!Name.IsEmpty()) V->SetActorLabel(Name);
#endif
	V->VehicleId = NewId(TEXT("veh"));
	V->VehicleType = TEXT("truck");
	V->DisplayName = Name.IsEmpty() ? TEXT("Vehicle") : Name;
	V->RouteEnuMeters = { FromEnuMeters, ToEnuMeters };
	V->StartDay = StartDay; V->Days = FMath::Max(1.f, Days); V->bLoop = bLoop;
	V->RebuildRoute(ActiveSite);
	V->ApplyDay(CurrentDay, ActiveSite);
	return FEarth4DResult::Ok(FString::Printf(TEXT("Vehicle '%s' added (%s)."), *V->DisplayName, *V->VehicleId));
}

namespace
{
	// Built-in construction-equipment catalog (generic stand-ins, like the web app's).
	// Type → approx length in metres, used to scale the placeholder body.
	static const TArray<TPair<FString, float>>& Earth4DVehicleCatalog()
	{
		static const TArray<TPair<FString, float>> Catalog = {
			{ TEXT("excavator"), 7.f }, { TEXT("dump_truck"), 9.f }, { TEXT("bulldozer"), 6.f },
			{ TEXT("crane"), 14.f }, { TEXT("concrete_mixer"), 9.f }, { TEXT("loader"), 7.f },
			{ TEXT("backhoe"), 6.f }, { TEXT("flatbed_truck"), 12.f }, { TEXT("pickup"), 5.5f },
			{ TEXT("car"), 4.5f }, { TEXT("forklift"), 3.5f }, { TEXT("truck"), 9.f }
		};
		return Catalog;
	}
	AEarth4DVehicle* Earth4DFindVehicle(UWorld* World, const FString& Id)
	{
		if (!World) return nullptr;
		for (TActorIterator<AEarth4DVehicle> It(World); It; ++It)
			if (It->VehicleId == Id) return *It;
		return nullptr;
	}
}

TArray<FString> UEarth4DSubsystem::ListVehicleTypes() const
{
	TArray<FString> Out;
	for (const TPair<FString, float>& P : Earth4DVehicleCatalog()) Out.Add(P.Key);
	return Out;
}

FEarth4DResult UEarth4DSubsystem::PlaceVehicle(const FString& Type, FVector EnuMeters, float HeadingDeg, FString& OutVehicleId)
{
	UWorld* World = GetWorld();
	if (!World) return FEarth4DResult::Fail(TEXT("No world"));
	float Size = 9.f; FString ResolvedType = Type.IsEmpty() ? TEXT("truck") : Type.ToLower();
	for (const TPair<FString, float>& P : Earth4DVehicleCatalog()) if (P.Key == ResolvedType) { Size = P.Value; break; }
	AEarth4DVehicle* V = World->SpawnActor<AEarth4DVehicle>();
	if (!V) return FEarth4DResult::Fail(TEXT("Spawn failed"));
	V->VehicleId = NewId(TEXT("veh"));
	V->VehicleType = ResolvedType;
	V->DisplayName = ResolvedType;
	// A short route along the heading so the body has an orientation and sits in place.
	const float Rad = FMath::DegreesToRadians(HeadingDeg);
	const FVector Fwd(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);
	V->RouteEnuMeters = { EnuMeters - Fwd * (Size * 0.5f), EnuMeters + Fwd * (Size * 0.5f) };
	V->StartDay = 0.f; V->Days = 1.f; V->bLoop = false; V->bHideOutsideWindow = false;
	V->RebuildRoute(ActiveSite);
	V->ApplyDay(CurrentDay, ActiveSite);
	OutVehicleId = V->VehicleId;
#if WITH_EDITOR
	V->SetActorLabel(FString::Printf(TEXT("Vehicle_%s"), *ResolvedType));
#endif
	return FEarth4DResult::Ok(FString::Printf(TEXT("Placed %s (%s)"), *ResolvedType, *V->VehicleId));
}

TArray<FString> UEarth4DSubsystem::ListVehicles() const
{
	TArray<FString> Out;
	if (UWorld* World = GetWorld())
		for (TActorIterator<AEarth4DVehicle> It(World); It; ++It)
			Out.Add(FString::Printf(TEXT("%s | %s | %s"), *It->VehicleId, *It->VehicleType, *It->DisplayName));
	return Out;
}

FEarth4DResult UEarth4DSubsystem::RemoveVehicle(const FString& VehicleId)
{
	AEarth4DVehicle* V = Earth4DFindVehicle(GetWorld(), VehicleId);
	if (!V) return FEarth4DResult::Fail(TEXT("Vehicle not found"));
	V->Destroy();
	return FEarth4DResult::Ok(TEXT("Vehicle removed"));
}

FEarth4DResult UEarth4DSubsystem::SetVehicleRoute(const FString& VehicleId, const TArray<FVector>& RouteEnuMeters, float StartDay, float Days, bool bLoop)
{
	AEarth4DVehicle* V = Earth4DFindVehicle(GetWorld(), VehicleId);
	if (!V) return FEarth4DResult::Fail(TEXT("Vehicle not found"));
	if (RouteEnuMeters.Num() < 2) return FEarth4DResult::Fail(TEXT("A route needs at least two points"));
	V->RouteEnuMeters = RouteEnuMeters;
	V->StartDay = StartDay; V->Days = FMath::Max(1.f, Days); V->bLoop = bLoop;
	V->RebuildRoute(ActiveSite);
	V->ApplyDay(CurrentDay, ActiveSite);
	return FEarth4DResult::Ok(FString::Printf(TEXT("Route set (%d points)"), RouteEnuMeters.Num()));
}

FEarth4DResult UEarth4DSubsystem::CreateTraffic(const TArray<FVector>& PathEnuMeters, int32 Count, float Days, const FString& Type, int32& OutSpawned)
{
	UWorld* World = GetWorld();
	if (!World) return FEarth4DResult::Fail(TEXT("No world"));
	if (PathEnuMeters.Num() < 2) return FEarth4DResult::Fail(TEXT("Traffic needs a path of at least two points"));
	const int32 N = FMath::Clamp(Count, 1, 200);
	const float Loop = FMath::Max(1.f, Days);
	OutSpawned = 0;
	for (int32 i = 0; i < N; ++i)
	{
		AEarth4DVehicle* V = World->SpawnActor<AEarth4DVehicle>();
		if (!V) continue;
		V->VehicleId = NewId(TEXT("veh"));
		V->VehicleType = Type.IsEmpty() ? TEXT("car") : Type.ToLower();
		V->DisplayName = FString::Printf(TEXT("traffic_%d"), i);
		V->RouteEnuMeters = PathEnuMeters;
		// Stagger each vehicle's phase so they spread along the path.
		V->StartDay = -(Loop * i) / N;
		V->Days = Loop; V->bLoop = true; V->bHideOutsideWindow = false;
		V->RebuildRoute(ActiveSite);
		V->ApplyDay(CurrentDay, ActiveSite);
		++OutSpawned;
	}
	return FEarth4DResult::Ok(FString::Printf(TEXT("Spawned %d traffic vehicles"), OutSpawned));
}

// ---- Export (glTF / GLB) ----
namespace
{
	// Export the current world to a glTF/GLB via UE's glTF Exporter plugin (editor only).
	// Returns false + message if the exporter isn't available (plugin disabled / packaged build).
	FEarth4DResult Earth4DExportWorld(UWorld* World, const FString& FilePath)
	{
#if WITH_EDITOR
		if (!World) return FEarth4DResult::Fail(TEXT("No world"));
		const FString Dir = FPaths::GetPath(FilePath);
		if (!Dir.IsEmpty()) IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
		UAssetExportTask* Task = NewObject<UAssetExportTask>();
		Task->Object = World;
		Task->Filename = FilePath;
		Task->bSelected = false;
		Task->bReplaceIdentical = true;
		Task->bPrompt = false;
		Task->bAutomated = true;
		Task->bUseFileArchive = FilePath.EndsWith(TEXT(".glb")); // binary container
		Task->bWriteEmptyFiles = false;
		const bool bOk = UExporter::RunAssetExportTask(Task);
		if (!bOk) return FEarth4DResult::Fail(TEXT("glTF export failed — enable the 'glTF Exporter' plugin (Edit → Plugins)."));
		return FEarth4DResult::Ok(FString::Printf(TEXT("Exported %s"), *FilePath));
#else
		return FEarth4DResult::Fail(TEXT("Export is editor-only; in the packaged app use the Film camera + Movie Render Queue to record."));
#endif
	}
}

FEarth4DResult UEarth4DSubsystem::ExportRegionGLB(const FString& FilePath)
{
	const FString Path = FilePath.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Earth4D/Exports/region.glb")) : FilePath;
	EvaluateAndApply(CurrentDay);
	return Earth4DExportWorld(GetWorld(), Path);
}

FEarth4DResult UEarth4DSubsystem::ExportAnimatedGLB(const FString& FilePath)
{
	const FString Path = FilePath.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Earth4D/Exports/animated.glb")) : FilePath;
	// The glTF Exporter bakes any level animations present; the schedule's per-day
	// element motion is best recorded via the Film camera + Movie Render Queue.
	FEarth4DResult R = Earth4DExportWorld(GetWorld(), Path);
	if (R.bOk) R.Message += TEXT(" (static scene; for the full 4D animation use Play Film + Movie Render Queue)");
	return R;
}

FEarth4DResult UEarth4DSubsystem::ExportPerTaskGLB(const FString& FolderPath, int32& OutFiles)
{
	OutFiles = 0;
	if (!Schedule) return FEarth4DResult::Fail(TEXT("No schedule"));
	const FString Folder = FolderPath.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Earth4D/Exports/PerTask")) : FolderPath;
	const float SavedDay = CurrentDay;
	for (const FEarth4DTask& T : Schedule->Tasks)
	{
		EvaluateAndApply(T.End); // model state at this task's completion
		FString Safe = T.Name.IsEmpty() ? T.Id : T.Name;
		Safe = Safe.Replace(TEXT(" "), TEXT("_")).Replace(TEXT("/"), TEXT("-"));
		const FString Path = FPaths::Combine(Folder, FString::Printf(TEXT("%02d_%s.glb"), OutFiles + 1, *Safe));
		if (Earth4DExportWorld(GetWorld(), Path).bOk) ++OutFiles;
	}
	EvaluateAndApply(SavedDay);
	return OutFiles > 0 ? FEarth4DResult::Ok(FString::Printf(TEXT("Exported %d per-task GLBs to %s"), OutFiles, *Folder))
		: FEarth4DResult::Fail(TEXT("No files exported (enable the glTF Exporter plugin, or no tasks)."));
}

FEarth4DResult UEarth4DSubsystem::AddCameraKeyframe(float Day)
{
	UWorld* World = GetWorld();
	if (!World) return FEarth4DResult::Fail(TEXT("No world"));
	if (!FilmDirector)
	{
		FilmDirector = World->SpawnActor<AEarth4DCameraDirector>();
		if (FilmDirector) RegisterDayDriven(FilmDirector);
	}
	if (!FilmDirector) return FEarth4DResult::Fail(TEXT("No film director"));

	FVector Loc; FRotator Rot;
	if (!GetCurrentViewPose(Loc, Rot)) return FEarth4DResult::Fail(TEXT("Could not read the current view"));
	FilmDirector->AddKey(Day, Loc, Rot);
	return FEarth4DResult::Ok(FString::Printf(TEXT("Camera keyframe at day %.1f (%d total)."), Day, FilmDirector->NumKeys()));
}

FEarth4DResult UEarth4DSubsystem::ClearFilm()
{
	if (FilmDirector) { FilmDirector->ClearKeys(); FilmDirector->SetDriveView(false); }
	return FEarth4DResult::Ok(TEXT("Film cleared."));
}

FEarth4DResult UEarth4DSubsystem::PlayFilm()
{
	if (!FilmDirector || FilmDirector->NumKeys() < 1) return FEarth4DResult::Fail(TEXT("No camera keyframes — add some first."));
	FilmDirector->SetDriveView(true);
	float Mn, Mx; if (Schedule) { Schedule->GetBounds(Mn, Mx); SetCurrentDay(Mn); }
	Play();
	return FEarth4DResult::Ok(TEXT("Playing film."));
}
