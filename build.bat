@echo off
setlocal
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0tools\build.py" %*
) else (
    python "%~dp0tools\build.py" %*
)
exit /b %errorlevel%
