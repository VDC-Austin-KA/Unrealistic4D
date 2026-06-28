// Copyright Earth4D. Licensed for project use.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Interfaces/IHttpRequest.h"
#include "Earth4DTypes.h"
#include "Earth4DSubsystem.generated.h"

class UEarth4DSchedule;
class AEarth4DSite;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEarth4DScheduleChanged);
/** Result of an async location/geocode action (success, message, lat, lon). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FEarth4DLocationResult, bool, bSuccess, const FString&, Message, double, Latitude, double, Longitude);

/**
 * The Earth4D command layer + runtime driver.
 *
 * THE SINGLE SOURCE OF MUTATION: UMG widgets, the in-app Claude chat, and the
 * editor MCP server all call these BlueprintCallable verbs — nothing else edits
 * the schedule. Ticks the active day and applies per-element animation to the
 * scene (port of the web viewer's applyAnimationStates).
 */
UCLASS(BlueprintType)
class EARTH4DRUNTIME_API UEarth4DSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UEarth4DSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return Schedule != nullptr; }

	UPROPERTY(BlueprintReadOnly, Category = "Earth4D") UEarth4DSchedule* Schedule = nullptr;
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D") float CurrentDay = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D") bool bPlaying = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D") float DaysPerSecond = 1.5f;

	/** EvaluateAndApply drives opacity/clip/glow onto element materials (Phase 3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Render") bool bDriveMaterials = true;
	/** When a material can't fade, hide the element below this opacity (safe fallback). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Render") float MaterialFadeVisibilityCutoff = 0.5f;

	/** The active georeferenced site (Cesium georeference + Google tiles). May be null. */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Site") TObjectPtr<AEarth4DSite> ActiveSite = nullptr;

	/** Fires after any command mutates the schedule (UI/Gantt re-read on this). */
	UPROPERTY(BlueprintAssignable, Category = "Earth4D") FEarth4DScheduleChanged OnScheduleChanged;
	/** Fires when a geocode / fly-to / region command resolves (async-safe for chat/UI). */
	UPROPERTY(BlueprintAssignable, Category = "Earth4D") FEarth4DLocationResult OnLocationResult;

	UFUNCTION(BlueprintCallable, Category = "Earth4D") UEarth4DSchedule* GetOrCreateSchedule();
	UFUNCTION(BlueprintCallable, Category = "Earth4D") void SetSchedule(UEarth4DSchedule* InSchedule);

	// ---- Site / georeference ----
	/** Bind the subsystem to a site so element placement & location verbs use Cesium. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Site") void SetSite(AEarth4DSite* InSite);
	/** Find an AEarth4DSite already in the world and bind to it (no-op if none). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Site") AEarth4DSite* FindAndBindSite();
	AEarth4DSite* GetSite() const { return ActiveSite; }

	// ---- Location / region (command verbs; mirrored as natural-language tools) ----
	/** Re-origin the georeference to a geodetic point ("set region center"). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Location") FEarth4DResult SetRegionOrigin(double Lat, double Lon, double Height);
	/** Move the active camera/pawn to view a geodetic point. ViewDistanceMeters frames from afar. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Location") FEarth4DResult FlyToLatLon(double Lat, double Lon, double Height, double ViewDistanceMeters = 600.0);
	/** Frame the whole project region in the active view. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Location") FEarth4DResult FrameRegion();
	/** Geocode a place name (async HTTP) and re-origin + fly there. Reports via OnLocationResult. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Location") void GeocodeAndGoTo(const FString& Query, bool bSetRegionOrigin = true);

	// ---- Playback ----
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Playback") void SetCurrentDay(float Day);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Playback") void Play();
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Playback") void Pause();
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Playback") void SetSpeed(float DaysPerSec);

	// ---- Tasks (command verbs; mirrored as natural-language tools) ----
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult AddTask(const FString& Name, EEarth4DTaskType Type, float Start, float Duration, FString& OutTaskId);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult RemoveTask(const FString& TaskId);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult RenameTask(const FString& TaskId, const FString& NewName);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskDuration(const FString& TaskId, float Days);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskWindow(const FString& TaskId, float Start, float End);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskType(const FString& TaskId, EEarth4DTaskType Type);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskStyle(const FString& TaskId, EEarth4DAnimStyle Style);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskDirection(const FString& TaskId, EEarth4DDirection Direction);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskStagger(const FString& TaskId, float Stagger);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskStage(const FString& TaskId, const FString& StageId);

	// ---- Assignment ----
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Assign") FEarth4DResult AssignElementsToTask(const FString& TaskId, const TArray<FString>& ElementIds, bool bAdd);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Assign") FEarth4DResult RemoveElementsFromTask(const FString& TaskId, const TArray<FString>& ElementIds);

	// ---- Elements ----
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Elements") FEarth4DResult SetElementEdit(const FString& ElementId, const FEarth4DObjectEdit& Edit);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Elements") FEarth4DResult ResetElementEdit(const FString& ElementId);
	/** Register a scene component as a schedulable element; returns its stable id.
	 *  If the component is already registered, returns the existing id (no duplicate). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Elements") FString RegisterElement(USceneComponent* Component, const FString& DisplayName, const FString& Path);

	// ---- Import → elements (Phase 3; mirrored as natural-language tools) ----
	/** Ingest an actor (and, by default, its attached child-actor subtree) into the
	 *  schedule: every mesh becomes an FEarth4DElement in region-local ENU, with
	 *  imported Datasmith/Navisworks/Revit metadata copied onto it. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Elements") FEarth4DResult IngestActor(AActor* Root, bool bRecurse = true);
	/** Find world actor(s) whose label/name matches and ingest them (chat-friendly). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Elements") FEarth4DResult IngestActorByName(const FString& ActorName, bool bRecurse = true);
	/** Ingest the current editor selection (authoring-time; no-op in a packaged build). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Elements") FEarth4DResult IngestSelectedActors(bool bRecurse = true);

	// ---- Stages ----
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Stages") FEarth4DResult AddStage(const FString& Name, FString& OutStageId);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Stages") FEarth4DResult RemoveStage(const FString& StageId);

	// ---- Query (read-only; grounds the chat) ----
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Query") FString GetScheduleSummary() const;
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Query") TArray<FString> FindElements(const FString& Query) const;

	/** Evaluate the schedule at `Day` and write states onto the scene components. */
	void EvaluateAndApply(float Day);

private:
	void NotifyChanged() { OnScheduleChanged.Broadcast(); }
	static FString NewId(const TCHAR* Prefix);

	/** Move the active view (PIE pawn / editor viewport) to look at a UE world point. */
	void MoveViewToUnreal(const FVector& TargetUnrealCm, double ViewDistanceMeters);
	void OnGeocodeResponse(FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk, bool bSetRegionOrigin);
};
