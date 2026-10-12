# 一键发布到 GitHub（自动绕过坏代理）
# 用法:
#   powershell -ExecutionPolicy Bypass -File publish.ps1 -Repo https://github.com/你的用户名/仓库名.git
#
# 前提: 已登录 GitHub 并建好一个【空仓库】（不要勾选 README/LICENSE）

param([string]$Repo = "")

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

# 本机 git 常配了个 127.0.0.1 代理但没开，这里统一绕过
function Git { & git -c http.proxy= -c https.proxy= @args }

# 1. 确保是 git 仓库
if (-not (Test-Path ".git")) { Git init | Out-Null; Write-Host "已初始化 git 仓库" -ForegroundColor Cyan }

# 2. 远程地址
if (-not $Repo) { $Repo = (Git remote get-url origin 2>$null) }
if (-not $Repo -or $Repo -match '[<＞]' -or $Repo -match '你的用户名') {
    Write-Host "`n请先在 GitHub 新建一个【空仓库】，再用真实地址运行：" -ForegroundColor Yellow
    Write-Host "  powershell -ExecutionPolicy Bypass -File publish.ps1 -Repo https://github.com/你的用户名/仓库名.git" -ForegroundColor Yellow
    exit 1
}
$remotes = (Git remote 2>$null)
if ($remotes -match 'origin') { Git remote set-url origin $Repo | Out-Null }
else { Git remote add origin $Repo | Out-Null }
Write-Host "远程仓库: $Repo" -ForegroundColor Green

# 3. 连通性检查
Write-Host "`n检查能否连接 GitHub…" -ForegroundColor Cyan
Git ls-remote --heads $Repo *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Host "连不上 GitHub。请检查：仓库地址是否正确/是否已建好、网络是否可用。" -ForegroundColor Red
    exit 1
}

# 4. 提交改动
Git add -A
if (Git status --porcelain) { Git commit -m "Update" | Out-Null; Write-Host "已提交本地改动" -ForegroundColor Cyan }

# 5. 推送
Git branch -M main | Out-Null
Write-Host "`n正在推送到 GitHub…（首次可能弹出登录窗口，按提示授权）" -ForegroundColor Cyan
Git push -u origin main
if ($LASTEXITCODE -ne 0) {
    Write-Host "`n推送失败。若提示要账号，请用 GitHub Personal Access Token 或在弹出的窗口登录。" -ForegroundColor Red
    exit 1
}

Write-Host "`n推送完成！" -ForegroundColor Green
Write-Host @"

接下来手动做两件事：
1) 建 Release：仓库页 -> Releases -> Draft a new release
   Tag: v0.1.0   Title: StickS3 Weather Clock v0.1.0
   附件上传: release/sticks3-weather-full.bin
2) 填 About（右上角齿轮）：描述和 topics 见 docs/PUBLISH.md
"@ -ForegroundColor Yellow
