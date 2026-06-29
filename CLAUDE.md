# Earth4D (Unrealistic4D) — Claude working notes

UE 5.8 plugin (`Earth4D/`) + template project (`Earth4DTemplate/`) porting the
Earth4D web app into Unreal: 4D construction sequencing over Google Photorealistic
3D Tiles, driven by one shared command layer. Read `ARCHITECTURE.md` for the design
and `PARITY.md` for the web→UE feature/tool mapping.

## Project Context — anchor to the real reference first

- The parity reference is the **web app `VDC-Austin-KA/EarthEditor`** (`src/` +
  its `mcp-server/` / `agentBridge.ts` = the authoritative tool list). This repo is
  **ahead** of the `unreal/` snapshot bundled inside EarthEditor — never treat that
  snapshot as the target.
- **Before building any new feature or MCP server, confirm whether the user is
  pointing at an existing deployed app / repo, and build against ITS data and tool
  surface — do not invent unrelated local features.** When a request names "earth4d"
  / "the editor" / "the app," clone or read that reference and diff against it first,
  then confirm the plan, then implement. (Inventing a parallel local system instead
  of using the referenced one has been the single most expensive misunderstanding.)
- One command layer is the contract: `UEarth4DSubsystem` BlueprintCallable verbs,
  wrapped by `Earth4DTools::GetToolsJson`/`Dispatch`, shared verbatim by the UMG UI,
  in-app Claude chat, and the editor MCP server. Add a verb + a tool together so all
  three surfaces stay in lockstep.

## Response Style — stay under output limits

- Keep chat replies short. **Write large code / file content to disk with Write/Edit
  rather than printing it into the conversation**, then give a one-line summary per
  file. (Whole sessions have been lost to the output-token maximum on big dumps.)
- Prefer small, scoped edits over regenerating whole files. Don't echo full file
  contents back to verify — the tools error if an edit didn't apply.

## Build & Packaging — verify prerequisites BEFORE compiling

The build scripts in `Scripts/` already auto-detect the engine; the failures that
actually bite are environment prerequisites. Check these first (the `/package`
skill encodes the full preflight):

1. **Engine path.** Default install for this setup is `D:\EPIC\UE_5.8`. **The D:
   drive may be unmounted at session start — confirm it's mounted before building.**
   If auto-detect fails, pass the path: `Build.bat "D:\EPIC\UE_5.8"`.
2. **.NET Framework 4.8 Developer Pack (NetFxSDK).** Required beyond VS Build Tools.
   Without it UBT fails `RulesError: Could not find NetFxSDK install dir` (module
   `SwarmInterface`) and the editor refuses to launch. Verify the registry key
   `HKLM\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\NETFXSDK\4.8` exists; if not,
   install `ndp48-devpack-enu.exe`.
3. **Cesium is OFF by default** (`WITH_EARTH4D_CESIUM=0`). Do **not** enable it on
   UE 5.8 — it isn't released for 5.8 and a stale install breaks the build. Only set
   `EARTH4D_FORCE_CESIUM=1` (before generating project files) once Cesium ships for
   the engine version. `Target.cs` already pins `BuildSettingsVersion.V7`.
4. **Editor must be closed to compile** — it locks the module DLLs. Use
   `Build.bat /close` (or `BuildAndRun.bat`).

Working invocations:
- Compile (iterate): `Scripts\Build.bat /close` — then open the project.
- Compile + launch: `Scripts\BuildAndRun.bat`
- Clean rebuild: `Scripts\Rebuild.bat`
- Package standalone: `Scripts\Package.bat` → `Dist\Windows\` (or use **`/package`**).
- Raw UBT: `<engine>\Engine\Build\BatchFiles\Build.bat Earth4DTemplateEditor Win64 Development -project="<repo>\Earth4DTemplate\Earth4D.uproject" -waitmutex`

When a build fails, find the **first** line containing `error:` / `error C` — that's
the cause. Summarize long build logs; don't paste them wholesale.

## Bash / Shell Conventions

- Long-running commands (UBT builds, packaging, VHDX/archive decompression) need
  **generous timeouts set up front** (builds: 600000 ms) to avoid timeout-retry
  cycles.
- If `D:` (the engine drive) is unmounted, remount it before any build step.
- A PostToolUse hook runs `Scripts/validate_tools.py` after edits: it fast-checks the
  command layer (tool-schema JSON validity, tool↔handler 1:1, verb decl/def) so the
  compile-only bug classes don't survive to a multi-minute build.

## Keys & Git

- Keys (Google Map Tiles API, Cesium ion token, Anthropic API) live in the
  **gitignored** `Earth4DTemplate/Config/DefaultEarth4D.ini` via `UEarth4DSettings` —
  **never commit keys**.
- Workflow: per-phase PRs to the `VDC-Austin-KA/Unrealistic4D` remote; branch off
  `main`, push, open a PR. Commit only the files relevant to the change — leave
  unrelated working-tree tooling/config (`.hyperai/`, `.codex/`, editor `Config/`,
  template build artifacts) out of feature commits.
