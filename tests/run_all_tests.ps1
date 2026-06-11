$ErrorActionPreference = "Stop"
chcp 65001 | Out-Null
[Console]::InputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$env:PYTHONUTF8 = "1"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
python tools\test_runner.py scheduler
python tools\test_runner.py memory --no-compile
python tools\test_runner.py sync --no-compile
python tools\test_runner.py filesystem --no-compile
