# Earth4D for Unreal Engine 5.8

An Unreal Engine **plugin** (`Earth4D/`) + **template project** (`Earth4DTemplate/`)
that ports the Earth4D web tool into UE 5.8: 4D construction sequencing over
**Google Photorealistic 3D Tiles** (via Cesium), with **natural-language control**
powered by Claude. Focused on the **4D / phasing** intent — not photoreal rendering.

> Read **[ARCHITECTURE.md](./ARCHITECTURE.md)** for the full design, the web→UE
> feature map, and the phased roadmap. This README is the quick-start.

## What's here (Phase 1 scaffold)

```
Earth4D/                     the plugin
  Earth4D.uplugin            (depends on Cesium for Unreal, HTTP, Json)
  Source/Earth4DRuntime/     data model · schedule evaluator · COMMAND LAYER ·
                             tool schema · Claude chat client · Datasmith ingest
  Source/Earth4DEditor/      dockable panel (Slate stub) · local MCP server
Earth4DTemplate/             content-only UE project preconfigured with the plugin,
                             Cesium and Datasmith
```

### The one idea that ties it together
Everything that changes the schedule goes through **`UEarth4DSubsystem`** (the
command layer). The **UMG UI**, the **in-app Claude chat**, and the **editor MCP
server** all call the same `BlueprintCallable` verbs — so they can never drift.
"Control with natural language" is just a thin adapter (`Earth4DTools`) mapping
tool calls onto those verbs.

## Setup

1. **Install Cesium for Unreal** (Epic Games store / Marketplace) for UE 5.8.
2. Copy the `Earth4D/` plugin into your project's `Plugins/` (or open the
   `Earth4DTemplate` project, which references it).
