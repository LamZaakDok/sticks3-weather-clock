# 一键编译 + 烧录 StickS3 天气机
# 用法:  powershell -ExecutionPolicy Bypass -File flash.ps1 [COM端口]
# 例如:  powershell -ExecutionPolicy Bypass -File flash.ps1 COM6

param(
    [string]$Port = ""
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if (-not $Port) {
    Write-Host "未指定端口，自动检测 ESP32 串口..." -ForegroundColor Cyan
    $dev = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue |
           Where-Object { $_.PNPDeviceID -match 'VID_303A' } | Select-Object -First 1
    if ($dev) { $Port = $dev.DeviceID }
}
if (-not $Port) {
    Write-Host "找不到 ESP32 串口，请插好设备（设备管理器里一般是 COMx / USB 串行设备）" -ForegroundColor Red
    exit 1
}
Write-Host "使用端口: $Port" -ForegroundColor Green

Write-Host "`n[1/2] 编译..." -ForegroundColor Cyan
python -m platformio run
if ($LASTEXITCODE -ne 0) { Write-Host "编译失败" -ForegroundColor Red; exit 1 }

Write-Host "`n[2/2] 烧录到 $Port ..." -ForegroundColor Cyan
python -m platformio run -t upload --upload-port $Port
if ($LASTEXITCODE -ne 0) { Write-Host "烧录失败" -ForegroundColor Red; exit 1 }

Write-Host "`n完成！按 KEY1 翻页，KEY2 刷新。" -ForegroundColor Green
