# Earth4D for Unreal Engine 5.8 — Architecture & Plan

A port of the Earth4D web app (React/three.js 4D construction-sequencing tool over
Google Photorealistic 3D Tiles) into an **Unreal Engine 5.8 plugin + template
project**, with a **natural-language control** feature driven by Claude.

The end goal is a **standalone packaged program** the whole team can run — focused
on the **4D / construction-sequencing** intent (not photoreal rendering like
Twinmotion), with the same authoring tools plus chat-driven editing.

---

## 1. Decisions (locked)

| Area | Decision |
|------|----------|
| **UI** | Native **UMG/Slate** widgets (Models tree, 4D Gantt, Film/camera, element editor, chat). Packages cleanly into a standalone app. |
| **Geo / tiles** | **Cesium for Unreal** streaming **Google Photorealistic 3D Tiles** (native georeferencing via `CesiumGeoreference` + `Cesium3DTileset`). |
| **AI control** | A **single command layer** (the `UEarth4DSubsystem` API) is the only way the schedule is mutated. It is exposed to **(a)** an in-app Claude-API chat (priority) and **(b)** an editor MCP server. |
| **Delivery** | A reusable **plugin** (`Earth4D`) + a **template project** (`Earth4DTemplate`) preconfigured with Cesium + Google tiles. |

### Why a single command layer is the core idea
Everything that changes the project — a slider drag in UMG, a chat instruction, an
MCP call from Claude Desktop — funnels through the **same** `BlueprintCallable`
methods on `UEarth4DSubsystem`. That guarantees the UI, chat, and MCP can never
drift apart, and the "control with natural language" feature is just a thin
adapter that maps tool calls onto those methods.

```
  UMG widgets ─┐
  Chat (Claude API, tool-use) ─┼─► UEarth4DSubsystem (command layer) ─► UEarth4DSchedule (data)
  Editor MCP server ─┘                         │
                                               └─► applies animation to Cesium/world actors each tick
```

---

## 2. Module layout

```
unreal/
  Earth4D/                         ← the plugin
    Earth4D.uplugin
    Source/
      Earth4DRuntime/              ← runs in editor AND packaged build
        Earth4DTypes.{h}           data model: enums + USTRUCTs (Task, Stage, ObjectEdit, ObjectState)
        Earth4DSchedule.{h,cpp}    UObject "document": tasks/stages/element edits + save/load
        Earth4DSubsystem.{h,cpp}   UWorldSubsystem: owns active schedule, currentDay, tick→apply, the COMMAND API
        Earth4DScheduleEvaluator.{h,cpp}  port of evaluateSchedule/styleState (per-day object state)
        Earth4DTools.{h,cpp}       tool schema (JSON) + name→command dispatcher (shared by chat & MCP)
        Earth4DChatClient.{h,cpp}  Claude Messages API client (HTTP, tool-use loop)
        Earth4DDatasmithLibrary.{h,cpp}  Datasmith → elements+metadata; TimeLiner CSV → tasks (auto-link)
      Earth4DEditor/               ← editor-only
        Earth4DEditorModule.{h,cpp}  registers the dockable tab + menu
        SEarth4DGanttPanel.{h,cpp}   Slate 4D Gantt (authoring)
        Earth4DMcpServer.{h,cpp}     local MCP server exposing the same tools to Claude Desktop/Code
  Earth4DTemplate/                 ← the template project
    Earth4D.uproject
    Config/DefaultEngine.ini       Cesium + georeference + Google tiles token slot
    Config/DefaultGame.ini
```

`Earth4DRuntime` has no editor dependency so the standalone app ships with the full
4D system + chat. `Earth4DEditor` adds authoring UI and the MCP server for
edit-time use by Claude Desktop/Code.

---

## 3. Data model (mirrors the web app's `store.ts`)

All USTRUCTs in `Earth4DTypes.h`, held in `UEarth4DSchedule`:

