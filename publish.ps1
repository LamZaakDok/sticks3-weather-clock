# 一键发布到 GitHub
# 用法:
#   powershell -ExecutionPolicy Bypass -File publish.ps1
#   或指定仓库地址:
#   powershell -ExecutionPolicy Bypass -File publish.ps1 -Repo https://github.com/你的用户名/sticks3-weather-clock.git
#
# 前提: 已在 GitHub 建好一个空仓库（不要勾选 README）

param([string]$Repo = "")

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

# 1. 确保是 git 仓库
if (-not (Test-Path ".git")) { git init | Out-Null; Write-Host "已初始化 git 仓库" -ForegroundColor Cyan }

# 2. 设置远程
if (-not $Repo) {
    $Repo = (git remote get-url origin 2>$null)
    if (-not $Repo) {
        $Repo = Read-Host "请输入 GitHub 仓库地址 (例如 https://github.com/you/sticks3-weather-clock.git)"
    }
}
if (git remote 2>$null | Select-String -Quiet "^origin$") {
    git remote set-url origin $Repo
} else {
    git remote add origin $Repo
}
Write-Host "远程仓库: $Repo" -ForegroundColor Green

# 3. 提交改动
git add -A
$changes = git status --porcelain
if ($changes) {
    git commit -m "Update" | Out-Null
    Write-Host "已提交本地改动" -ForegroundColor Cyan
}

# 4. 推送
git branch -M main
Write-Host "`n正在推送到 GitHub…" -ForegroundColor Cyan
git push -u origin main

Write-Host "`n✅ 推送完成！" -ForegroundColor Green
Write-Host @"

接下来手动做两件事：
1) 建 Release：
   打开仓库 → Releases → Draft a new release
   Tag: v0.1.0   Title: StickS3 Weather Clock v0.1.0
   附件上传: release/sticks3-weather-full.bin
2) 填 About（右上角齿轮）：
   描述和 topics 见 docs/PUBLISH.md

（想用命令行建 Release，可安装 GitHub CLI: winget install GitHub.cli，然后 `gh auth login`）
"@ -ForegroundColor Yellow
