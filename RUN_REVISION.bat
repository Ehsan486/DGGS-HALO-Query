@echo off
rem Builds the project and repeats the halo measurements of the paper.
rem Double click, or run from a terminal.
rem Add -Quick for a short test run, or -Stage BUILD / F / A / D / B for one stage.
rem Output goes to results_local\ ; the records of the paper in results_rev\ are not changed.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\run_revision.ps1" %*
echo.
echo Finished (see results_local\STATUS.txt). You can close this window.
pause
