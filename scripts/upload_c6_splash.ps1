# Upload C6 splash firmware + LittleFS (Board #4 — ESP32-C6-LCD-1.47)
# Usage: powershell -File scripts/upload_c6_splash.ps1 [-Port COM15]

param(
  [string]$Port = "COM15"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$sketch = Join-Path $root "firmware\c6_splash"
$build = Join-Path $sketch "build"
$data = Join-Path $sketch "data"
$fqbn = "esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=default,UploadSpeed=921600"
$mkl = Join-Path $env:LOCALAPPDATA "Arduino15\packages\esp32\tools\mklittlefs\4.0.2-db0513a\mklittlefs.exe"
$fsImg = Join-Path $build "littlefs.bin"
$fsOffset = "0x290000"
$fsSize = "0x160000"

New-Item -ItemType Directory -Force -Path $build | Out-Null

Write-Host "== compile =="
arduino-cli compile --fqbn $fqbn --build-path $build $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== littlefs image =="
& $mkl -c $data -b 4096 -p 256 -s ([Convert]::ToInt64($fsSize, 16)) $fsImg
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== upload firmware $Port =="
arduino-cli upload -p $Port --fqbn $fqbn --build-path $build $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== upload LittleFS @ $fsOffset =="
$esp = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool*" -Recurse -Filter "esptool.exe" -ErrorAction SilentlyContinue |
  Sort-Object FullName -Descending | Select-Object -First 1
if (-not $esp) { throw "esptool not found" }
& $esp.FullName --chip esp32c6 --port $Port --baud 921600 write_flash $fsOffset $fsImg
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "DONE - C6 splash on $Port"
