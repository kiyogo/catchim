param([string]$WasmBuildDir = 'C:/catchim-wasm-mt', [int]$Port = 8766)
$ErrorActionPreference = 'Stop'
$directory = Join-Path ([IO.Path]::GetFullPath($WasmBuildDir)) 'bin'
if (!(Test-Path -LiteralPath (Join-Path $directory 'index.html'))) { throw 'Run Build_Web.bat first.' }
if (!(Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue)) {
    $python = (Get-Command python -ErrorAction Stop).Source
    $server = Join-Path $PSScriptRoot 'serve-browser.py'
    Start-Process -FilePath $python -ArgumentList @("`"$server`"", "`"$directory`"", '--port', "$Port") -WindowStyle Hidden -RedirectStandardOutput (Join-Path $WasmBuildDir 'browser-server.log') -RedirectStandardError (Join-Path $WasmBuildDir 'browser-server-error.log')
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        if (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue) { break }
        Start-Sleep -Milliseconds 100
    }
}
Start-Process "http://127.0.0.1:$Port/"
