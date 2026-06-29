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
class AEarth4DCameraDirector;

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

	/** The element ids the user/chat has selected; UI highlights them, chat scopes edits to them. */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D") TArray<FString> SelectedElementIds;

	/** Bumped on every mutation; native (Slate) UI polls this to refresh cheaply. */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D") int32 Revision = 0;

	/** EvaluateAndApply drives opacity/clip/glow onto element materials (Phase 3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Render") bool bDriveMaterials = true;
	/** When a material can't fade, hide the element below this opacity (safe fallback). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Render") float MaterialFadeVisibilityCutoff = 0.5f;

	/** The active georeferenced site (Cesium georeference + Google tiles). May be null. */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Site") TObjectPtr<AEarth4DSite> ActiveSite = nullptr;

	/** The film camera director (lazily spawned by AddCameraKeyframe / PlayFilm). */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Film") TObjectPtr<AEarth4DCameraDirector> FilmDirector = nullptr;

	/** Actors implementing IEarth4DDayDriven, ticked each EvaluateAndApply. */
	UPROPERTY() TArray<TWeakObjectPtr<UObject>> DayDrivenActors;

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
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskSequence(const FString& TaskId, EEarth4DSequence Sequence);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskDistance(const FString& TaskId, float Distance);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskOverlap(const FString& TaskId, float Overlap);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskColor(const FString& TaskId, FLinearColor Color);
	/** Set (or clear) the per-task "action glow" emissive shown while an element animates. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetTaskGlowColor(const FString& TaskId, FLinearColor Color, bool bEnable = true);
	/** Align a task relative to another: Mode = after | before | start-with | end-with (Gap days). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult AlignTask(const FString& TaskId, const FString& RelativeToTaskId, const FString& Mode, float Gap = 0.f);
	/** Chain tasks back-to-back in the given order (auto-sequence), each keeping its duration. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SequenceTasks(const TArray<FString>& TaskIds, float Start, float Gap = 0.f);
	/** Set the project start date (ISO-8601), the day-0 anchor for the schedule. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tasks") FEarth4DResult SetProjectStart(const FString& IsoDate);

	// ---- Assignment ----
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Assign") FEarth4DResult AssignElementsToTask(const FString& TaskId, const TArray<FString>& ElementIds, bool bAdd);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Assign") FEarth4DResult RemoveElementsFromTask(const FString& TaskId, const TArray<FString>& ElementIds);

	// ---- Elements ----
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Elements") FEarth4DResult SetElementEdit(const FString& ElementId, const FEarth4DObjectEdit& Edit);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Elements") FEarth4DResult ResetElementEdit(const FString& ElementId);
	/** Set (or clear) an element's manual colour override. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Elements") FEarth4DResult SetElementColor(const FString& ElementId, FLinearColor Color, bool bEnable = true);

	// ---- Selection (shared by UI + chat for scoping) ----
	/** Replace the selection with these element ids (bAdd appends instead). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Select") FEarth4DResult SelectElements(const TArray<FString>& ElementIds, bool bAdd = false);
	/** Select every element whose name/path matches a substring (chat-friendly). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Select") FEarth4DResult SelectByName(const FString& Query, bool bAdd = false);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Select") void ClearSelection();

	// ---- Selection sets (named reusable selections; web-app parity) ----
	/** Save element ids as a named selection set; returns its id (re-names if the name exists). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Select") FEarth4DResult SaveSelectionSet(const FString& Name, const TArray<FString>& ElementIds, FString& OutSetId);
	/** Replace the live selection with a saved set (match by id or name). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Select") FEarth4DResult ApplySelectionSet(const FString& IdOrName);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Select") FEarth4DResult DeleteSelectionSet(const FString& IdOrName);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Select") TArray<FString> ListSelectionSetNames() const;
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
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Stages") FEarth4DResult RenameStage(const FString& StageId, const FString& NewName);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Stages") FEarth4DResult SetStageColor(const FString& StageId, FLinearColor Color);

	// ---- Save / load + scenarios (project persistence) ----
	/** Save the active schedule to JSON. Empty path → the default project file. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|IO") FEarth4DResult SaveProject(const FString& FilePath);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|IO") FEarth4DResult LoadProject(const FString& FilePath);
	/** A scenario is a named schedule snapshot saved alongside the project (Saved/Earth4D/Scenarios). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|IO") FEarth4DResult SaveScenario(const FString& Name);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|IO") FEarth4DResult LoadScenario(const FString& Name);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|IO") TArray<FString> ListScenarios() const;

	// ---- Query (read-only; grounds the chat) ----
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Query") FString GetScheduleSummary() const;
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Query") TArray<FString> FindElements(const FString& Query) const;

	// ---- Day-driven feature actors (vehicles / annotations / excavation / film) ----
	/** Register an actor implementing IEarth4DDayDriven so it ticks with the schedule. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Features") void RegisterDayDriven(UObject* Actor);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Features") void UnregisterDayDriven(UObject* Actor);

	// ---- Phase 6 command verbs (mirrored as natural-language tools) ----
	/** Spawn a 3D-text annotation at a region-local ENU point (metres), appearing on a day. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Annotate") FEarth4DResult AddAnnotation(const FString& Text, FVector EnuMeters, float AppearDay = -1.f, float DisappearDay = -1.f);
	/** Capture the current camera/viewport pose as a film keyframe at the given day. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Film") FEarth4DResult AddCameraKeyframe(float Day);
	/** Clear all film keyframes. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Film") FEarth4DResult ClearFilm();
	/** Play the schedule with the film camera driving the view (a recorded fly-through). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Film") FEarth4DResult PlayFilm();
	/** Spawn an excavation pit at a region-local ENU point that digs out over a day window. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Excavate") FEarth4DResult AddExcavation(FVector EnuMeters, FVector SizeMeters, float StartDay, float Days);
	/** Spawn a vehicle that drives a straight route between two region-local ENU points over a window. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Vehicles") FEarth4DResult AddVehicle(const FString& Name, FVector FromEnuMeters, FVector ToEnuMeters, float StartDay, float Days, bool bLoop = false);
	/** The built-in vehicle/equipment catalog types that PlaceVehicle accepts. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Vehicles") TArray<FString> ListVehicleTypes() const;
	/** Place a catalog vehicle at a region-local ENU point facing HeadingDeg; returns its id. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Vehicles") FEarth4DResult PlaceVehicle(const FString& Type, FVector EnuMeters, float HeadingDeg, FString& OutVehicleId);
	/** List the spawned vehicles as "id | type | name" strings. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Vehicles") TArray<FString> ListVehicles() const;
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Vehicles") FEarth4DResult RemoveVehicle(const FString& VehicleId);
	/** Set a spawned vehicle's multi-point route (region-local ENU metres). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Vehicles") FEarth4DResult SetVehicleRoute(const FString& VehicleId, const TArray<FVector>& RouteEnuMeters, float StartDay, float Days, bool bLoop);
	/** Spawn Count looping vehicles staggered along a shared path for continuous traffic. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Vehicles") FEarth4DResult CreateTraffic(const TArray<FVector>& PathEnuMeters, int32 Count, float Days, const FString& Type, int32& OutSpawned);

	// ---- Export (glTF / GLB; web-app parity) ----
	/** Export the scheduled element geometry to a single GLB at the current day (editor only). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Export") FEarth4DResult ExportRegionGLB(const FString& FilePath);
	/** Export the build as an animated GLB (baked schedule). Editor only. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Export") FEarth4DResult ExportAnimatedGLB(const FString& FilePath);
	/** Export one GLB per task (the model state at each task's completion) into a folder. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Export") FEarth4DResult ExportPerTaskGLB(const FString& FolderPath, int32& OutFiles);

	/** Evaluate the schedule at `Day` and write states onto the scene components. */
	void EvaluateAndApply(float Day);

private:
	void NotifyChanged() { ++Revision; OnScheduleChanged.Broadcast(); }
	static FString NewId(const TCHAR* Prefix);

	/** Move the active view (PIE pawn / editor viewport) to look at a UE world point. */
	void MoveViewToUnreal(const FVector& TargetUnrealCm, double ViewDistanceMeters);
	/** Read the active view pose (PIE camera / editor viewport). Returns false if none. */
	bool GetCurrentViewPose(FVector& OutLocation, FRotator& OutRotation) const;
	void OnGeocodeResponse(FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk, bool bSetRegionOrigin);
};
