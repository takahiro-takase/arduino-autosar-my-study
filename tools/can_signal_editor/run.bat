@echo off
REM can_signal_editor 起動ランチャー。カレントディレクトリに関わらず常に
REM data/can_signals.json をリポジトリルート基準で解決させるため、実行前に
REM 自分自身のディレクトリへ cd する（src\app.py の --data 既定値は
REM スクリプト自身の場所からの相対パスのため、cwd に依存しない）。
cd /d "%~dp0"
python src\app.py %*
pause
