@echo off
setlocal
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0tools\setup_cross_sdk.py" %*
) else (
    python "%~dp0tools\setup_cross_sdk.py" %*
)
set "XENGINE_EXIT_CODE=%errorlevel%"
rem Keep the terminal open when launched with cmd /c (e.g. Explorer double-click).
echo(%CMDCMDLINE% | findstr /I /C:" /c " >nul
if not errorlevel 1 pause
exit /b %XENGINE_EXIT_CODE%
