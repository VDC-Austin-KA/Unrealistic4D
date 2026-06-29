@echo off
REM ============================================================================
REM  Earth4D — iterative editor build.
REM
REM  Recompiles the plugin + project C++ modules (Earth4DRuntime / Earth4DEditor /
REM  Earth4DTemplate). Run this AFTER changing C++ and BEFORE opening the editor.
REM
REM  WHY a script instead of letting the editor compile on open:
REM    The editor locks the module DLLs while running, and on a RAM-limited machine
REM    the editor's "compile on load" can fail (the compiler + the loading editor
REM    fight for memory) -> "Earth4D could not be compiled". Building with the editor
REM    CLOSED gives the compiler the whole machine, so it succeeds, and the editor
REM    then opens straight to up-to-date modules with no on-load compile.
REM
REM  Usage:
REM    Build.bat                 Compile (editor must already be closed).
REM    Build.bat /close          Close a running editor first, then compile.
REM    Build.bat /clean          Delete module binaries+intermediate, full rebuild.
REM    Build.bat /run            Launch the editor after a successful compile.
REM    Build.bat /close /run     The fast iterate loop (== BuildAndRun.bat).
REM    Build.bat "D:\EPIC\UE_5.8"  Force the engine path (else auto-detected).
REM    (flags can be combined in any order)
REM ============================================================================
setlocal EnableExtensions EnableDelayedExpansion

set "DO_CLOSE="
set "DO_CLEAN="
set "DO_RUN="
set "DO_NOPAUSE="
set "UE_ROOT="
for %%A in (%*) do (
  if /I "%%~A"=="/close"   ( set "DO_CLOSE=1"
  ) else if /I "%%~A"=="/clean"  ( set "DO_CLEAN=1"
  ) else if /I "%%~A"=="/run"    ( set "DO_RUN=1"
  ) else if /I "%%~A"=="/nopause" ( set "DO_NOPAUSE=1"
  ) else ( set "UE_ROOT=%%~A" )
)

set "PROJDIR=%~dp0..\Earth4DTemplate"
set "UPROJECT=%PROJDIR%\Earth4D.uproject"

REM ---- Editor running? It locks the DLLs; we cannot build over it. ----
tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>nul | find /I "UnrealEditor.exe" >nul
if not errorlevel 1 (
  if defined DO_CLOSE (
    echo Closing the running Unreal editor...
    taskkill /IM UnrealEditor.exe /F >nul 2>&1
    taskkill /IM CrashReportClient.exe /F >nul 2>&1
    timeout /t 3 /nobreak >nul
  ) else (
    echo [STOP] The Unreal editor is running -- it locks the module DLLs.
    echo        Close it and re-run, or use:  Build.bat /close
    if not defined DO_NOPAUSE pause
    exit /b 1
  )
)

REM ---- Locate the engine (arg > registry > LauncherInstalled.dat > default). ----
if not defined UE_ROOT for /f "tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8" /v InstalledDirectory 2^>nul') do set "UE_ROOT=%%B"
if not defined UE_ROOT for /f "usebackq delims=" %%A in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$d='%ProgramData%\Epic\UnrealEngineLauncher\LauncherInstalled.dat'; if(Test-Path $d){(Get-Content $d -Raw ^| ConvertFrom-Json).InstallationList ^| ?{$_.AppName -like 'UE_5.8*'} ^| select -First 1 -ExpandProperty InstallLocation}"`) do set "UE_ROOT=%%A"
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"

set "UBT=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
set "EDITOR=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%UBT%" (
  echo [ERROR] Could not find Unreal at "%UE_ROOT%".
  echo         Pass your engine path, e.g.:  Build.bat "D:\EPIC\UE_5.8"
  if not defined DO_NOPAUSE pause
  exit /b 1
)

REM ---- Optional clean: drop built module binaries + their intermediate. ----
if defined DO_CLEAN (
  echo Cleaning Earth4D module binaries + intermediate...
  del /q "%PROJDIR%\Binaries\Win64\UnrealEditor-Earth4D*.dll" >nul 2>&1
  del /q "%PROJDIR%\Binaries\Win64\UnrealEditor-Earth4D*.pdb" >nul 2>&1
  rmdir /s /q "%~dp0..\Earth4D\Intermediate\Build" >nul 2>&1
)

echo.
echo Engine : %UE_ROOT%
echo Project: %UPROJECT%
echo Compiling Earth4DTemplateEditor ^(Win64 Development^)...
echo.
call "%UBT%" Earth4DTemplateEditor Win64 Development -project="%UPROJECT%" -waitmutex
set "ERR=%ERRORLEVEL%"

if not "%ERR%"=="0" (
  echo.
  echo [FAILED] Compile failed ^(exit %ERR%^). Scroll up: the FIRST line containing
  echo          "error:" or "error C" is the cause. Copy it back if you want help.
  if not defined DO_NOPAUSE pause
  exit /b %ERR%
)

echo.
echo [DONE] Modules are up to date.
if defined DO_RUN (
  echo Launching the editor...
  start "" "%EDITOR%" "%UPROJECT%"
) else (
  echo You can open Earth4D.uproject now.
)
if not defined DO_NOPAUSE pause
exit /b 0
