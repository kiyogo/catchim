@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" -C build catchim_app catchim_ui_grabber catchim_native_workflow catchim_playback_regression catchim_preview_selection catchim_ui_finish catchim_tests
exit /b %errorlevel%
