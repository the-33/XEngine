@echo off
set "GAME_DIR=%~dp0"
python "%GAME_DIR%..\..\tools\build.py" game --project-dir "%GAME_DIR%" %*
exit /b %errorlevel%
