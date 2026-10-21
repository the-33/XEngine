@echo off
setlocal
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0png_to_ico.py" %*
) else (
    python "%~dp0png_to_ico.py" %*
)
exit /b %errorlevel%
