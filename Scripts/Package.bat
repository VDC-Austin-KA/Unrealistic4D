@echo off
REM ============================================================================
REM  Earth4D — one-click PACKAGE (build the distributable standalone app).
REM
REM  Usage:  double-click this file, OR run:  Package.bat "C:\Path\To\UE_5.8"
REM  Output: ..\Dist\Windows\  (zip that folder and send it to coworkers).
REM
REM  Requires: Unreal Engine 5.8 + Visual Studio 2022 with the
REM            "Game development with C++" workload installed.
REM ============================================================================
setlocal EnableExtensions

REM --- Locate the engine (arg 1 wins, else registry, else default guess) ---
set "UE_ROOT=%~1"
if "%UE_ROOT%"=="" for /f "tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8" /v InstalledDirectory 2^>nul') do set "UE_ROOT=%%B"
REM Launcher installs (any drive, e.g. D:\EPIC\UE_5.8) are listed in LauncherInstalled.dat.
if "%UE_ROOT%"=="" for /f "usebackq delims=" %%A in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$d='%ProgramData%\Epic\UnrealEngineLauncher\LauncherInstalled.dat'; if(Test-Path $d){(Get-Content $d -Raw ^| ConvertFrom-Json).InstallationList ^| ?{$_.AppName -like 'UE_5.8*'} ^| select -First 1 -ExpandProperty InstallLocation}"`) do set "UE_ROOT=%%A"
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"

set "UPROJECT=%~dp0..\Earth4DTemplate\Earth4D.uproject"
set "OUTPUT=%~dp0..\Dist"
set "RUNUAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"

echo.
echo   Engine : %UE_ROOT%
echo   Project: %UPROJECT%
echo   Output : %OUTPUT%\Windows
echo.

if not exist "%RUNUAT%" (
  echo [ERROR] Could not find Unreal at "%UE_ROOT%".
  echo         Pass your engine path, e.g.:  Package.bat "C:\Program Files\Epic Games\UE_5.8"
  pause & exit /b 1
)

call "%RUNUAT%" BuildCookRun ^
  -project="%UPROJECT%" ^
  -noP4 -platform=Win64 -clientconfig=Shipping ^
  -build -cook -stage -pak -archive -archivedirectory="%OUTPUT%"

if errorlevel 1 (
  echo.
  echo [FAILED] Packaging failed. Scroll up for the first error.
  pause & exit /b 1
)

echo.
echo [DONE] Packaged to:  %OUTPUT%\Windows
echo        Zip that folder and share it. Coworkers run Earth4D.exe inside it.
pause
