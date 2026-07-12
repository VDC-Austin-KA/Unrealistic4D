# ============================================================================
#  Earth4D — cleanup of broken / stale plugin installs.
#
#  Finds every Earth4D plugin copy visible to UE 5.8 (engine Marketplace +
#  engine Plugins + project Plugins folders) and removes the ones that can
#  break editor startup:
#
#    * copies whose .uplugin declares a DIFFERENT engine version (e.g. a 5.7
#      build left behind) -> "not made for the current version" on launch,
#      and the in-editor rebuild loops forever because source and engine
#      disagree;
#    * copies with stale Binaries/Intermediate compiled against another
#      engine -> "modules are missing or built with a different engine
#      version ... rebuild failed".
#
#  Nothing is hard-deleted by default: removed plugins are MOVED to a
#  timestamped quarantine folder ($env:TEMP\Earth4D-cleanup-<stamp>) so any
#  mistake is recoverable. Re-run the installer afterwards to deploy a fresh
#  5.8 copy.
#
#  Usage:
#    ./Cleanup-Earth4D.ps1                          # report + confirm, then quarantine
#    ./Cleanup-Earth4D.ps1 -DryRun                  # report only, touch nothing
#    ./Cleanup-Earth4D.ps1 -Force                   # no confirmation prompt
#    ./Cleanup-Earth4D.ps1 -All                     # also remove version-MATCHED copies
#                                                   # (full reset before a clean reinstall)
#    ./Cleanup-Earth4D.ps1 -ProjectDirs "D:\P\A","D:\P\B"   # extra projects to scan
#    ./Cleanup-Earth4D.ps1 -EnginePath "D:\EPIC\UE_5.8"     # skip engine auto-detect
#    ./Cleanup-Earth4D.ps1 -NoBackup                # hard delete instead of quarantine
# ============================================================================
[CmdletBinding()]
param(
    [string]$EnginePath = "",
    [string[]]$ProjectDirs = @(),
    [string]$TargetEngineVersion = "5.8",
    [switch]$All,
    [switch]$DryRun,
    [switch]$Force,
    [switch]$NoBackup
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot

# powershell.exe -File passes arrays as one comma-joined string; split them back.
$ProjectDirs = @($ProjectDirs | ForEach-Object { $_ -split ',' } | Where-Object { $_ })

function Write-Step($msg)  { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-Ok($msg)    { Write-Host "  [OK]   $msg" -ForegroundColor Green }
function Write-Warn2($msg) { Write-Host "  [WARN] $msg" -ForegroundColor Yellow }
function Write-Info($msg)  { Write-Host "  $msg" -ForegroundColor Gray }

# Plugin identities this pipeline owns. Only these are ever touched — the
# script must never remove someone else's plugin.
$ownedNames = @("Earth4D", "EarthEditor", "Earth4DRuntime", "Earth4DEditor")

# ---------------------------------------------------------------------------
# Locate UE 5.8 (same order as Build.bat / Install-Earth4D.ps1).
# ---------------------------------------------------------------------------
Write-Step "Locating Unreal Engine $TargetEngineVersion"
$ue = $null
if ($EnginePath -and (Test-Path (Join-Path $EnginePath "Engine"))) { $ue = $EnginePath }
if (-not $ue -and ($env:OS -eq "Windows_NT")) {
    foreach ($hive in @("HKLM:\SOFTWARE\EpicGames\Unreal Engine\$TargetEngineVersion",
                        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$TargetEngineVersion")) {
        try {
            $v = (Get-ItemProperty -Path $hive -ErrorAction Stop).InstalledDirectory
            if ($v -and (Test-Path $v)) { $ue = $v; break }
        } catch { }
    }
    if (-not $ue) {
        $dat = Join-Path $env:ProgramData "Epic\UnrealEngineLauncher\LauncherInstalled.dat"
        if (Test-Path $dat) {
            try {
                $hit = (Get-Content $dat -Raw | ConvertFrom-Json).InstallationList |
                       Where-Object { $_.AppName -like "UE_$TargetEngineVersion*" } | Select-Object -First 1
                if ($hit -and (Test-Path $hit.InstallLocation)) { $ue = $hit.InstallLocation }
            } catch { }
        }
    }
    if (-not $ue) {
        foreach ($c in @("D:\EPIC\UE_$TargetEngineVersion", "C:\Program Files\Epic Games\UE_$TargetEngineVersion")) {
            if (Test-Path (Join-Path $c "Engine")) { $ue = $c; break }
        }
    }
}
if ($ue) { Write-Ok "Engine: $ue" } else { Write-Warn2 "Engine not found — only project folders will be scanned." }

# The editor locks plugin DLLs; nothing can be moved while it runs.
if (-not $DryRun) {
    $editorProc = Get-Process UnrealEditor -ErrorAction SilentlyContinue
    if ($editorProc) {
        if (-not $Force) {
            $ans = Read-Host "UnrealEditor.exe is running and locks plugin files. Close it now? [y/N]"
            if ($ans -notmatch '^[Yy]') { Write-Host "Aborted — close the editor and re-run."; exit 1 }
        }
        Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue
        Stop-Process -Name CrashReportClient -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 3
    }
}

# ---------------------------------------------------------------------------
# Scan for Earth4D plugin copies.
# ---------------------------------------------------------------------------
Write-Step "Scanning for Earth4D plugin copies"

$scanRoots = @()
if ($ue) {
    $scanRoots += Join-Path $ue "Engine/Plugins/Marketplace"
    $scanRoots += Join-Path $ue "Engine/Plugins"
}
# The bundled template ships its own copy — a healthy one; still scanned so
# a stale-binaries state there gets fixed too.
$defaultProjects = @((Join-Path $repoRoot "Earth4DTemplate"))
foreach ($p in ($defaultProjects + $ProjectDirs)) {
    if ($p -and (Test-Path $p)) { $scanRoots += Join-Path $p "Plugins" }
}
$scanRoots = $scanRoots | Where-Object { Test-Path $_ } | Select-Object -Unique

$findings = @()   # objects: Dir, Uplugin, Name, EngineVer, Verdict, Reason
foreach ($root in $scanRoots) {
    # Depth 2 covers Plugins\<Name>\<Name>.uplugin and Marketplace\<Name>\...
    $uplugins = Get-ChildItem -Path $root -Filter *.uplugin -Recurse -Depth 2 -ErrorAction SilentlyContinue
    foreach ($up in $uplugins) {
        $baseName = [IO.Path]::GetFileNameWithoutExtension($up.Name)
        $friendly = $null; $engVer = $null
        try {
            $meta = Get-Content $up.FullName -Raw | ConvertFrom-Json
            $friendly = $meta.FriendlyName
            $engVer = $meta.EngineVersion
        } catch { }
        $isOurs = ($ownedNames -contains $baseName) -or ($ownedNames -contains $friendly)
        if (-not $isOurs) { continue }

        $pluginDir = $up.Directory.FullName
        $binaries = Join-Path $pluginDir "Binaries"
        $intermediate = Join-Path $pluginDir "Intermediate"
        $hasArtifacts = (Test-Path $binaries) -or (Test-Path $intermediate)

        $verMatches = $engVer -and ($engVer -like "$TargetEngineVersion*")
        if (-not $engVer) { $verMatches = $false }

        if ($All) {
            $verdict = "REMOVE"; $reason = "full reset requested (-All)"
        } elseif (-not $verMatches) {
            $verdict = "REMOVE"
            $reason = "declares engine '$engVer' (expected $TargetEngineVersion.x) — this is the copy that breaks editor startup"
        } elseif ($hasArtifacts) {
            $verdict = "CLEAN-ARTIFACTS"
            $reason = "version matches but stale Binaries/Intermediate can still fail the module load"
        } else {
            $verdict = "KEEP"; $reason = "matches $TargetEngineVersion, no stale artifacts"
        }

        $findings += [pscustomobject]@{
            Dir = $pluginDir; Uplugin = $up.FullName; Name = $baseName
            EngineVer = $engVer; Verdict = $verdict; Reason = $reason
        }
    }
}

# Engine/Plugins and Engine/Plugins/Marketplace overlap — keep one finding per dir.
$findings = @($findings | Group-Object Dir | ForEach-Object { $_.Group[0] })

if ($findings.Count -eq 0) {
    Write-Ok "No Earth4D plugin copies found under: $($scanRoots -join '; ')"
    exit 0
}

foreach ($f in $findings) {
    $color = switch ($f.Verdict) { "REMOVE" { "Red" } "CLEAN-ARTIFACTS" { "Yellow" } default { "Green" } }
    Write-Host ("  [{0}] {1}" -f $f.Verdict, $f.Dir) -ForegroundColor $color
    Write-Info "      engine=$($f.EngineVer)  $($f.Reason)"
}

$actionable = @($findings | Where-Object { $_.Verdict -ne "KEEP" })
if ($actionable.Count -eq 0) { Write-Ok "Nothing to clean."; exit 0 }
if ($DryRun) { Write-Host "`nDry run — nothing was changed." -ForegroundColor Cyan; exit 0 }

if (-not $Force) {
    $ans = Read-Host "`nApply the actions above? Removed folders go to a recoverable quarantine. [y/N]"
    if ($ans -notmatch '^[Yy]') { Write-Host "Aborted."; exit 1 }
}

# ---------------------------------------------------------------------------
# Apply: quarantine (move) or delete.
# ---------------------------------------------------------------------------
$tempRoot = if ($env:TEMP) { $env:TEMP } else { [IO.Path]::GetTempPath() }
$quarantine = Join-Path $tempRoot ("Earth4D-cleanup-" + (Get-Date -Format "yyyyMMdd-HHmmss"))

function Remove-OrQuarantine([string]$Path) {
    if (-not (Test-Path $Path)) { return }
    if ($NoBackup) {
        Remove-Item -Path $Path -Recurse -Force
        Write-Ok "Deleted: $Path"
    } else {
        New-Item -ItemType Directory -Force -Path $quarantine | Out-Null
        # Keep enough of the source path to disambiguate same-named plugins.
        $leaf = ($Path -replace '[:\\/]+', '_').Trim('_')
        Move-Item -Path $Path -Destination (Join-Path $quarantine $leaf) -Force
        Write-Ok "Quarantined: $Path"
    }
}

Write-Step "Applying"
foreach ($f in $actionable) {
    if ($f.Verdict -eq "REMOVE") {
        Remove-OrQuarantine $f.Dir
    } else {
        foreach ($sub in @("Binaries", "Intermediate")) {
            $p = Join-Path $f.Dir $sub
            if (Test-Path $p) { Remove-OrQuarantine $p }
        }
    }
}

if (-not $NoBackup -and (Test-Path $quarantine)) {
    Write-Host ""
    Write-Host "Quarantine folder (delete it once the editor opens cleanly):" -ForegroundColor Cyan
    Write-Host "  $quarantine"
}
Write-Host ""
Write-Host "Cleanup complete. Re-run the installer (or Scripts\Install-Earth4D.ps1) to deploy a fresh $TargetEngineVersion build." -ForegroundColor Green
