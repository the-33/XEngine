@echo off
setlocal
set "ROOT=%~dp0..\.."
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%ROOT%\tools\new_game_asset.py" --project-dir "%~dp0." %*
) else (
    python "%ROOT%\tools\new_game_asset.py" --project-dir "%~dp0." %*
)
exit /b %errorlevel%
