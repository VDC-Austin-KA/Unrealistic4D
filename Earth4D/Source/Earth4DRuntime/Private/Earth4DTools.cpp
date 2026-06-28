// Copyright Earth4D. Licensed for project use.
#include "Earth4DTools.h"
#include "Earth4DSubsystem.h"
#include "Earth4DSchedule.h"
#include "Dom/JsonObject.h"

namespace
{
	EEarth4DAnimStyle ParseStyle(const FString& S)
	{
		const UEnum* E = StaticEnum<EEarth4DAnimStyle>();
		const int64 V = E->GetValueByNameString(S, EGetByNameFlags::CaseSensitive);
		return V == INDEX_NONE ? EEarth4DAnimStyle::Rise : (EEarth4DAnimStyle)V;
	}
	EEarth4DDirection ParseDir(const FString& S)
	{
		const UEnum* E = StaticEnum<EEarth4DDirection>();
		const int64 V = E->GetValueByNameString(S);
		return V == INDEX_NONE ? EEarth4DDirection::Above : (EEarth4DDirection)V;
	}
	EEarth4DTaskType ParseType(const FString& S)
	{
		const UEnum* E = StaticEnum<EEarth4DTaskType>();
		const int64 V = E->GetValueByNameString(S);
		return V == INDEX_NONE ? EEarth4DTaskType::Construction : (EEarth4DTaskType)V;
	}
	TArray<FString> ResolveTargets(UEarth4DSubsystem* Sub, const TSharedPtr<FJsonObject>& Args)
	{
		// Accept either explicit element ids or a name query ("element_query").
		TArray<FString> Ids;
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Args->TryGetArrayField(TEXT("element_ids"), Arr))
			for (const auto& V : *Arr) Ids.Add(V->AsString());
		FString Query;
		if (Args->TryGetStringField(TEXT("element_query"), Query))
			Ids.Append(Sub->FindElements(Query));
		return Ids;
	}
}

