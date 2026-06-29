// Copyright Earth4D. Licensed for project use.
#include "Earth4DTools.h"
#include "Earth4DSubsystem.h"
#include "Earth4DSchedule.h"
#include "Earth4DDatasmithLibrary.h"
#include "Earth4DSite.h"
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
TEXT(R"JSON({"name":"rename_task","description":"Rename a task.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"name":{"type":"string"}},"required":["task_id","name"]}},
{"name":"set_task_stage","description":"Assign a task to a stage/phase (empty stage_id clears it).","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"stage_id":{"type":"string"}},"required":["task_id"]}},
{"name":"set_task_sequence","description":"Set how a task orders its elements as it animates.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"sequence":{"type":"string","enum":["ByElevation","ByName","Together","Random"]}},"required":["task_id","sequence"]}},
{"name":"set_task_distance","description":"Set the animation travel distance (metres) for slide/rise/drop styles.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"distance":{"type":"number"}},"required":["task_id","distance"]}},
{"name":"set_task_overlap","description":"Set element overlap 0..1 (0 = strictly sequential, 1 = all elements move together).","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"overlap":{"type":"number"}},"required":["task_id","overlap"]}},
{"name":"set_task_color","description":"Set a task's display colour (RGB 0..1).","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"r":{"type":"number"},"g":{"type":"number"},"b":{"type":"number"}},"required":["task_id","r","g","b"]}},
{"name":"set_task_glow","description":"Set (enable=true) or clear (enable=false) the action-glow emissive shown while a task's elements animate.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"enable":{"type":"boolean"},"r":{"type":"number"},"g":{"type":"number"},"b":{"type":"number"}},"required":["task_id"]}},
{"name":"select_elements","description":"Set the active element selection by ids and/or a name query. add=true appends.","input_schema":{"type":"object","properties":{"element_ids":{"type":"array","items":{"type":"string"}},"element_query":{"type":"string"},"add":{"type":"boolean"}}}},
{"name":"set_element_edit","description":"Modify model content: per-element overrides applied on top of its authored transform/material. Targets element_ids and/or element_query (or the current selection if neither given). Any omitted field is left unchanged.","input_schema":{"type":"object","properties":{"element_ids":{"type":"array","items":{"type":"string"}},"element_query":{"type":"string"},"offset_e":{"type":"number"},"offset_n":{"type":"number"},"offset_up":{"type":"number"},"rotation_deg":{"type":"number"},"scale":{"type":"number"},"opacity":{"type":"number"},"hidden":{"type":"boolean"},"appear_day":{"type":"number"},"disappear_day":{"type":"number"},"stagger_delay":{"type":"number"},"override_style":{"type":"string","enum":["Fade","Drop","Rise","Slide","Grow","GrowUp","Spiral","Swoop","Assemble","Wipe"]},"override_direction":{"type":"string","enum":["North","South","East","West","Above","Below"]}}}},
{"name":"set_element_color","description":"Set (enable=true) or clear (enable=false) a per-element colour override (RGB 0..1). Targets element_ids/element_query/selection.","input_schema":{"type":"object","properties":{"element_ids":{"type":"array","items":{"type":"string"}},"element_query":{"type":"string"},"enable":{"type":"boolean"},"r":{"type":"number"},"g":{"type":"number"},"b":{"type":"number"}}}},
{"name":"reset_element_edit","description":"Clear all per-element overrides on the targeted elements.","input_schema":{"type":"object","properties":{"element_ids":{"type":"array","items":{"type":"string"}},"element_query":{"type":"string"}}}},
{"name":"remove_stage","description":"Delete a stage/phase by id.","input_schema":{"type":"object","properties":{"stage_id":{"type":"string"}},"required":["stage_id"]}},
{"name":"rename_stage","description":"Rename a stage/phase.","input_schema":{"type":"object","properties":{"stage_id":{"type":"string"},"name":{"type":"string"}},"required":["stage_id","name"]}},
{"name":"set_stage_color","description":"Set a stage/phase colour (RGB 0..1).","input_schema":{"type":"object","properties":{"stage_id":{"type":"string"},"r":{"type":"number"},"g":{"type":"number"},"b":{"type":"number"}},"required":["stage_id","r","g","b"]}},
{"name":"play","description":"Start playback of the construction sequence.","input_schema":{"type":"object","properties":{}}},
{"name":"pause","description":"Pause playback.","input_schema":{"type":"object","properties":{}}},
{"name":"set_speed","description":"Set playback speed in construction days per real second.","input_schema":{"type":"object","properties":{"days_per_second":{"type":"number"}},"required":["days_per_second"]}},
{"name":"import_timeliner_csv","description":"Import a Navisworks TimeLiner schedule exported as CSV and rebuild it as Earth4D tasks, auto-linking elements whose metadata matches each task by the given match key.","input_schema":{"type":"object","properties":{"path":{"type":"string"},"match_key":{"type":"string"}},"required":["path","match_key"]}},
{"name":"align_task","description":"Align a task's start relative to another task. mode: after|before|start-with|end-with; gap in days.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"relative_to":{"type":"string"},"mode":{"type":"string","enum":["after","before","start-with","end-with"]},"gap":{"type":"number"}},"required":["task_id","relative_to"]}},
{"name":"sequence_tasks","description":"Chain a set of tasks back-to-back in the given order, each keeping its duration. Optional start day and gap between them.","input_schema":{"type":"object","properties":{"task_ids":{"type":"array","items":{"type":"string"}},"start":{"type":"number"},"gap":{"type":"number"}},"required":["task_ids"]}},
{"name":"set_project_start","description":"Set the project start date (ISO-8601, e.g. 2026-03-01), the day-0 anchor for the schedule.","input_schema":{"type":"object","properties":{"date":{"type":"string"}},"required":["date"]}},
{"name":"list_stages","description":"List the stages/phases (id, name).","input_schema":{"type":"object","properties":{}}},
{"name":"get_selection","description":"Get the ids of the currently selected elements.","input_schema":{"type":"object","properties":{}}},
{"name":"region_center","description":"Get the project region's geodetic origin (lat, lon, height).","input_schema":{"type":"object","properties":{}}},
{"name":"save_selection_set","description":"Save a named, reusable element selection. Targets element_ids/element_query, or the current selection if neither is given.","input_schema":{"type":"object","properties":{"name":{"type":"string"},"element_ids":{"type":"array","items":{"type":"string"}},"element_query":{"type":"string"}},"required":["name"]}},
{"name":"apply_selection_set","description":"Make a saved selection set the live selection (match by name or id).","input_schema":{"type":"object","properties":{"name":{"type":"string"}},"required":["name"]}},
{"name":"delete_selection_set","description":"Delete a saved selection set (by name or id).","input_schema":{"type":"object","properties":{"name":{"type":"string"}},"required":["name"]}},
{"name":"list_selection_sets","description":"List the saved selection sets.","input_schema":{"type":"object","properties":{}}},
{"name":"list_vehicle_types","description":"List the built-in vehicle/equipment catalog types that place_vehicle accepts.","input_schema":{"type":"object","properties":{}}},
{"name":"place_vehicle","description":"Place a catalog vehicle/equipment at a region-local ENU point (metres east/north/up) facing heading degrees. Returns its id.","input_schema":{"type":"object","properties":{"type":{"type":"string"},"east":{"type":"number"},"north":{"type":"number"},"up":{"type":"number"},"heading":{"type":"number"}},"required":["type","east","north"]}},
{"name":"list_vehicles","description":"List the placed vehicles as 'id | type | name'.","input_schema":{"type":"object","properties":{}}},
{"name":"remove_vehicle","description":"Remove a placed vehicle by id.","input_schema":{"type":"object","properties":{"vehicle_id":{"type":"string"}},"required":["vehicle_id"]}},
{"name":"set_vehicle_route","description":"Set a placed vehicle's multi-point route. path is an array of [east,north,up] (metres). Drives the route across the day window; loop=true for continuous.","input_schema":{"type":"object","properties":{"vehicle_id":{"type":"string"},"path":{"type":"array","items":{"type":"array","items":{"type":"number"}}},"start":{"type":"number"},"days":{"type":"number"},"loop":{"type":"boolean"}},"required":["vehicle_id","path"]}},
{"name":"create_traffic","description":"Spawn a flow of looping vehicles staggered along a shared path for continuous traffic. path is an array of [east,north,up] (metres).","input_schema":{"type":"object","properties":{"path":{"type":"array","items":{"type":"array","items":{"type":"number"}}},"count":{"type":"number"},"days":{"type":"number"},"type":{"type":"string"}},"required":["path"]}},
{"name":"export_region_glb","description":"Export the scheduled scene at the current day to a single GLB (editor only; needs the glTF Exporter plugin).","input_schema":{"type":"object","properties":{"path":{"type":"string"}}}},
{"name":"export_animated_glb","description":"Export the build as a GLB (editor only). For the full 4D animation, use play_film + Movie Render Queue.","input_schema":{"type":"object","properties":{"path":{"type":"string"}}}},
{"name":"export_per_task_glb","description":"Export one GLB per task (the model state at each task's completion) into a folder (editor only).","input_schema":{"type":"object","properties":{"folder":{"type":"string"}}}},
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

		// Optional numeric field with default (used across many tools below).
		auto Num = [&](const TCHAR* Key, double Default = 0.0) { return Args->HasField(Key) ? Args->GetNumberField(Key) : Default; };

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
		if (ToolName == TEXT("rename_task")) return Sub->RenameTask(Args->GetStringField(TEXT("task_id")), Args->GetStringField(TEXT("name"))).Message;
		if (ToolName == TEXT("set_task_stage")) return Sub->SetTaskStage(Args->GetStringField(TEXT("task_id")), Args->HasField(TEXT("stage_id")) ? Args->GetStringField(TEXT("stage_id")) : FString()).Message;
		if (ToolName == TEXT("set_task_sequence"))
		{
			const UEnum* E = StaticEnum<EEarth4DSequence>();
			const int64 V = E->GetValueByNameString(Args->GetStringField(TEXT("sequence")));
			return Sub->SetTaskSequence(Args->GetStringField(TEXT("task_id")), V == INDEX_NONE ? EEarth4DSequence::ByElevation : (EEarth4DSequence)V).Message;
		}
		if (ToolName == TEXT("set_task_distance")) return Sub->SetTaskDistance(Args->GetStringField(TEXT("task_id")), Args->GetNumberField(TEXT("distance"))).Message;
		if (ToolName == TEXT("set_task_overlap")) return Sub->SetTaskOverlap(Args->GetStringField(TEXT("task_id")), Args->GetNumberField(TEXT("overlap"))).Message;
		if (ToolName == TEXT("set_task_color"))
			return Sub->SetTaskColor(Args->GetStringField(TEXT("task_id")), FLinearColor(Args->GetNumberField(TEXT("r")), Args->GetNumberField(TEXT("g")), Args->GetNumberField(TEXT("b")))).Message;
		if (ToolName == TEXT("set_task_glow"))
		{
			const bool bEnable = Args->HasField(TEXT("enable")) ? Args->GetBoolField(TEXT("enable")) : true;
			const FLinearColor C(Args->HasField(TEXT("r")) ? Args->GetNumberField(TEXT("r")) : 0.2, Args->HasField(TEXT("g")) ? Args->GetNumberField(TEXT("g")) : 1.0, Args->HasField(TEXT("b")) ? Args->GetNumberField(TEXT("b")) : 0.3);
			return Sub->SetTaskGlowColor(Args->GetStringField(TEXT("task_id")), C, bEnable).Message;
		}
		if (ToolName == TEXT("add_stage")) { FString Id; return Sub->AddStage(Args->GetStringField(TEXT("name")), Id).Message; }
		if (ToolName == TEXT("remove_stage")) return Sub->RemoveStage(Args->GetStringField(TEXT("stage_id"))).Message;
		if (ToolName == TEXT("rename_stage")) return Sub->RenameStage(Args->GetStringField(TEXT("stage_id")), Args->GetStringField(TEXT("name"))).Message;
		if (ToolName == TEXT("set_stage_color"))
			return Sub->SetStageColor(Args->GetStringField(TEXT("stage_id")), FLinearColor(Args->GetNumberField(TEXT("r")), Args->GetNumberField(TEXT("g")), Args->GetNumberField(TEXT("b")))).Message;
		if (ToolName == TEXT("play")) { Sub->Play(); return TEXT("playing"); }
		if (ToolName == TEXT("pause")) { Sub->Pause(); return TEXT("paused"); }
		if (ToolName == TEXT("set_speed")) { Sub->SetSpeed(Args->GetNumberField(TEXT("days_per_second"))); return TEXT("ok"); }
		if (ToolName == TEXT("set_current_day")) { Sub->SetCurrentDay(Args->GetNumberField(TEXT("day"))); return TEXT("ok"); }

		if (ToolName == TEXT("select_elements"))
		{
			const bool bAdd = Args->HasField(TEXT("add")) ? Args->GetBoolField(TEXT("add")) : false;
			return Sub->SelectElements(ResolveTargets(Sub, Args), bAdd).Message;
		}
		if (ToolName == TEXT("set_element_edit") || ToolName == TEXT("set_element_color") || ToolName == TEXT("reset_element_edit"))
		{
			// Targets: explicit ids/query, else the current selection.
			TArray<FString> Ids = ResolveTargets(Sub, Args);
			if (Ids.Num() == 0) Ids = Sub->SelectedElementIds;
			if (Ids.Num() == 0) return TEXT("error: no target elements (give element_ids/element_query or select some first)");
			int32 N = 0;
			for (const FString& Id : Ids)
			{
				if (ToolName == TEXT("reset_element_edit")) { Sub->ResetElementEdit(Id); ++N; continue; }
				if (ToolName == TEXT("set_element_color"))
				{
					const bool bEnable = Args->HasField(TEXT("enable")) ? Args->GetBoolField(TEXT("enable")) : true;
					const FLinearColor C(Args->HasField(TEXT("r")) ? Args->GetNumberField(TEXT("r")) : 1.0, Args->HasField(TEXT("g")) ? Args->GetNumberField(TEXT("g")) : 1.0, Args->HasField(TEXT("b")) ? Args->GetNumberField(TEXT("b")) : 1.0);
					Sub->SetElementColor(Id, C, bEnable); ++N; continue;
				}
				// set_element_edit: start from the element's current edit and patch.
				const FEarth4DElement* El = Sub->Schedule ? Sub->Schedule->FindElement(Id) : nullptr;
				if (!El) continue;
				FEarth4DObjectEdit Edit = El->Edit;
				if (Args->HasField(TEXT("offset_e"))) Edit.Offset.X = Args->GetNumberField(TEXT("offset_e"));
				if (Args->HasField(TEXT("offset_n"))) Edit.Offset.Y = Args->GetNumberField(TEXT("offset_n"));
				if (Args->HasField(TEXT("offset_up"))) Edit.Offset.Z = Args->GetNumberField(TEXT("offset_up"));
				if (Args->HasField(TEXT("rotation_deg"))) Edit.RotationDeg = Args->GetNumberField(TEXT("rotation_deg"));
				if (Args->HasField(TEXT("scale"))) Edit.Scale = FMath::Max(0.001f, (float)Args->GetNumberField(TEXT("scale")));
				if (Args->HasField(TEXT("opacity"))) Edit.Opacity = Args->GetNumberField(TEXT("opacity"));
				if (Args->HasField(TEXT("hidden"))) Edit.bHidden = Args->GetBoolField(TEXT("hidden"));
				if (Args->HasField(TEXT("appear_day"))) Edit.AppearDay = Args->GetNumberField(TEXT("appear_day"));
				if (Args->HasField(TEXT("disappear_day"))) Edit.DisappearDay = Args->GetNumberField(TEXT("disappear_day"));
				if (Args->HasField(TEXT("stagger_delay"))) Edit.StaggerDelay = Args->GetNumberField(TEXT("stagger_delay"));
				if (Args->HasField(TEXT("override_style"))) { Edit.bOverrideStyle = true; Edit.OverrideStyle = ParseStyle(Args->GetStringField(TEXT("override_style"))); }
				if (Args->HasField(TEXT("override_direction"))) { Edit.bOverrideDirection = true; Edit.OverrideDirection = ParseDir(Args->GetStringField(TEXT("override_direction"))); }
				Sub->SetElementEdit(Id, Edit); ++N;
			}
			return FString::Printf(TEXT("Updated %d element(s)."), N);
		}

		if (ToolName == TEXT("import_timeliner_csv"))
		{
			// Routed through the Datasmith library helper (same as the editor button).
			return UEarth4DDatasmithLibrary::ImportTimeLinerCsv(Sub, Args->GetStringField(TEXT("path")), Args->GetStringField(TEXT("match_key"))).Message;
		}
		if (ToolName == TEXT("align_task"))
		{
			const FString Mode = Args->HasField(TEXT("mode")) ? Args->GetStringField(TEXT("mode")) : TEXT("after");
			const float Gap = Args->HasField(TEXT("gap")) ? Args->GetNumberField(TEXT("gap")) : 0.f;
			return Sub->AlignTask(Args->GetStringField(TEXT("task_id")), Args->GetStringField(TEXT("relative_to")), Mode, Gap).Message;
		}
		if (ToolName == TEXT("sequence_tasks"))
		{
			TArray<FString> Ids;
			const TArray<TSharedPtr<FJsonValue>>* Arr;
			if (Args->TryGetArrayField(TEXT("task_ids"), Arr)) for (const auto& V : *Arr) Ids.Add(V->AsString());
			const float Start = Args->HasField(TEXT("start")) ? Args->GetNumberField(TEXT("start")) : 0.f;
			const float Gap = Args->HasField(TEXT("gap")) ? Args->GetNumberField(TEXT("gap")) : 0.f;
			return Sub->SequenceTasks(Ids, Start, Gap).Message;
		}
		if (ToolName == TEXT("set_project_start")) return Sub->SetProjectStart(Args->GetStringField(TEXT("date"))).Message;
		if (ToolName == TEXT("list_stages"))
		{
			if (!Sub->Schedule) return TEXT("(no schedule)");
			TArray<FString> Lines;
			for (const FEarth4DStage& St : Sub->Schedule->Stages) Lines.Add(FString::Printf(TEXT("%s: %s"), *St.Id, *St.Name));
			return Lines.Num() ? FString::Join(Lines, TEXT("\n")) : TEXT("(no stages)");
		}
		if (ToolName == TEXT("get_selection"))
			return Sub->SelectedElementIds.Num() ? FString::Join(Sub->SelectedElementIds, TEXT(", ")) : TEXT("(nothing selected)");
		if (ToolName == TEXT("region_center"))
		{
			if (AEarth4DSite* Site = Sub->GetSite())
				return FString::Printf(TEXT("{\"lat\":%.7f,\"lon\":%.7f,\"height\":%.2f}"), Site->OriginLatitude, Site->OriginLongitude, Site->OriginHeight);
			return TEXT("error: no site bound (place an Earth4D Site actor)");
		}

		// ---- Selection sets ----
		if (ToolName == TEXT("save_selection_set"))
		{
			TArray<FString> Ids = ResolveTargets(Sub, Args);
			if (Ids.Num() == 0) Ids = Sub->SelectedElementIds;
			FString SetId;
			return Sub->SaveSelectionSet(Args->GetStringField(TEXT("name")), Ids, SetId).Message;
		}
		if (ToolName == TEXT("apply_selection_set")) return Sub->ApplySelectionSet(Args->GetStringField(TEXT("name"))).Message;
		if (ToolName == TEXT("delete_selection_set")) return Sub->DeleteSelectionSet(Args->GetStringField(TEXT("name"))).Message;
		if (ToolName == TEXT("list_selection_sets"))
		{
			const TArray<FString> S = Sub->ListSelectionSetNames();
			return S.Num() ? FString::Join(S, TEXT("\n")) : TEXT("(no selection sets)");
		}

		// ---- Vehicles / traffic ----
		// Parse a JSON path field (array of [east,north,up]) into region-local ENU vectors.
		auto ParsePath = [&](const TCHAR* Field) -> TArray<FVector>
		{
			TArray<FVector> Pts;
			const TArray<TSharedPtr<FJsonValue>>* Arr;
			if (Args->TryGetArrayField(Field, Arr))
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					const TArray<TSharedPtr<FJsonValue>>* P;
					if (V->TryGetArray(P) && P->Num() >= 2)
						Pts.Add(FVector((*P)[0]->AsNumber(), (*P)[1]->AsNumber(), P->Num() >= 3 ? (*P)[2]->AsNumber() : 0.0));
				}
			return Pts;
		};
		if (ToolName == TEXT("list_vehicle_types"))
		{
			const TArray<FString> T = Sub->ListVehicleTypes();
			return FString::Join(T, TEXT(", "));
		}
		if (ToolName == TEXT("place_vehicle"))
		{
			const FVector Enu(Num(TEXT("east")), Num(TEXT("north")), Num(TEXT("up")));
			FString Id;
			return Sub->PlaceVehicle(Args->GetStringField(TEXT("type")), Enu, Num(TEXT("heading")), Id).Message;
		}
		if (ToolName == TEXT("list_vehicles"))
		{
			const TArray<FString> V = Sub->ListVehicles();
			return V.Num() ? FString::Join(V, TEXT("\n")) : TEXT("(no vehicles)");
		}
		if (ToolName == TEXT("remove_vehicle")) return Sub->RemoveVehicle(Args->GetStringField(TEXT("vehicle_id"))).Message;
		if (ToolName == TEXT("set_vehicle_route"))
		{
			const bool bLoop = Args->HasField(TEXT("loop")) ? Args->GetBoolField(TEXT("loop")) : false;
			return Sub->SetVehicleRoute(Args->GetStringField(TEXT("vehicle_id")), ParsePath(TEXT("path")), Num(TEXT("start")), Num(TEXT("days"), 5.0), bLoop).Message;
		}
		if (ToolName == TEXT("create_traffic"))
		{
			int32 Spawned = 0;
			const int32 Count = Args->HasField(TEXT("count")) ? (int32)Args->GetNumberField(TEXT("count")) : 6;
			const FString Type = Args->HasField(TEXT("type")) ? Args->GetStringField(TEXT("type")) : TEXT("car");
			return Sub->CreateTraffic(ParsePath(TEXT("path")), Count, Num(TEXT("days"), 10.0), Type, Spawned).Message;
		}

		// ---- Export ----
		if (ToolName == TEXT("export_region_glb")) return Sub->ExportRegionGLB(Args->HasField(TEXT("path")) ? Args->GetStringField(TEXT("path")) : FString()).Message;
		if (ToolName == TEXT("export_animated_glb")) return Sub->ExportAnimatedGLB(Args->HasField(TEXT("path")) ? Args->GetStringField(TEXT("path")) : FString()).Message;
		if (ToolName == TEXT("export_per_task_glb"))
		{
			int32 Files = 0;
			return Sub->ExportPerTaskGLB(Args->HasField(TEXT("folder")) ? Args->GetStringField(TEXT("folder")) : FString(), Files).Message;
		}

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