- `FEarth4DObjectEdit` — per-element overrides (offset, rotation, scale, color,
  opacity, appear/disappear day, style/direction override, stagger, pivot).
- `FEarth4DTask` — `Id, Name, Type(Construction/Demolition/Temporary), Start, End,
  Style, Direction, Sequence, Overlap, Distance, Stagger, StageId, ObjectIds[]`.
- `FEarth4DStage` — `Id, Name, Color` (phases / layers).
- `FEarth4DElement` — a scheduled scene element: `Id, DisplayName, Actor/Component
  ref or tag, BaseTransform, BoundsCenter/Size`. Built from imported geometry,
  Datasmith/CAD imports, or tagged actors.
- `FEarth4DObjectState` — evaluator output: `bVisible, Offset, Scale, Spin,
  Opacity, ClipPlane` for a given day.

Element identity: in the web app every imported mesh got a stable id. In UE,
elements map to **scene components/actors** (e.g. from a Datasmith import or a
tagged Actor). The schedule stores stable element ids ↔ soft object references so
edits/tasks survive reload, same as the web app's id-preserving model.

---

## 4. The command API (`UEarth4DSubsystem`, all `BlueprintCallable`)

These are the verbs the UI, chat, and MCP share. Each returns a small result
struct (ok + message) so the chat can report back.

- Tasks: `AddTask`, `RemoveTask`, `SetTaskDuration`, `SetTaskWindow`,
  `SetTaskType`, `SetTaskStyle`, `SetTaskDirection`, `SetTaskStage`,
  `SetTaskStagger`, `RenameTask`.
- Assignment: `AssignElementsToTask`, `AddElementsToTask`,
  `RemoveElementsFromTask`, `SelectElements`, `SelectByName`.
- Elements: `SetElementEdit`, `ResetElementEdit`, `SetElementColor`.
- Stages: `AddStage`, `RemoveStage`, `RenameStage`, `SetStageColor`.
- Playback: `SetCurrentDay`, `Play`, `Pause`, `SetSpeed`.
- Query (read-only, for chat grounding): `GetScheduleSummary`, `ListTasks`,
  `ListElements`, `FindElements`.

`Tick` (or a timeline driver) calls `EvaluateAndApply(CurrentDay)` →
`Earth4DScheduleEvaluator::Evaluate` → writes each element's transform/visibility/
material params onto its component, exactly like `applyAnimationStates` in the web
viewer.

---

## 5. Natural-language control

### 5a. In-app chat → Claude API (priority; works in the standalone app)
`UEarth4DChatClient` (runtime):
1. Builds a request to the **Claude Messages API** with the **tool list** from
   `Earth4DTools` (each tool = one command-API verb, with a JSON-schema input).
2. Sends the user's message + a system prompt that includes a live
   `GetScheduleSummary()` so Claude is grounded in the current project.
3. On `tool_use` blocks, `Earth4DTools::Dispatch(name, argsJson, Subsystem)` calls
   the matching command method, returns a `tool_result`, and the loop continues
   until Claude returns a final text answer.
4. UI shows the conversation; every tool call has *already* mutated the project, so
   the user sees the Gantt/scene update live.

Auth: the API key is entered in-app (Project Settings / a secure slot), never
hard-coded. This is the path your team uses in the packaged program — **no Claude
Code/Desktop dependency**.

### 5b. Editor MCP server (authoring with Claude Desktop/Code)
`FEarth4DMcpServer` (editor) hosts a small local MCP endpoint advertising the
**same** `Earth4DTools` tool list; each call dispatches into the command API. This
lets Claude Desktop/Code drive the editor at authoring time.