namespace Earth4DTools
{
	FString GetToolsJson()
	{
		// Kept as a literal for clarity; every tool maps to a subsystem verb.
		return TEXT(R"JSON([
{"name":"list_tasks","description":"Summarize the current 4D schedule: tasks, stages, element counts and the current day. Call this first to ground yourself.","input_schema":{"type":"object","properties":{}}},
{"name":"find_elements","description":"Find scheduled element ids by a name/path substring.","input_schema":{"type":"object","properties":{"query":{"type":"string"}},"required":["query"]}},
{"name":"add_task","description":"Create a 4D task.","input_schema":{"type":"object","properties":{"name":{"type":"string"},"type":{"type":"string","enum":["Construction","Demolition","Temporary"]},"start":{"type":"number","description":"start day index"},"duration":{"type":"number","description":"days"}},"required":["name","duration"]}},
{"name":"remove_task","description":"Delete a task by id.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"}},"required":["task_id"]}},
{"name":"set_task_duration","description":"Set a task's duration in days (keeps its start).","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"days":{"type":"number"}},"required":["task_id","days"]}},
{"name":"set_task_window","description":"Set a task's start and end day.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"start":{"type":"number"},"end":{"type":"number"}},"required":["task_id","start","end"]}},
{"name":"set_task_type","description":"Set a task type.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"type":{"type":"string","enum":["Construction","Demolition","Temporary"]}},"required":["task_id","type"]}},
{"name":"set_task_style","description":"Set a task's animation style.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"style":{"type":"string","enum":["Fade","Drop","Rise","Slide","Grow","GrowUp","Spiral","Swoop","Assemble","Wipe"]}},"required":["task_id","style"]}},
{"name":"set_task_direction","description":"Set the direction a task's motion comes from.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"direction":{"type":"string","enum":["North","South","East","West","Above","Below"]}},"required":["task_id","direction"]}},
{"name":"set_task_stagger","description":"Set Fuzor-style per-element stagger (days).","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"stagger":{"type":"number"}},"required":["task_id","stagger"]}},
{"name":"assign_elements","description":"Assign elements to a task (by ids and/or a name query). add=false replaces.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"element_ids":{"type":"array","items":{"type":"string"}},"element_query":{"type":"string"},"add":{"type":"boolean"}},"required":["task_id"]}},
)JSON")
TEXT(R"JSON({"name":"remove_elements","description":"Remove elements from a task.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"element_ids":{"type":"array","items":{"type":"string"}},"element_query":{"type":"string"}},"required":["task_id"]}},
{"name":"add_stage","description":"Add a stage / phase.","input_schema":{"type":"object","properties":{"name":{"type":"string"}},"required":["name"]}},
{"name":"set_current_day","description":"Scrub the playback to a construction day.","input_schema":{"type":"object","properties":{"day":{"type":"number"}},"required":["day"]}},
{"name":"auto_assign_from_metadata","description":"Group elements by an imported metadata key (e.g. a Navisworks/Revit property or phase) and auto-create a task per group with the elements assigned.","input_schema":{"type":"object","properties":{"meta_key":{"type":"string"},"type":{"type":"string","enum":["Construction","Demolition"]},"days_per_task":{"type":"number"}},"required":["meta_key"]}},
{"name":"ingest_actor","description":"Import a scene actor (and, by default, its attached child-actor subtree) into the 4D schedule: every mesh becomes a schedulable element with its imported Datasmith/Navisworks/Revit metadata. Match by actor label/name (substring ok).","input_schema":{"type":"object","properties":{"actor_name":{"type":"string"},"recurse":{"type":"boolean","description":"also ingest attached child actors (default true)"}},"required":["actor_name"]}},
{"name":"import_selected","description":"Ingest the actors currently selected in the editor into the 4D schedule (authoring-time).","input_schema":{"type":"object","properties":{"recurse":{"type":"boolean"}}}},
{"name":"set_region_origin","description":"Re-origin the project region (and the Cesium georeference) to a geodetic point. Use this to set the construction site's real-world location by coordinates.","input_schema":{"type":"object","properties":{"lat":{"type":"number","description":"latitude degrees (WGS84)"},"lon":{"type":"number","description":"longitude degrees (WGS84)"},"height":{"type":"number","description":"ellipsoidal height in metres (optional)"}},"required":["lat","lon"]}},
{"name":"fly_to","description":"Move the active camera to view a geodetic point on the Google 3D Tiles.","input_schema":{"type":"object","properties":{"lat":{"type":"number"},"lon":{"type":"number"},"height":{"type":"number"},"view_distance_m":{"type":"number","description":"how far back to frame from, metres"}},"required":["lat","lon"]}},
{"name":"frame_region","description":"Move the active camera to frame the whole project region.","input_schema":{"type":"object","properties":{}}},
{"name":"geocode_goto","description":"Search for a place by name (e.g. 'Sydney Opera House') and move the project region + camera there. Async; result is reported back when found.","input_schema":{"type":"object","properties":{"query":{"type":"string"},"set_region_origin":{"type":"boolean","description":"also re-origin the region to the found place (default true)"}},"required":["query"]}},
)JSON")
TEXT(R"JSON({"name":"add_annotation","description":"Place a 3D-text annotation/callout on the site at a region-local ENU point (metres: east/north/up), optionally gated by a day window.","input_schema":{"type":"object","properties":{"text":{"type":"string"},"east":{"type":"number"},"north":{"type":"number"},"up":{"type":"number"},"appear_day":{"type":"number"},"disappear_day":{"type":"number"}},"required":["text"]}},
{"name":"add_excavation","description":"Dig an excavation pit at a region-local ENU point that carves out over a day window. size_e/size_n are the footprint, depth is the dig depth (metres).","input_schema":{"type":"object","properties":{"east":{"type":"number"},"north":{"type":"number"},"up":{"type":"number"},"size_e":{"type":"number"},"size_n":{"type":"number"},"depth":{"type":"number"},"start":{"type":"number"},"days":{"type":"number"}},"required":["size_e","size_n","depth","days"]}},
{"name":"add_vehicle","description":"Add a vehicle/equipment that drives a straight route between two region-local ENU points (metres) across a day window. Set loop=true for continuous traffic.","input_schema":{"type":"object","properties":{"name":{"type":"string"},"from_east":{"type":"number"},"from_north":{"type":"number"},"from_up":{"type":"number"},"to_east":{"type":"number"},"to_north":{"type":"number"},"to_up":{"type":"number"},"start":{"type":"number"},"days":{"type":"number"},"loop":{"type":"boolean"}},"required":["from_east","from_north","to_east","to_north","days"]}},
{"name":"add_camera_keyframe","description":"Capture the current camera/viewport pose as a film keyframe at a construction day. Build a fly-through by adding several at different days.","input_schema":{"type":"object","properties":{"day":{"type":"number"}},"required":["day"]}},
{"name":"play_film","description":"Play the schedule with the film camera driving the view along the captured keyframes (a recorded fly-through of the build).","input_schema":{"type":"object","properties":{}}},
{"name":"clear_film","description":"Remove all camera keyframes.","input_schema":{"type":"object","properties":{}}},
{"name":"save_project","description":"Save the current 4D project (tasks/stages/elements) to disk.","input_schema":{"type":"object","properties":{"path":{"type":"string","description":"optional file path; default project file if omitted"}}}},
{"name":"load_project","description":"Load a 4D project from disk.","input_schema":{"type":"object","properties":{"path":{"type":"string"}}}},
{"name":"save_scenario","description":"Save the current schedule as a named scenario (a what-if snapshot).","input_schema":{"type":"object","properties":{"name":{"type":"string"}},"required":["name"]}},
{"name":"load_scenario","description":"Load a previously-saved named scenario.","input_schema":{"type":"object","properties":{"name":{"type":"string"}},"required":["name"]}},
{"name":"list_scenarios","description":"List the saved scenario names.","input_schema":{"type":"object","properties":{}}}
])JSON");
	}

	FString Dispatch(const FString& ToolName, const TSharedPtr<FJsonObject>& Args, UEarth4DSubsystem* Sub)
	{
		if (!Sub || !Args) return TEXT("error: no subsystem/args");

		if (ToolName == TEXT("list_tasks")) return Sub->GetScheduleSummary();

		if (ToolName == TEXT("find_elements"))
		{
			const TArray<FString> Ids = Sub->FindElements(Args->GetStringField(TEXT("query")));
			return FString::Printf(TEXT("Found %d: %s"), Ids.Num(), *FString::Join(Ids, TEXT(", ")));
		}
		if (ToolName == TEXT("add_task"))
		{
			FString Id;
			const FEarth4DResult R = Sub->AddTask(
				Args->GetStringField(TEXT("name")),
				ParseType(Args->HasField(TEXT("type")) ? Args->GetStringField(TEXT("type")) : TEXT("Construction")),
				Args->HasField(TEXT("start")) ? Args->GetNumberField(TEXT("start")) : 0.f,
				Args->GetNumberField(TEXT("duration")), Id);
			return R.Message;
		}
		if (ToolName == TEXT("remove_task")) return Sub->RemoveTask(Args->GetStringField(TEXT("task_id"))).Message;
		if (ToolName == TEXT("set_task_duration")) return Sub->SetTaskDuration(Args->GetStringField(TEXT("task_id")), Args->GetNumberField(TEXT("days"))).Message;
		if (ToolName == TEXT("set_task_window")) return Sub->SetTaskWindow(Args->GetStringField(TEXT("task_id")), Args->GetNumberField(TEXT("start")), Args->GetNumberField(TEXT("end"))).Message;
		if (ToolName == TEXT("set_task_type")) return Sub->SetTaskType(Args->GetStringField(TEXT("task_id")), ParseType(Args->GetStringField(TEXT("type")))).Message;
		if (ToolName == TEXT("set_task_style")) return Sub->SetTaskStyle(Args->GetStringField(TEXT("task_id")), ParseStyle(Args->GetStringField(TEXT("style")))).Message;
		if (ToolName == TEXT("set_task_direction")) return Sub->SetTaskDirection(Args->GetStringField(TEXT("task_id")), ParseDir(Args->GetStringField(TEXT("direction")))).Message;
		if (ToolName == TEXT("set_task_stagger")) return Sub->SetTaskStagger(Args->GetStringField(TEXT("task_id")), Args->GetNumberField(TEXT("stagger"))).Message;
		if (ToolName == TEXT("add_stage")) { FString Id; return Sub->AddStage(Args->GetStringField(TEXT("name")), Id).Message; }
		if (ToolName == TEXT("set_current_day")) { Sub->SetCurrentDay(Args->GetNumberField(TEXT("day"))); return TEXT("ok"); }

		if (ToolName == TEXT("assign_elements"))
		{
			const bool bAdd = Args->HasField(TEXT("add")) ? Args->GetBoolField(TEXT("add")) : true;
			return Sub->AssignElementsToTask(Args->GetStringField(TEXT("task_id")), ResolveTargets(Sub, Args), bAdd).Message;
		}
		if (ToolName == TEXT("remove_elements"))
			return Sub->RemoveElementsFromTask(Args->GetStringField(TEXT("task_id")), ResolveTargets(Sub, Args)).Message;

		if (ToolName == TEXT("set_region_origin"))
		{
			const double Lat = Args->GetNumberField(TEXT("lat"));
			const double Lon = Args->GetNumberField(TEXT("lon"));
			const double Height = Args->HasField(TEXT("height")) ? Args->GetNumberField(TEXT("height")) : 0.0;
			return Sub->SetRegionOrigin(Lat, Lon, Height).Message;
		}
		if (ToolName == TEXT("fly_to"))
		{
			const double Lat = Args->GetNumberField(TEXT("lat"));
			const double Lon = Args->GetNumberField(TEXT("lon"));
			const double Height = Args->HasField(TEXT("height")) ? Args->GetNumberField(TEXT("height")) : 0.0;
			const double Dist = Args->HasField(TEXT("view_distance_m")) ? Args->GetNumberField(TEXT("view_distance_m")) : 600.0;
			return Sub->FlyToLatLon(Lat, Lon, Height, Dist).Message;
		}
		if (ToolName == TEXT("frame_region")) return Sub->FrameRegion().Message;
		if (ToolName == TEXT("geocode_goto"))
		{
			const bool bSetOrigin = Args->HasField(TEXT("set_region_origin")) ? Args->GetBoolField(TEXT("set_region_origin")) : true;
			Sub->GeocodeAndGoTo(Args->GetStringField(TEXT("query")), bSetOrigin);
			// Async: the actual lat/lon arrives via OnLocationResult. Acknowledge the dispatch.
			return FString::Printf(TEXT("Geocoding '%s'… result will follow on resolution."), *Args->GetStringField(TEXT("query")));
		}

		if (ToolName == TEXT("ingest_actor"))
		{
			const bool bRecurse = Args->HasField(TEXT("recurse")) ? Args->GetBoolField(TEXT("recurse")) : true;
			return Sub->IngestActorByName(Args->GetStringField(TEXT("actor_name")), bRecurse).Message;
		}
		if (ToolName == TEXT("import_selected"))
		{
			const bool bRecurse = Args->HasField(TEXT("recurse")) ? Args->GetBoolField(TEXT("recurse")) : true;
			return Sub->IngestSelectedActors(bRecurse).Message;
		}

		auto Num = [&](const TCHAR* Key, double Default = 0.0) { return Args->HasField(Key) ? Args->GetNumberField(Key) : Default; };

		if (ToolName == TEXT("add_annotation"))
		{
			const FVector Enu(Num(TEXT("east")), Num(TEXT("north")), Num(TEXT("up")));
			return Sub->AddAnnotation(Args->GetStringField(TEXT("text")), Enu, Num(TEXT("appear_day"), -1.0), Num(TEXT("disappear_day"), -1.0)).Message;
		}
		if (ToolName == TEXT("add_excavation"))
		{
			const FVector Enu(Num(TEXT("east")), Num(TEXT("north")), Num(TEXT("up")));
			const FVector Size(Num(TEXT("size_e"), 20.0), Num(TEXT("size_n"), 20.0), Num(TEXT("depth"), 5.0));
			return Sub->AddExcavation(Enu, Size, Num(TEXT("start")), Num(TEXT("days"), 5.0)).Message;
		}
		if (ToolName == TEXT("add_vehicle"))
		{
			const FVector From(Num(TEXT("from_east")), Num(TEXT("from_north")), Num(TEXT("from_up")));
			const FVector To(Num(TEXT("to_east")), Num(TEXT("to_north")), Num(TEXT("to_up")));
			const bool bLoop = Args->HasField(TEXT("loop")) ? Args->GetBoolField(TEXT("loop")) : false;
			const FString Name = Args->HasField(TEXT("name")) ? Args->GetStringField(TEXT("name")) : TEXT("Vehicle");
			return Sub->AddVehicle(Name, From, To, Num(TEXT("start")), Num(TEXT("days"), 5.0), bLoop).Message;
		}
		if (ToolName == TEXT("add_camera_keyframe")) return Sub->AddCameraKeyframe(Num(TEXT("day"))).Message;
		if (ToolName == TEXT("play_film")) return Sub->PlayFilm().Message;
		if (ToolName == TEXT("clear_film")) return Sub->ClearFilm().Message;

		if (ToolName == TEXT("save_project")) return Sub->SaveProject(Args->HasField(TEXT("path")) ? Args->GetStringField(TEXT("path")) : FString()).Message;
		if (ToolName == TEXT("load_project")) return Sub->LoadProject(Args->HasField(TEXT("path")) ? Args->GetStringField(TEXT("path")) : FString()).Message;
		if (ToolName == TEXT("save_scenario")) return Sub->SaveScenario(Args->GetStringField(TEXT("name"))).Message;
		if (ToolName == TEXT("load_scenario")) return Sub->LoadScenario(Args->GetStringField(TEXT("name"))).Message;
		if (ToolName == TEXT("list_scenarios"))
		{
			const TArray<FString> S = Sub->ListScenarios();
			return S.Num() ? FString::Join(S, TEXT(", ")) : TEXT("(no scenarios)");
		}

		if (ToolName == TEXT("auto_assign_from_metadata"))
		{
			// Group elements by a metadata value and create one task per group.
			const FString Key = Args->GetStringField(TEXT("meta_key"));
			const EEarth4DTaskType Type = ParseType(Args->HasField(TEXT("type")) ? Args->GetStringField(TEXT("type")) : TEXT("Construction"));
			const float Days = Args->HasField(TEXT("days_per_task")) ? Args->GetNumberField(TEXT("days_per_task")) : 5.f;
			UEarth4DSchedule* Sched = Sub->Schedule;
			if (!Sched) return TEXT("error: no schedule");
			TMap<FString, TArray<FString>> Groups;
			for (const FEarth4DElement& E : Sched->Elements)
				if (const FString* V = E.Meta.Find(Key)) Groups.FindOrAdd(*V).Add(E.Id);
			float Cursor = 0.f; int32 Made = 0;
			for (const TPair<FString, TArray<FString>>& G : Groups)
			{
				FString Id; Sub->AddTask(G.Key, Type, Cursor, Days, Id);
				Sub->AssignElementsToTask(Id, G.Value, false);
				Cursor += Days; Made++;
			}
			return FString::Printf(TEXT("Created %d tasks grouped by '%s'."), Made, *Key);
		}

		return FString::Printf(TEXT("error: unknown tool '%s'"), *ToolName);
	}
}
