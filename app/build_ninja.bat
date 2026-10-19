@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set NINJA="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set QT_DIR=D:\Qt\6.8.0\msvc2022_64

echo === Configuring with Ninja and Qt6 ===
%CMAKE% -B build -G Ninja -DCMAKE_MAKE_PROGRAM=%NINJA% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%"
if errorlevel 1 exit /b 1

echo === Building ===
%NINJA% -C build
if errorlevel 1 exit /b 1

echo === Deploying Qt Runtime DLLs ===
if exist "%QT_DIR%\bin\windeployqt.exe" (
    if exist ".\build\bin\Catchim.exe" (
        "%QT_DIR%\bin\windeployqt.exe" --compiler-runtime .\build\bin\Catchim.exe
    ) else if exist ".\build\bin\catchim_app.exe" (
        "%QT_DIR%\bin\windeployqt.exe" --compiler-runtime .\build\bin\catchim_app.exe
    )
)

echo === Copying to Root Catchim.exe and Dependencies ===
if exist ".\build\bin\Catchim.exe" (
    taskkill /F /IM Catchim.exe >nul 2>&1
    copy /Y ".\build\bin\Catchim.exe" "..\Catchim.exe"
    copy /Y ".\build\bin\*.dll" "..\"
    if exist ".\build\bin\platforms" xcopy /E /I /Y ".\build\bin\platforms" "..\platforms"
    if exist ".\build\bin\styles" xcopy /E /I /Y ".\build\bin\styles" "..\styles"
    if exist ".\build\bin\imageformats" xcopy /E /I /Y ".\build\bin\imageformats" "..\imageformats"
    if exist ".\build\bin\iconengines" xcopy /E /I /Y ".\build\bin\iconengines" "..\iconengines"
    if exist ".\icons" xcopy /E /I /Y ".\icons" ".\build\bin\icons"
    if exist ".\icons" xcopy /E /I /Y ".\icons" "..\icons"
)

echo === Running Parity Tests ===
.\build\bin\catchim_tests.exe

