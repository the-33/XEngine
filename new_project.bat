@echo off
setlocal
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0tools\new_project.py" %*
) else (
    python "%~dp0tools\new_project.py" %*
)
exit /b %errorlevel%
