# Earth4D Web ↔ Unreal Plugin — Feature Parity Audit

Reference web app: **[VDC-Austin-KA/EarthEditor](https://github.com/VDC-Austin-KA/EarthEditor)**
(`src/` React/three.js tool). This repo (`Unrealistic4D`) is the UE 5.8 port and is
**ahead** of the `unreal/` snapshot bundled inside EarthEditor (it adds Site,
Vehicle, Excavation, Annotation, CameraDirector, GoogleTiles, MaterialApplier,
Settings, and the Timeline / Elements / Chat Slate panels).

The audit compares the web app's command surface — its MCP server
(`mcp-server/index.mjs`) and agent bridge (`src/agent/agentBridge.ts`), which are the
authoritative "every tool" list — against the UE command layer
(`Earth4DSubsystem` verbs + `Earth4DTools` tool schema). Every UE tool is shared
verbatim by the **UMG UI, in-app Claude chat, and the editor MCP server** (single
command layer), so closing a gap closes it on all three surfaces at once.

## Tool-for-tool mapping (web → UE)

| Web app tool | UE tool / verb | Status |
|---|---|---|
| `get_scene_summary` | `list_tasks` (`GetScheduleSummary`) | ✅ |
| `list_models` / `list_objects` | `find_elements` | ✅ (flat list) |
| `find_objects` | `find_elements` | ✅ |
| `get_selection` | `get_selection` | ✅ **added** |
| `select_objects` | `select_elements` | ✅ **added** |
| `edit_objects` | `set_element_edit` | ✅ **added** |
| `reset_objects` | `reset_element_edit` | ✅ **added** |
| (object colour) | `set_element_color` | ✅ **added** |
| `save_selection_set` | `save_selection_set` (+ apply/delete/list) | ✅ **added** |
| `create_task` | `add_task` | ✅ |
| `update_task` | `rename_task` + granular `set_task_*` | ✅ (granular) |
| `set_task_dates` | `set_task_window` | ✅ |
| `delete_task` | `remove_task` | ✅ |
| `assign_objects_to_task` | `assign_elements` | ✅ |
| `remove_objects_from_task` | `remove_elements` | ✅ |
| `align_task` | `align_task` | ✅ **added** |
| `sequence_tasks` | `sequence_tasks` | ✅ **added** |
| (task style) | `set_task_style` | ✅ |
| (task direction) | `set_task_direction` | ✅ |
| (task sequence) | `set_task_sequence` | ✅ **added** |
| (task distance) | `set_task_distance` | ✅ **added** |
| (task overlap) | `set_task_overlap` | ✅ **added** |
| (task stagger) | `set_task_stagger` | ✅ |
| (task colour / glow) | `set_task_color`, `set_task_glow` | ✅ **added** |
| (task type) | `set_task_type` | ✅ |
| (task stage) | `set_task_stage` | ✅ **added (tool)** |
| `create_stage` | `add_stage` | ✅ |
| `list_stages` | `list_stages` | ✅ **added** |
| (stage rename/colour/remove) | `rename_stage`, `set_stage_color`, `remove_stage` | ✅ **added** |
| `set_current_day` | `set_current_day` | ✅ |
| `set_playing` | `play` / `pause` | ✅ **added** |
| (speed) | `set_speed` | ✅ **added** |
| `set_project_start` | `set_project_start` | ✅ **added** |
| `region_center` | `region_center` | ✅ **added** |
| (geo search / fly) | `geocode_goto`, `fly_to`, `frame_region`, `set_region_origin` | ✅ (UE-native, richer) |
| `create_excavation` | `add_excavation` | ✅ |
| `place_vehicle` | `place_vehicle` (12-type catalog) + `add_vehicle` | ✅ **added** |
| `set_vehicle_route` / `list_vehicles` / `remove_vehicle` / `list_vehicle_types` | same names | ✅ **added** |
| `create_traffic` | `create_traffic` | ✅ **added** |
| `export_region_glb` / `export_animated_glb` / `export_per_task_glb` | same names (glTF Exporter, editor) | ✅ **added** |
| `go_to_tab` | n/a (single docked panel) | — |
| (import models) | `ingest_actor`, `import_selected`, `auto_assign_from_metadata`, `import_timeliner_csv` | ✅ (UE-native, richer: Datasmith + Navisworks) |
| (camera / film) | `add_camera_keyframe`, `play_film`, `clear_film` | ✅ (UE-native) |
| (annotations) | `add_annotation` | ✅ |
| (save/scenarios) | `save_project`, `load_project`, `save_scenario`, `load_scenario`, `list_scenarios` | ✅ (UE-native, richer) |

UE tool count: **69** (was 37). Every advertised tool has a dispatch handler
(verified) and the schema parses as valid JSON. **Every tool on the web app's
MCP/agent surface now has a UE equivalent.**

## What this pass added

**Model-content modification on every surface.** Previously chat/MCP could not
move, recolour, scale, hide, or re-time a model element — only the UMG editor could.
Added `set_element_edit` / `set_element_color` / `reset_element_edit` /
`select_elements` / `get_selection` so the "same level of modification" is available
by natural language too.

**Per-element animation override (CMBuilder/Fuzor parity).** `FEarth4DObjectEdit`
gained `bOverrideStyle/OverrideStyle` and `bOverrideDirection/OverrideDirection`;
the schedule evaluator honours them (an element animates with its own style/direction
instead of inheriting the task's). Exposed in the element editor (checkbox + combo)
and via `set_element_edit`. This makes ARCHITECTURE §3's claim true.

**Full task modification.** New verbs/tools `set_task_sequence`, `set_task_distance`,
`set_task_overlap`, `set_task_color`, `set_task_glow`, plus `align_task` /
`sequence_tasks` for dependency-style scheduling, and `set_project_start`. The Gantt
task inspector now exposes Sequence, Task colour, and Action-glow controls, and routes
Distance/Overlap through the command layer (was poking the struct directly).

**Stage + playback + query tools.** `rename_stage`, `set_stage_color`,
`remove_stage`, `list_stages`, `play`, `pause`, `set_speed`, `region_center`, and
`import_timeliner_csv` are now callable from chat/MCP.

**UI depth.** Task inspector: + Sequence, Stage-aware colour, Glow. Element editor:
+ Stagger-delay, + Style override, + Direction override.

## Second pass — remaining gaps now closed

- **Selection sets** ✅ `FEarth4DSelectionSet` on the schedule (saved/loaded with the
  project), verbs `SaveSelectionSet`/`ApplySelectionSet`/`DeleteSelectionSet`/
  `ListSelectionSetNames`, tools `save/apply/delete/list_selection_set(s)`, and a
  **Save set / Sets ▾** menu in the Models panel.
- **Vehicle catalog + routing + traffic** ✅ a 12-type construction-equipment catalog
  (`ListVehicleTypes`), `PlaceVehicle` by type at an ENU point + heading, a world
  vehicle registry (`ListVehicles`/`RemoveVehicle`), multi-point `SetVehicleRoute`,
  and `CreateTraffic` (N looping vehicles staggered along a path). Tools mirror the
  web names.
- **GLB export tools** ✅ `ExportRegionGLB` / `ExportAnimatedGLB` / `ExportPerTaskGLB`
  drive UE's **glTF Exporter** (editor) via `UExporter::RunAssetExportTask`; per-task
  scrubs to each task's completion day. Packaged builds return a clear "use Play Film
  + Movie Render Queue" message (UE records video natively rather than WebM-in-browser).

## Remaining (UI-only — not part of the web app's tool/MCP surface)

1. **Hierarchical model tree + grouping presets** — the web Models tab groups by
   hierarchy / layer / category / type / level / block; the UE Elements panel is a
   flat, searchable, multi-select list (with per-element editing + selection sets).
   Upgrade path: convert the `SListView` to an `STreeView` keyed on `Element.Path` /
   `Element.Meta`. No new tools required — purely a tree presentation of existing data.
2. **Media studio** — the web app's CapCut-style in-browser video editor (clips,
   layers, trim, annotation templates, WebM export). It is **UI-only in the web app —
   it exposes no agent/MCP tools** — so it is outside "all of the same tools". The
   native UE equivalent is **Sequencer + Movie Render Queue** (already reachable via the
   Film camera tools `add_camera_keyframe` / `play_film`), which is higher-fidelity
   than the web editor. A bespoke in-app media panel would be a large, separate effort.
