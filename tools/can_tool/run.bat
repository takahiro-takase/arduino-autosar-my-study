@echo off
REM can_tool 起動ランチャー（CAN信号定義エディタ + UDS Tester の統合ツール）。
REM --data/--config の既定値は src\app.py 側で __file__ 基準の絶対パスとして
REM 解決されるため cwd には依存しない。
cd /d "%~dp0"
python src\app.py %*
pause
