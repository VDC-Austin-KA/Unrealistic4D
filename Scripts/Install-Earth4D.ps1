# ============================================================================
#  Earth4D — Unreal Engine 5.8 plugin installer.
#
#  Deploys the Earth4D plugin either into the UE 5.8 engine (Engine\Plugins\
#  Marketplace\Earth4D — visible to every project) or into a specific project's
#  Plugins folder, then optionally compiles it with Scripts\Build.bat so the
#  binaries match the user's toolchain.
#
#  Called standalone, or by the 4D-pipeline meta-installer
#  (4d-construction-animation-tool/installer/install.ps1).
#
#  Usage (PowerShell, from anywhere):
#    ./Install-Earth4D.ps1                                  # engine auto-detect, project scope (Earth4DTemplate)
#    ./Install-Earth4D.ps1 -Scope engine                    # install into Engine\Plugins\Marketplace
#    ./Install-Earth4D.ps1 -ProjectDir "D:\Projects\Site4D" # into another project's Plugins\
#    ./Install-Earth4D.ps1 -EnginePath "D:\EPIC\UE_5.8"     # skip auto-detect
#    ./Install-Earth4D.ps1 -EnableCesium                    # opt-in Cesium (verified first; see below)
#    ./Install-Earth4D.ps1 -NoBuild                         # copy files only, don't compile
#    ./Install-Earth4D.ps1 -NonInteractive                  # no prompts (skips key entry)
#
#  Cesium: OFF by default (WITH_EARTH4D_CESIUM=0). Cesium for Unreal is not
#  released for UE 5.8; a stale install breaks the build. -EnableCesium only
#  takes effect if a CesiumForUnreal install whose .uplugin declares engine
#  5.8 compatibility is actually found — otherwise it is refused with a warning.
# ============================================================================
[CmdletBinding()]
param(
    [string]$EnginePath = "",
    [ValidateSet("engine", "project")]
    [string]$Scope = "project",
    [string]$ProjectDir = "",
    [switch]$EnableCesium,
    [switch]$NoBuild,
    [switch]$NonInteractive
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot   # Scripts\ -> repo root

function Write-Step($msg)  { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-Ok($msg)    { Write-Host "  [OK]   $msg" -ForegroundColor Green }
function Write-Warn2($msg) { Write-Host "  [WARN] $msg" -ForegroundColor Yellow }
function Fail($msg)        { Write-Host "  [FAIL] $msg" -ForegroundColor Red; exit 1 }

# ---------------------------------------------------------------------------
# 1. Locate UE 5.8 (param > registry > Epic Launcher manifest > default path).
#    Same resolution order as Scripts\Build.bat so both agree on the engine.
# ---------------------------------------------------------------------------
Write-Step "Locating Unreal Engine 5.8"

function Find-UE58 {
    param([string]$Hint)
    if ($Hint -and (Test-Path (Join-Path $Hint "Engine\Build\BatchFiles\Build.bat"))) { return $Hint }

    # Epic installer writes InstalledDirectory here for launcher-installed engines.
    foreach ($hive in @("HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8",
                        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\5.8")) {
        try {
            $v = (Get-ItemProperty -Path $hive -ErrorAction Stop).InstalledDirectory
            if ($v -and (Test-Path $v)) { return $v }
        } catch { }
    }

    # Epic Games Launcher manifest lists every managed engine/app install.
    $dat = Join-Path $env:ProgramData "Epic\UnrealEngineLauncher\LauncherInstalled.dat"
    if (Test-Path $dat) {
        try {
            $list = (Get-Content $dat -Raw | ConvertFrom-Json).InstallationList
            $hit = $list | Where-Object { $_.AppName -like "UE_5.8*" } | Select-Object -First 1
            if ($hit -and (Test-Path $hit.InstallLocation)) { return $hit.InstallLocation }
        } catch { }
    }

    # Known install locations for this team, then the Epic default.
    foreach ($c in @("D:\EPIC\UE_5.8", "C:\Program Files\Epic Games\UE_5.8")) {
        if (Test-Path (Join-Path $c "Engine\Build\BatchFiles\Build.bat")) { return $c }
    }
    return $null
}

$ue = Find-UE58 -Hint $EnginePath
if (-not $ue) {
    Fail "Unreal Engine 5.8 not found (registry, LauncherInstalled.dat, and default paths all missed). If the engine lives on a drive that isn't mounted (e.g. D:), mount it and re-run, or pass -EnginePath."
}
Write-Ok "Engine: $ue"

# ---------------------------------------------------------------------------
# 2. Prerequisite: .NET Framework 4.8 Developer Pack (NetFxSDK). Without it UBT
#    fails with 'Could not find NetFxSDK install dir' (module SwarmInterface).
# ---------------------------------------------------------------------------
Write-Step "Checking build prerequisites"
$netfx = Test-Path "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\NETFXSDK\4.8"
if ($netfx) { Write-Ok ".NET Framework 4.8 Developer Pack present" }
elseif (-not $NoBuild) {
    Fail ".NET Framework 4.8 Developer Pack (NetFxSDK) is missing — UBT will fail with 'Could not find NetFxSDK install dir'. Install ndp48-devpack-enu.exe (https://dotnet.microsoft.com/download/dotnet-framework/net48) and re-run, or pass -NoBuild to copy files without compiling."
} else {
    Write-Warn2 "NetFxSDK missing — files will be copied but the plugin cannot be compiled on this machine until ndp48-devpack-enu.exe is installed."
}

# The editor locks module DLLs; a copy/compile over a running editor fails.
$editorProc = Get-Process UnrealEditor -ErrorAction SilentlyContinue
if ($editorProc) {
    if ($NonInteractive) { Fail "UnrealEditor.exe is running and locks the plugin DLLs. Close it and re-run." }
    $ans = Read-Host "UnrealEditor.exe is running and locks the plugin DLLs. Close it now? [y/N]"
    if ($ans -match '^[Yy]') {
        Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue
        Stop-Process -Name CrashReportClient -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 3
    } else { Fail "Cannot install while the editor is running." }
}

# ---------------------------------------------------------------------------
# 3. Cesium gate. Opt-in only, and only when a CesiumForUnreal install that
#    declares UE 5.8 compatibility actually exists. A stale (older-engine)
#    Cesium install silently breaks the build, so we verify the .uplugin.
# ---------------------------------------------------------------------------
Write-Step "Checking Cesium for Unreal (optional)"
$cesiumOk = $false
$cesiumUplugins = @(
    (Join-Path $ue "Engine\Plugins\Marketplace\CesiumForUnreal\CesiumForUnreal.uplugin")
)
if ($ProjectDir) { $cesiumUplugins += (Join-Path $ProjectDir "Plugins\CesiumForUnreal\CesiumForUnreal.uplugin") }
foreach ($cu in $cesiumUplugins) {
    if (Test-Path $cu) {
        try {
            $meta = Get-Content $cu -Raw | ConvertFrom-Json
            if ($meta.EngineVersion -like "5.8*") {
                $cesiumOk = $true
                Write-Ok "CesiumForUnreal $($meta.VersionName) for engine $($meta.EngineVersion): $cu"
            } else {
                Write-Warn2 "CesiumForUnreal found at $cu but targets engine '$($meta.EngineVersion)', not 5.8 — ignoring it (a mismatched Cesium breaks the build)."
            }
        } catch { Write-Warn2 "Could not parse $cu — ignoring." }
    }
}
$useCesium = $false
if ($EnableCesium) {
    if ($cesiumOk) {
        $useCesium = $true
        Write-Ok "Cesium ENABLED for this build (EARTH4D_FORCE_CESIUM=1)."
    } else {
        Write-Warn2 "-EnableCesium requested but no UE 5.8-compatible CesiumForUnreal install was found. Keeping Cesium OFF (the Cesium-free Google tiles path is the default and needs no Cesium)."
    }
} elseif ($cesiumOk) {
    Write-Host "  A 5.8-compatible Cesium install exists. Re-run with -EnableCesium to build against it; default stays OFF." -ForegroundColor Gray
} else {
    Write-Ok "No compatible Cesium install — building the default Cesium-free path."
}

# ---------------------------------------------------------------------------
# 4. Deploy plugin files.
# ---------------------------------------------------------------------------
Write-Step "Deploying Earth4D plugin ($Scope scope)"
$pluginSrc = Join-Path $repoRoot "Earth4D"
if (-not (Test-Path (Join-Path $pluginSrc "Earth4D.uplugin"))) { Fail "Plugin source not found at $pluginSrc" }

$uproject = $null
if ($Scope -eq "engine") {
    $dest = Join-Path $ue "Engine\Plugins\Marketplace\Earth4D"
    Write-Host "  Target: $dest"
    try {
        New-Item -ItemType Directory -Force -Path $dest | Out-Null
    } catch {
        Fail "Cannot write to the engine folder ($dest). Re-run this script from an elevated (Administrator) PowerShell, or use -Scope project."
    }
    # Clean stale Binaries/Intermediate so the engine copy always recompiles fresh.
    foreach ($sub in @("Binaries", "Intermediate")) {
        $p = Join-Path $dest $sub
        if (Test-Path $p) { Remove-Item $p -Recurse -Force }
    }
    Copy-Item -Path (Join-Path $pluginSrc "*") -Destination $dest -Recurse -Force
    Write-Ok "Plugin copied into the engine (all projects can enable it)."
} else {
    if (-not $ProjectDir) {
        # Default target: the bundled Earth4DTemplate project (already wired up).
        $ProjectDir = Join-Path $repoRoot "Earth4DTemplate"
    }
    $uprojectFile = Get-ChildItem -Path $ProjectDir -Filter *.uproject -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $uprojectFile) { Fail "No .uproject found in $ProjectDir" }
    $uproject = $uprojectFile.FullName
    $dest = Join-Path $ProjectDir "Plugins\Earth4D"
    Write-Host "  Target: $dest"
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    if ((Resolve-Path $pluginSrc).Path -ne (Resolve-Path $dest -ErrorAction SilentlyContinue).Path) {
        Copy-Item -Path $pluginSrc -Destination $dest -Recurse -Force
    }
    Write-Ok "Plugin copied into project: $uproject"
}

# ---------------------------------------------------------------------------
# 5. First-run config: Earth4DTemplate\Config\DefaultEarth4D.ini (gitignored).
#    Holds the Google Map Tiles key, Anthropic key, and Cesium ion token.
# ---------------------------------------------------------------------------
Write-Step "First-run configuration (API keys)"
$templateCfgDir = Join-Path $repoRoot "Earth4DTemplate\Config"
if ($ProjectDir -and ($Scope -eq "project")) { $templateCfgDir = Join-Path $ProjectDir "Config" }
$iniPath = Join-Path $templateCfgDir "DefaultEarth4D.ini"

if (Test-Path $iniPath) {
    Write-Ok "Existing $iniPath left untouched."
} else {
    $google = ""; $claude = ""; $ion = ""
    if (-not $NonInteractive) {
        Write-Host "  Enter API keys (press Enter to skip any — they can be added later in"
        Write-Host "  Project Settings > Plugins > Earth4D, or by editing $iniPath):"
        $google = Read-Host "  Google Map Tiles API key"
        $claude = Read-Host "  Anthropic (Claude) API key"
        $ion    = Read-Host "  Cesium ion access token (optional)"
    }
    New-Item -ItemType Directory -Force -Path $templateCfgDir | Out-Null
    @"
; Earth4D per-project keys. This file is GITIGNORED — never commit keys.
; Section maps to UEarth4DSettings (config=Earth4D).
[/Script/Earth4DRuntime.Earth4DSettings]
GoogleMapTilesApiKey=$google
ClaudeApiKey=$claude
CesiumIonToken=$ion
"@ | Set-Content -Path $iniPath -Encoding UTF8
    Write-Ok "Created $iniPath $(if (-not $google -and -not $claude) { '(keys left blank — fill in before first run)' })"
}

# ---------------------------------------------------------------------------
# 6. Compile (verifies the environment and produces binaries for THIS machine).
# ---------------------------------------------------------------------------
if ($NoBuild) {
    Write-Step "Skipping compile (-NoBuild)"
} elseif ($Scope -eq "project" -and (Split-Path $uproject -Parent) -eq (Join-Path $repoRoot "Earth4DTemplate")) {
    Write-Step "Compiling via Scripts\Build.bat (editor closed build)"
    # Build.bat auto-detects the engine the same way we did; pass it explicitly
    # so both stages are guaranteed to use the same install.
    if ($useCesium) { $env:EARTH4D_FORCE_CESIUM = "1" } else { $env:EARTH4D_FORCE_CESIUM = $null }
    & cmd /c "`"$PSScriptRoot\Build.bat`" /close /nopause `"$ue`""
    if ($LASTEXITCODE -ne 0) { Fail "Build failed (exit $LASTEXITCODE). Search the output above for the FIRST line containing 'error:' or 'error C' — that is the cause." }
    Write-Ok "Earth4D compiled successfully."
} else {
    Write-Step "Compiling with UnrealBuildTool"
    if ($useCesium) { $env:EARTH4D_FORCE_CESIUM = "1" } else { $env:EARTH4D_FORCE_CESIUM = $null }
    $ubt = Join-Path $ue "Engine\Build\BatchFiles\Build.bat"
    if ($Scope -eq "engine") {
        # Engine-scope plugins are compiled per-project on first use; validate by
        # building the bundled template against the engine copy if present.
        $tpl = Join-Path $repoRoot "Earth4DTemplate\Earth4D.uproject"
        if (Test-Path $tpl) {
            & cmd /c "`"$ubt`" Earth4DTemplateEditor Win64 Development -project=`"$tpl`" -waitmutex"
            if ($LASTEXITCODE -ne 0) { Fail "Build failed (exit $LASTEXITCODE)." }
            Write-Ok "Engine plugin verified by compiling Earth4DTemplate."
        } else {
            Write-Warn2 "Template project not found — plugin copied; it will compile on first project load."
        }
    } else {
        $targetName = ([IO.Path]::GetFileNameWithoutExtension($uproject)) + "Editor"
        & cmd /c "`"$ubt`" $targetName Win64 Development -project=`"$uproject`" -waitmutex"
        if ($LASTEXITCODE -ne 0) { Fail "Build failed (exit $LASTEXITCODE)." }
        Write-Ok "Plugin compiled for $uproject."
    }
}

Write-Host ""
Write-Host "Earth4D installed." -ForegroundColor Green
Write-Host "  Scope   : $Scope"
Write-Host "  Engine  : $ue"
if ($uproject) { Write-Host "  Project : $uproject" }
Write-Host "  Keys    : $iniPath"
Write-Host "  Cesium  : $(if ($useCesium) { 'ENABLED (EARTH4D_FORCE_CESIUM=1)' } else { 'off (default Cesium-free tiles path)' })"
