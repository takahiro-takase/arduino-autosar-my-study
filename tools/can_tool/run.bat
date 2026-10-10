@echo off
REM can_tool launcher (CAN signal editor + config editor + UDS Tester).
REM Comments are ASCII only: cmd.exe misparses UTF-8 Japanese in .bat files under code page 932.
REM Defaults of --data/--config are resolved from the location of the app script, so the cwd does not matter.
cd /d "%~dp0"
python src\app.py %*
pause
