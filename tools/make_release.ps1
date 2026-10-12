# 生成整包镜像 release/sticks3-weather-full.bin（从 0x0 刷入，含 bootloader+分区+程序+语音）
#
# ⚠️ 注意：给 esptool merge_bin 千万不要加 --flash_mode / --flash_freq / --flash_size，
#    否则会把 bootloader 头部（第 2 字节）改错，烧进去设备无法开机！
#
# 用法: powershell -ExecutionPolicy Bypass -File tools\make_release.ps1

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

Write-Host "[1/2] 编译…" -ForegroundColor Cyan
python -m platformio run
if ($LASTEXITCODE -ne 0) { exit 1 }

$esptool = Join-Path $env:USERPROFILE ".platformio\packages\tool-esptoolpy\esptool.py"
$b = ".pio\build\m5stack-sticks3"
$bootapp = Join-Path $env:USERPROFILE ".platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"

New-Item -ItemType Directory -Force -Path release | Out-Null

Write-Host "[2/2] 合成整包…" -ForegroundColor Cyan
python $esptool --chip esp32s3 merge_bin -o "release\sticks3-weather-full.bin" `
    0x0 "$b\bootloader.bin" `
    0x8000 "$b\partitions.bin" `
    0xe000 "$bootapp" `
    0x10000 "$b\firmware.bin" `
    0x670000 "$b\littlefs.bin"

Write-Host "`n完成: release\sticks3-weather-full.bin （刷录地址 0x0）" -ForegroundColor Green
Write-Host "校验: esptool.py --chip esp32s3 -p COM5 write_flash 0x0 release\sticks3-weather-full.bin" -ForegroundColor Yellow
