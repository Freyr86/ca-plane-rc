@echo off
set "XFLR=%~dp0tools\xflr5\xflr5_v6.62_win64\xflr5.exe"
if not exist "%XFLR%" set "XFLR=%USERPROFILE%\Dropbox\SimPlaneurPerso\tools\xflr5\xflr5_v6.62_win64\xflr5.exe"
start "" "%XFLR%"
