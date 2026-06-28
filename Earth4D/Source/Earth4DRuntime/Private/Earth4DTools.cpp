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
{"name":"remove_elements","description":"Remove elements from a task.","input_schema":{"type":"object","properties":{"task_id":{"type":"string"},"element_ids":{"type":"array","items":{"type":"string"}},"element_query":{"type":"string"}},"required":["task_id"]}},
{"name":"add_stage","description":"Add a stage / phase.","input_schema":{"type":"object","properties":{"name":{"type":"string"}},"required":["name"]}},
{"name":"set_current_day","description":"Scrub the playback to a construction day.","input_schema":{"type":"object","properties":{"day":{"type":"number"}},"required":["day"]}},
{"name":"auto_assign_from_metadata","description":"Group elements by an imported metadata key (e.g. a Navisworks/Revit property or phase) and auto-create a task per group with the elements assigned.","input_schema":{"type":"object","properties":{"meta_key":{"type":"string"},"type":{"type":"string","enum":["Construction","Demolition"]},"days_per_task":{"type":"number"}},"required":["meta_key"]}},
{"name":"set_region_origin","description":"Re-origin the project region (and the Cesium georeference) to a geodetic point. Use this to set the construction site's real-world location by coordinates.","input_schema":{"type":"object","properties":{"lat":{"type":"number","description":"latitude degrees (WGS84)"},"lon":{"type":"number","description":"longitude degrees (WGS84)"},"height":{"type":"number","description":"ellipsoidal height in metres (optional)"}},"required":["lat","lon"]}},
{"name":"fly_to","description":"Move the active camera to view a geodetic point on the Google 3D Tiles.","input_schema":{"type":"object","properties":{"lat":{"type":"number"},"lon":{"type":"number"},"height":{"type":"number"},"view_distance_m":{"type":"number","description":"how far back to frame from, metres"}},"required":["lat","lon"]}},
{"name":"frame_region","description":"Move the active camera to frame the whole project region.","input_schema":{"type":"object","properties":{}}},
{"name":"geocode_goto","description":"Search for a place by name (e.g. 'Sydney Opera House') and move the project region + camera there. Async; result is reported back when found.","input_schema":{"type":"object","properties":{"query":{"type":"string"},"set_region_origin":{"type":"boolean","description":"also re-origin the region to the found place (default true)"}},"required":["query"]}}
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
