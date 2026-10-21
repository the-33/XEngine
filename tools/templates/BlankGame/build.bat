@echo off
setlocal
set "ROOT=%~dp0..\.."
rem %%~dp0 ends in \; append . so Windows does not swallow the closing quote.
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%ROOT%\tools\build.py" game --project-dir "%~dp0." %*
) else (
    python "%ROOT%\tools\build.py" game --project-dir "%~dp0." %*
)
exit /b %errorlevel%
