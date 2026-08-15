@echo off
REM uds_tester 起動ランチャー。src\app.py の --config 既定値（DEFAULT_CONFIG_PATH）は
REM __file__ 基準の絶対パスのため cwd には依存しないが、他ツールの run.bat と
REM 体裁を揃えるため実行前に自分自身のディレクトリへ cd しておく。
cd /d "%~dp0"
python src\app.py %*
pause
