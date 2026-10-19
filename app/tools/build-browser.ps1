param(
    [string]$WasmBuildDir = 'C:/catchim-wasm-mt',
    [string]$EmsdkDir = 'C:/emsdk',
    [string]$CMake = "$env:APPDATA/Python/Python311/site-packages/cmake/data/bin/cmake.exe"
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$web = Join-Path $repo 'web'
$target = Join-Path ([IO.Path]::GetFullPath($WasmBuildDir)) 'bin'
if (!(Test-Path -LiteralPath (Join-Path $WasmBuildDir 'CMakeCache.txt'))) { throw 'Configure the Emscripten CMake build before building the browser frontend.' }
if (!(Test-Path -LiteralPath $CMake)) { $CMake = (Get-Command cmake -ErrorAction Stop).Source }
$environment = Join-Path $EmsdkDir 'emsdk_env.bat'
& cmd /d /s /c "call `"$environment`" >nul 2>&1 && `"$CMake`" --build `"$WasmBuildDir`" --target catchim_browser_core -j 6"
if ($LASTEXITCODE -ne 0) { throw 'C++ browser core build failed.' }
Copy-Item -Path (Join-Path $target 'catchim_browser_core.*') -Destination (Join-Path $web 'public/wasm') -Force
Push-Location $web
try {
    & node node_modules/vite/bin/vite.js build --config vite.browser.config.ts
    if ($LASTEXITCODE -ne 0) { throw 'Browser frontend build failed.' }
} finally { Pop-Location }
# Keep the Qt frontend and its persisted filesystem accessible while upgrading.
Copy-Item -LiteralPath (Join-Path $repo 'app/src/wasm/catchim_app.html') -Destination (Join-Path $target 'qt-editor.html') -Force
Copy-Item -Path (Join-Path $web 'dist-browser/*') -Destination $target -Recurse -Force
Copy-Item -LiteralPath (Join-Path $web 'dist-browser/index.html') -Destination (Join-Path $target 'browser.html') -Force
Write-Host "Built Catchim HTML/Canvas/WebGPU + C++ browser core at $target"
