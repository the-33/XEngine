@echo off
setlocal
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0tools\check_dependencies.py" %*
) else (
    python "%~dp0tools\check_dependencies.py" %*
)
exit /b %errorlevel%
