@echo off
REM ============================================================================
REM  Earth4D — compile the project + plugin (use this to fix the editor's
REM  "Earth4D could not be compiled" prompt and to see real compiler errors).
REM
REM  Usage:  double-click, OR:  Build.bat "C:\Path\To\UE_5.8"
REM  Then open Earth4DTemplate\Earth4D.uproject normally.
REM ============================================================================
setlocal EnableExtensions

set "UE_ROOT=%~1"
if "%UE_ROOT%"=="" for /f "tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8" /v InstalledDirectory 2^>nul') do set "UE_ROOT=%%B"
REM Launcher installs (any drive, e.g. D:\EPIC\UE_5.8) are listed in LauncherInstalled.dat.
if "%UE_ROOT%"=="" for /f "usebackq delims=" %%A in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$d='%ProgramData%\Epic\UnrealEngineLauncher\LauncherInstalled.dat'; if(Test-Path $d){(Get-Content $d -Raw ^| ConvertFrom-Json).InstallationList ^| ?{$_.AppName -like 'UE_5.8*'} ^| select -First 1 -ExpandProperty InstallLocation}"`) do set "UE_ROOT=%%A"
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"

set "UPROJECT=%~dp0..\Earth4DTemplate\Earth4D.uproject"
set "UBT=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"

if not exist "%UBT%" (
  echo [ERROR] Could not find Unreal at "%UE_ROOT%".
  echo         Pass your engine path, e.g.:  Build.bat "C:\Program Files\Epic Games\UE_5.8"
  pause & exit /b 1
)

echo Compiling Earth4DTemplateEditor (Win64, Development)...
call "%UBT%" Earth4DTemplateEditor Win64 Development -project="%UPROJECT%" -waitmutex

if errorlevel 1 (
  echo.
  echo [FAILED] Compile failed. The FIRST "error:" line above is the cause —
  echo          copy it back to share if you need help fixing it.
  pause & exit /b 1
)

echo.
echo [DONE] Compiled. You can now open Earth4D.uproject.
pause
