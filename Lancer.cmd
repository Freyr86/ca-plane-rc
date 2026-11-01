@echo off
set "PY=%~dp0tools\python\pythonw.exe"
if not exist "%PY%" set "PY=%USERPROFILE%\Dropbox\SimPlaneurPerso\tools\python\pythonw.exe"
start "" "%PY%" "%~dp0simulation\launcher.py"
