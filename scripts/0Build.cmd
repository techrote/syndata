@echo off
setlocal
set "ROOT=%~dp0.."
cmake -S "%ROOT%" -B "%ROOT%\build" -A x64
if errorlevel 1 exit /b %errorlevel%
cmake --build "%ROOT%\build" --config Release --parallel
exit /b %errorlevel%
