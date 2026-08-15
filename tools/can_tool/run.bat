@echo off
REM can_tool 起動ランチャー（can_signal_editor + uds_tester の統合版）。
REM --data/--config の既定値は src\app.py 側で __file__ 基準の絶対パスとして
REM 解決されるため cwd には依存しないが、他ツールの run.bat と体裁を揃える。
cd /d "%~dp0"
python src\app.py %*
pause
