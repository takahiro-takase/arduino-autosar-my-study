@echo off
REM uds_tester 起動ランチャー。カレントディレクトリに関わらず常に config.json /
REM capl_scripts/ をこのフォルダ基準で解決させるため、実行前に自分自身の
REM ディレクトリへ cd する（src\app.py の --config 既定値は相対パス "config.json"
REM のため、cwd がここでないと見つからない）。
cd /d "%~dp0"
python src\app.py %*
pause
