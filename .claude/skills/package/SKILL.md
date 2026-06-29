---
name: package
description: Build and package the Earth4D UE 5.8 plugin into a standalone Windows distributable. Runs a prerequisite preflight (engine path, NetFxSDK, Cesium state, editor closed) BEFORE compiling, then an iterate-build, then RunUAT BuildCookRun, reporting pass/fail per step. Use when the user asks to package, build the distributable, make the standalone app, or produce a shippable Earth4D.exe.
---

# /package — Earth4D build & package

Goal: produce `Dist\Windows\` (zip → coworkers run `Earth4D.exe`). Packaging here has
historically devolved into cascading config failures; this skill front-loads the
prerequisite checks so it fails fast with a clear message instead of mid-cook.

Run each step in order. **Stop and report at the first failing step** — don't push
past a failed preflight into a 10-minute cook. Keep output concise: summarize logs,
don't paste them.

## Step 1 — Preflight (cheap checks, fail fast)

Run these and report a one-line PASS/FAIL for each:

1. **Engine present.** Default `D:\EPIC\UE_5.8` (this setup) or
   `C:\Program Files\Epic Games\UE_5.8`. The **D: drive may be unmounted** — check
   `Test-Path "D:\EPIC\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat"`. If neither path
   exists, ask the user for the engine path and pass it to the scripts as arg 1.
2. **NetFxSDK** (.NET Framework 4.8 Developer Pack). Check the registry key:
   `Test-Path "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\NETFXSDK\4.8"`.
   FAIL → tell the user to install `ndp48-devpack-enu.exe` (UBT will otherwise fail
   `RulesError: Could not find NetFxSDK install dir`). Do not proceed.
3. **Cesium is OFF.** Confirm `WITH_EARTH4D_CESIUM` is not forced on:
   `EARTH4D_FORCE_CESIUM` env var should be unset/!=1 (Cesium isn't released for 5.8;
   forcing it breaks the build). Just warn if it's set.
4. **Editor closed.** `tasklist | find "UnrealEditor.exe"` — it locks module DLLs.
   The build scripts auto-close with `/close`, so this is informational.
5. **Build config sanity.** `Earth4DTemplate/Source/*.Target.cs` should have
   `DefaultBuildSettings = BuildSettingsVersion.V7`. (Already set; flag if changed.)

## Step 2 — Iterate-build first (catches C++ errors fast)

A full package cook is slow; compile the editor modules first so any C++ error
surfaces in ~1 min instead of after cooking:

```
Scripts\Build.bat /close /nopause
```

(Pass the engine path as the last arg if Step 1 needed it, e.g.
`Scripts\Build.bat /close /nopause "D:\EPIC\UE_5.8"`.)

On failure: find the **first** line with `error:` / `error C` — report that line as
the cause and stop. Do not run Step 3.

## Step 3 — Package

Set a generous timeout (≥ 600000 ms — cook is long):

```
Scripts\Package.bat "<engine path if non-default>"
```

This runs `RunUAT BuildCookRun -platform=Win64 -clientconfig=Shipping -build -cook
-stage -pak -archive` to `Dist\`. On failure, report the first error line and which
phase (build / cook / stage / pak) it died in.

## Step 4 — Report

On success: report the output path `Dist\Windows\` and that zipping + sharing it lets
coworkers run `Earth4D.exe` (no Unreal needed). Note the shipped build contains
`Earth4DRuntime` (4D core + command layer + in-app chat); the editor authoring panel
+ MCP server are editor-only by design.

> Note: GLB export tools are editor-only (UE glTF Exporter). The packaged app records
> the 4D animation via the Film camera + Movie Render Queue, not browser WebM.
