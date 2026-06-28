// Copyright Earth4D. Licensed for project use.
#include "Earth4DSubsystem.h"
#include "Earth4DSchedule.h"
#include "Earth4DScheduleEvaluator.h"
#include "Earth4DSite.h"
#include "Components/SceneComponent.h"
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

#if WITH_EDITOR
#include "Editor.h"                    // GEditor
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
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

FString UEarth4DSubsystem::RegisterElement(USceneComponent* Component, const FString& DisplayName, const FString& Path)
{
	if (!Schedule || !Component) return FString();
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

	for (const FEarth4DElement& Elem : Schedule->Elements)
	{
		USceneComponent* Comp = Elem.Component.Get();
		if (!Comp) continue;
		const FEarth4DObjectState* St = States.Find(Elem.Id);
		if (!St) { Comp->SetVisibility(true, true); Comp->SetRelativeTransform(Elem.BaseTransform); continue; }

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

		// TODO (material pass): drive opacity (St->Opacity * Ed.Opacity), section
		// clip (St->bHasClip) and glow (St->bHasGlow) via a dynamic material
		// instance / masked material. Hook: ApplyElementMaterialState(Comp, *St, Ed).
	}
}
