@echo off
REM ============================================================================
REM  Earth4D — clean rebuild. Use when an incremental build acts up, after an
REM  engine change, or when the editor insists modules are out of date:
REM    1) closes the editor, 2) deletes the Earth4D module binaries+intermediate,
REM    3) does a full from-scratch compile of the plugin + project modules.
REM
REM  Pass an engine path if auto-detect fails:  Rebuild.bat "D:\EPIC\UE_5.8"
REM ============================================================================
call "%~dp0Build.bat" /close /clean %*