> **UE 5.8 / MCP note:** rather than depend on a specific Epic "MCP plugin" (whose
> exact surface/availability we shouldn't assume), we **host our own** MCP server
> inside the plugin over the tool layer we already built. If Epic ships a
> first-party MCP host in 5.8, our tools register with it the same way — the
> command layer is the stable contract either way. Confirm the 5.8 MCP surface
> before wiring 5b; 5a (app chat) has no such dependency.

---

## 6. Feature parity map (web → UE)

| Web app feature | UE implementation |
|---|---|
| Google 3D Tiles + region framing | Cesium `Cesium3DTileset` (Google) + `CesiumGeoreference`; region = a georeferenced bounds actor |
| Location search | Geocode → set `CesiumGeoreference` origin (same Nominatim call, or Cesium ion geocoder) |
| Import GLB/NWC/RVT models | Datasmith / glTF / Interchange import → registered as `FEarth4DElement`s |
| Model tree / selection sets | UMG tree view over the element registry; saved sets |
| 4D Gantt (drag/stretch tasks) | `SEarth4DGanttPanel` (Slate) bound to the command API |
| Animation styles (rise/grow/wipe/…) | `Earth4DScheduleEvaluator` port; applied via component transforms + a masked/dither material for reveal/fade |
| Excavation (carve tiles) | Cesium polygon clipping (`CesiumPolygonRaster`/clipping) + extracted ground mesh |
| Vehicles + routes + traffic | Spline-driven actors; rig animation via UE AnimBP or sequencer |
| Film / camera keyframes | `LevelSequence` (Sequencer) tied to construction day; or a lightweight camera-path component |
| Annotations (tags/3D text) | `UWidgetComponent` (billboard tags w/ leader) + `TextRenderComponent` (3D text) |
| Project save/load + scenarios | `UEarth4DSchedule` as a SaveGame/asset; scenarios = named schedule snapshots |
| Export animated GLB | Native to UE: render via Sequencer/Movie Render Queue, or glTF export of the baked tracks |

---

## 6b. Datasmith import + Navisworks TimeLiner auto-wiring

**Datasmith is a first-class part of the plugin** — it's how Navisworks, Revit,
SketchUp, etc. come in *with their metadata*, which removes most of the manual
work the web app had to do.

`UEarth4DDatasmithLibrary` (runtime, `DatasmithContent` dependency):

1. **`BuildElementsFromActor(root)`** — walk a Datasmith-imported actor, register
   each mesh component as an `FEarth4DElement`, and copy its Datasmith metadata
   (`UDatasmithAssetUserData` + tags) into `Element.Meta` (Revit category/family,
   Navisworks item properties, phase, GUID, selection-set names, …).
2. **`AutoCreateTasksFromMetadata(key, type, days)`** — group elements by any
   metadata key (e.g. `Phase`) and create one task per group with the elements
   already assigned. Zero manual assignment.
3. **`ImportTimeLinerCsv(path, matchKey)`** — ingest a Navisworks **TimeLiner**
   schedule exported as CSV and rebuild it as Earth4D tasks (dates → day indices,
   task type construction/demolition), **auto-linking** each task to the elements
   whose metadata matches its attachment (selection set / search / task name).
   The only input required is the *match key* — the model→task links carry across.

> **Why CSV for TimeLiner:** the Datasmith Navisworks pipeline reliably carries
> *geometry + item properties*, but the TimeLiner *schedule* itself isn't part of
> the standard Datasmith scene. A TimeLiner CSV/XML export (or the Navisworks API)
> is the dependable source for task dates + attachments; we match it to the
> Datasmith-imported elements by a shared property. If a given export *does* embed
> a task property per element, `AutoCreateTasksFromMetadata` alone suffices.
>
> Future: a small Navisworks add-in could push TimeLiner JSON straight to the
> editor MCP endpoint so import is one click — same command layer, no CSV step.

This is exposed to the chat/MCP too (`auto_assign_from_metadata` tool), so the
user can say *"group everything by Phase into tasks"* in chat.

## 7. Phased roadmap

1. **Foundation (this scaffold).** Plugin + template, data model, command API,
   schedule evaluator, tool schema, chat-client skeleton, docs.
2. **Cesium base.** ✅ *(in progress)* Wire Google tiles + georeference;
   region/location search. Implemented: `AEarth4DSite` (find/spawn
   `ACesiumGeoreference` + Google `ACesium3DTileset`, region origin, geo↔UE +
   region-local-ENU↔UE conversions); subsystem `SetSite` + `EvaluateAndApply`
   uses the site for ENU→world placement; location verbs `SetRegionOrigin`,
   `FlyToLatLon`, `FrameRegion`, `GeocodeAndGoTo` (+ `set_region_origin`,
   `fly_to`, `frame_region`, `geocode_goto` tools). See §9.
3. **Elements + apply loop.** Import → element registry; tick applies per-day
   state to components (transform/visibility); a few core animation styles.
4. **Gantt + element editor (UMG).** Author tasks/stages/edits natively.
5. **In-app chat (5a).** Claude API tool-use loop end-to-end.
6. **Excavation, vehicles, annotations, film** — port feature-by-feature.
7. **Editor MCP server (5b)** + packaging the standalone app.

---

## 9. Cesium georeference ↔ Earth4D ENU (Phase 2)

`AEarth4DSite` (runtime) is the bridge between Earth4D's **region-local ENU metres**
(+X=east, +Y=north, +Z=up) and the **Cesium-georeferenced world**:

- **Anchor.** The site's region origin (lat/lon/height) is pushed onto the
  `ACesiumGeoreference` via `SetOriginLongitudeLatitudeHeight`. With Cesium's
  default *cartographic-origin* placement, that geodetic point maps to Unreal
  `(0,0,0)`, so the engine's local tangent plane there *is* the Earth4D region frame.
- **Handedness.** Unreal is left-handed; Cesium's local tangent helper is
  **East-South-Up (ESU)**. `AEarth4DSite` converts Earth4D **ENU** → ESU by
  negating north (`south = -north`) before applying
  `ComputeEastSouthUpToUnrealTransformation`, and back again for the inverse.
- **Placement.** `UEarth4DSubsystem::EvaluateAndApply` now asks the bound site to
  rotate each element's ENU animation offset into Unreal-world cm
  (`RegionLocalEnuOffsetToUnreal`). With **no site bound it falls back** to the
  original naive `metres × 100` path, so the 4D core still runs without Cesium.
- **Tiles source.** Google Photorealistic 3D Tiles via either a **Cesium ion**
  token (asset `2275207`) or a **direct Google Map Tiles API key**
  (`tile.googleapis.com/v1/3dtiles/root.json?key=…`), read from
  `[Earth4D.Tiles]` in `DefaultEngine.ini` — never hard-coded.
- **Location search.** `GeocodeAndGoTo` uses the same **Nominatim** call as the web
  app (free, no key; sends a required `User-Agent`), then re-origins + flies.

> **`// VERIFY:` spots** (exact Cesium for Unreal API names can drift by version):
> `SetOriginLongitudeLatitudeHeight`, `TransformLongitudeLatitudeHeightPositionToUnreal`,
> `TransformUnrealPositionToLongitudeLatitudeHeight`,
> `ComputeEastSouthUpToUnrealTransformation`, `SetGeoreference`, `SetTilesetSource` /
> `ETilesetSource` / `SetIonAssetID` / `SetIonAccessToken` / `SetUrl`. Confirm these
> against the installed Cesium version when the project first compiles.

## 8. Honest caveats

- **Cannot compile/test Unreal here.** This scaffold targets UE 5.8 conventions
  (UCLASS/USTRUCT/Build.cs/.uplugin/.uproject) but must be opened/built in the
  editor on your machine; expect to resolve module/include details there.
- **Cesium for Unreal** is a required marketplace/Epic-store plugin; the template
  references it and needs a Google Map Tiles API key (or Cesium ion token).
- **MCP in UE 5.8** — see §5b; we host our own server over the tool layer rather
  than assume Epic's exact MCP API. Verify before relying on a first-party host.
- The web app's three.js specifics (stencil tile carving, custom shaders) don't
  port 1:1; UE equivalents (Cesium clipping, material masks, Niagara) are noted
  per-feature above and will need their own implementation passes.
