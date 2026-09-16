@echo off
setlocal
set "ROOT=%~dp0.."
call "%~dp00Build.cmd"
if errorlevel 1 exit /b %errorlevel%
ctest --test-dir "%ROOT%\build" -C Release --output-on-failure
exit /b %errorlevel%
