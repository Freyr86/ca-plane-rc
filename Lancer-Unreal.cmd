@echo off
powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File "%~dp0simulation\launcher_unreal.ps1" %*
set "SIM_RESULT=%ERRORLEVEL%"
if not "%SIM_RESULT%"=="0" if "%~1"=="" pause
exit /b %SIM_RESULT%
