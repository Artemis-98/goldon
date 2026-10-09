# آپلود OTA روی همان وای‌فای خانه
# استفاده: .\upload-ota.ps1
# یا: .\upload-ota.ps1 -Ip 192.168.x.x

param(
  [string]$Ip = ""
)

$fqbn = "esp32:esp32:esp32s3:CDCOnBoot=default,FlashSize=8M,PSRAM=disabled,PartitionScheme=default_8MB,USBMode=hwcdc,UploadSpeed=115200"
$otaPass = "ota2026"
$hostName = "goldon-yard.local"

Set-Location -LiteralPath $PSScriptRoot
arduino-cli compile --fqbn $fqbn --build-path ".\build" .
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $Ip) {
  # تلاش برای resolve کردن mDNS / ping
  try {
    $r = Resolve-DnsName $hostName -ErrorAction Stop | Select-Object -First 1
    $Ip = $r.IPAddress
  } catch {
    Write-Host "IP خودکار پیدا نشد. مثال: .\upload-ota.ps1 -Ip 192.168.1.50"
    exit 2
  }
}

Write-Host "OTA upload -> $Ip"
arduino-cli upload -p $Ip --fqbn $fqbn --build-path ".\build" . --upload-field password=$otaPass