3. Open `Earth4DTemplate/Earth4D.uproject` in **UE 5.8**; let it compile the plugin.
4. **Set your tiles key** (don't commit it). In `Config/DefaultEngine.ini` under
   `[Earth4D.Tiles]` set **either** `GoogleMapTilesApiKey=…` (direct Google Map
   Tiles API key) **or** `CesiumIonToken=…` (Cesium ion access token; the site then
   uses ion asset `2275207` — set the site's `bUseCesiumIon=true`).
5. **Drop an `Earth4D Site` actor** into the level (Place Actors → search
   "Earth4D Site"). On play (and on edit-time construction if a georeference already
   exists) it will:
   - find-or-spawn a **`CesiumGeoreference`** + a Google **`Cesium3DTileset`**,
   - re-origin the georeference to its **Region Origin** (set `OriginLatitude` /
     `OriginLongitude` / `OriginHeight` on the actor, or call `set_region_origin` /
     `geocode_goto` from chat), and
   - bind itself to `UEarth4DSubsystem` so scheduled elements sit correctly on the
     tiles (region-local ENU metres → georeferenced world).
6. Window → **Earth4D** opens the authoring panel.

> The site is the only thing you place by hand for geo. Everything else — setting
> the region center, flying to a place, framing the region — is driven by the
> command verbs (and therefore by chat / MCP): `set_region_origin`, `fly_to`,
> `frame_region`, `geocode_goto`.

## Importing models (Datasmith) + Navisworks TimeLiner

The plugin treats **Datasmith** as the primary import path so Navisworks / Revit /
SketchUp geometry **and metadata** come in directly:

1. Import via Datasmith (File → Import, or Datasmith Runtime in the packaged app).
2. `UEarth4DDatasmithLibrary::BuildElementsFromActor` registers each mesh as an
   Earth4D **element**, copying its metadata (Revit category, Navisworks
   properties, phase, GUID, selection-set names…).
3. Then either:
   - **`AutoCreateTasksFromMetadata("Phase", …)`** — one task per metadata group,
     elements auto-assigned; or
   - **`ImportTimeLinerCsv(path, matchKey)`** — rebuild a Navisworks **TimeLiner**
     schedule from its CSV export as Earth4D tasks, auto-linking elements by a
     shared property. Only the *linking key* needs choosing — no manual reassign.

## Natural-language control

- **In-app chat (standalone app):** `UEarth4DChatClient` calls the **Claude Messages
  API** with the tool schema and runs the tool-use loop against the command layer.
  Enter your API key in-app (stored per-user). No Claude Code/Desktop needed.
- **Editor (authoring):** `FEarth4DMcpServer` exposes the same tools over a local
  endpoint for Claude Desktop/Code. (See ARCHITECTURE §5b on the UE 5.8 MCP surface.)

Example prompts once a project is loaded:
- "Make the steel erection task 12 days and start it on day 20."
- "Assign everything on level 3 to the slab-pour task and stagger it 0.5 days."
- "Import the TimeLiner CSV and switch the demolition phase to a top-down wipe."

## Cesium is optional on UE 5.8

Cesium for Unreal may not yet ship for a brand-new engine version. The plugin no
longer hard-depends on it:

- **Cesium installed** → streams Google Photorealistic 3D Tiles via
  `ACesium3DTileset` and uses Cesium's georeference (the full-fidelity path).
- **Cesium absent** → `AEarth4DSite` computes its **own WGS84 ENU georeference**
  (geodetic ↔ ECEF ↔ ENU ↔ Unreal), so element placement, `fly_to`, `frame_region`
  and `geocode_goto` all work. `AEarth4DGoogleTiles` connects to the **Google Map
  Tiles API** directly (key from settings) for the basemap; full native 3D-Tiles
  *mesh* rendering needs a Draco/glTF runtime decoder (a clearly-marked hook), so
  until Cesium supports the engine version the native path provides georeferencing +
  connectivity rather than streamed photoreal meshes.

Detection is automatic (`Earth4DRuntime.Build.cs`); force it with the
`EARTH4D_FORCE_CESIUM` / `EARTH4D_FORCE_NOCESIUM` environment variables.

### Keys (never commit them)
Set them in **Project Settings → Plugins → Earth4D** (saved to the gitignored
`Config/DefaultEarth4D.ini`) or type the Claude key into the in-app chat:
- **Google Map Tiles API key** (basemap, both paths), and/or **Cesium ion token**.
- **Anthropic API key** (in-app chat).

## Packaging the standalone app

The template is a **C++ code project** (`Source/Earth4DTemplate`), so it packages
into the standalone program the team runs:

1. Open `Earth4DTemplate/Earth4D.uproject` in UE 5.8 (let it compile).
2. **Platforms → Windows → Package Project** (or `RunUAT BuildCookRun`).
3. The packaged app ships `Earth4DRuntime` (4D core + command layer + in-app Claude
   chat). The editor-only authoring panel + MCP server are not included in the
   shipped build by design.

## Natural-language control surfaces

- **In-app chat** (runtime, packaged app): `SEarth4DChatPanel` → `UEarth4DChatClient`
  → Claude Messages API tool-use loop over the command layer.
- **Editor MCP server** (`FEarth4DMcpServer`): MCP JSON-RPC 2.0 at
  `http://127.0.0.1:<port>/mcp` (`initialize` / `tools/list` / `tools/call`) plus
  convenience `/tools` + `/call` routes, advertising the **same** `Earth4DTools`
  list. If UE 5.8's first-party `ModelContextProtocol` host is used instead, it
  registers the same tools — the command layer is the constant.

## Status & caveats

- **Phases 1–7 implemented**: data model + evaluator + command layer; Cesium base +
  WGS84 fallback; import → elements + material apply loop; the interactive Slate
  Gantt + element editor; in-app Claude chat; excavation / vehicles / annotations /
  film; MCP JSON-RPC server; save/load + scenarios; packageable template.
- **Not compiled here** — authored to UE 5.8 conventions; build in-editor and
  resolve any module/include specifics there. Cesium API call sites are marked
  `VERIFY:` (names can drift by version).
- Datasmith's richest metadata (and TimeLiner specifics) vary by exporter; the
  ingest is robust + configurable but verify against your real Navisworks export.
