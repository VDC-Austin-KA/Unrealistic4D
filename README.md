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

## Status & caveats

- **Phase 1 scaffold** — data model, evaluator, command layer, tools, chat client,
  Datasmith ingest, MCP server, and a stub authoring panel are in.
- **Phase 2 (Cesium base)** — `AEarth4DSite` ties the region-local ENU frame to a
  Cesium georeference + Google 3D Tiles; `SetRegionOrigin` / `FlyToLatLon` /
  `FrameRegion` / `GeocodeAndGoTo` location verbs + tools are in (ARCHITECTURE §9).
  The interactive Gantt/element-editor UMG, excavation/vehicles/film, and packaging
  are the next phases (ARCHITECTURE §7).
- **Not compiled here** — authored to UE 5.8 conventions but must be built in the
  editor on your machine; resolve any module/include specifics there.
- **Cesium** and a **Google tiles key/Cesium ion token** are required at runtime.
- Datasmith's richest metadata (and TimeLiner specifics) vary by exporter; the
  ingest is robust + configurable but verify against your real Navisworks export.
