@echo off
setlocal
set "ROOT=%~dp0.."
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%ROOT%\tools\build.py" engine %*
) else (
    python "%ROOT%\tools\build.py" engine %*
)
exit /b %errorlevel%
