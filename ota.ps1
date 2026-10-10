# 通过 WiFi 远程升级 StickS3（OTA，无需插线）
# 用法:  powershell -ExecutionPolicy Bypass -File ota.ps1 [IP]
#   不填 IP 时会尝试：1) mDNS 主机名 sticks3-weather.local  2) ARP 里 ESP32 网卡
# 例如:  powershell -ExecutionPolicy Bypass -File ota.ps1 192.168.1.50

param([string]$Ip = "")

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

# mDNS 主机名（固件里设置）
$MDNS = "sticks3-weather.local"

# Espressif 常见网卡 OUI 前缀
$OUIS = @('7c-df-a1','7c-e8-b1','24-0a-c4','30-ae-a4','3c-61-05','48-27-e2',
          'a4-cf-12','c4-4f-33','d8-a0-1d','e0-5a-1b','f4-cf-a2','8c-aa-b5',
          '94-b9-7e','cc-50-e3','b4-e6-2d','dc-54-75','08-d1-f9')

if (-not $Ip) {
    Write-Host "未指定 IP，正在查找设备…" -ForegroundColor Cyan
    try {
        $Ip = (Resolve-DnsName $MDNS -ErrorAction Stop | Select-Object -First 1).IPAddress
        if ($Ip) { Write-Host "mDNS 找到: $Ip" -ForegroundColor Green }
    } catch { }
}
if (-not $Ip) {
    arp -a | ForEach-Object {
        if ($_ -match '(\d+\.\d+\.\d+\.\d+)\s+([0-9a-fA-F]{2}-[0-9a-fA-F]{2}-[0-9a-fA-F]{2})') {
            if ($OUIS -contains $Matches[2].ToLower()) { $Ip = $Matches[1] }
        }
    }
    if ($Ip) { Write-Host "ARP 里找到 ESP32: $Ip" -ForegroundColor Green }
}
if (-not $Ip) {
    Write-Host "找不到设备。请确认：设备已联网、和电脑在同一 WiFi/热点下。" -ForegroundColor Red
    Write-Host "也可手动传入：ota.ps1 192.168.x.x" -ForegroundColor Yellow
    exit 1
}

Write-Host "`n[1/2] 编译…" -ForegroundColor Cyan
python -m platformio run
if ($LASTEXITCODE -ne 0) { Write-Host "编译失败" -ForegroundColor Red; exit 1 }

$espota = Join-Path $env:USERPROFILE ".platformio\packages\framework-arduinoespressif32\tools\espota.py"
Write-Host "`n[2/2] OTA 推送到 $Ip …" -ForegroundColor Cyan
python $espota -i $Ip -f ".pio\build\m5stack-sticks3\firmware.bin"
if ($LASTEXITCODE -ne 0) {
    Write-Host "首次可能丢包，自动重试一次…" -ForegroundColor Yellow
    python $espota -i $Ip -f ".pio\build\m5stack-sticks3\firmware.bin"
}

Write-Host "`n完成！设备会自动重启。" -ForegroundColor Green
