@echo off
setlocal
set "APP_DIR=%~dp0dist\publish"
set "APP_EXE=%APP_DIR%\CANaerospace Joystick Mapper.exe"
if not exist "%APP_EXE%" (
  echo CANaerospace Joystick Mapper has not been published yet.
  echo Run: powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Configuration Release
  exit /b 1
)
start "" "%APP_EXE%"
