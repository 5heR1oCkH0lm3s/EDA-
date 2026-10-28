@echo off
if not exist "%~dp0MyEDA\build\Debug\MyEDA.exe" (
    echo MyEDA.exe was not found. Run build.bat first.
    pause
    exit /b 1
)
pushd "%~dp0MyEDA\build\Debug"
start "" "MyEDA.exe"
popd
exit /b 0
