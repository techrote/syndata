@echo off
setlocal
set "ROOT=%~dp0.."
if not exist "%ROOT%\build\Release\SynData.exe" (
    call "%~dp00Build.cmd"
    if errorlevel 1 exit /b %errorlevel%
)
"%ROOT%\build\Release\SynData.exe" %*
exit /b %errorlevel%
