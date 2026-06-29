@echo off
REM ============================================================================
REM  Earth4D — the fast iterate loop. Double-click this after changing C++:
REM    1) closes the Unreal editor (it locks the module DLLs),
REM    2) recompiles the plugin + project modules (full RAM available),
REM    3) relaunches the editor on Earth4D.uproject.
REM
REM  Pass an engine path if auto-detect fails:  BuildAndRun.bat "D:\EPIC\UE_5.8"
REM ============================================================================
call "%~dp0Build.bat" /close /run %*
