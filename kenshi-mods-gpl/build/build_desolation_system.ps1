$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\env_loader.ps1"
$srcDir = "D:\kenshimods\KenshiPluginDev\DesolationSystem"
$outDir = "D:\kenshimods\KenshiPluginDev\build_test"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

# ---- 版本指纹: 注入 git 短哈希 (P0 治理) ----
# 编码规则: 指纹 = g<短哈希> + (工作树有未提交改动时追加 'x')；如 g09c5c4fx
$gitHash = "unknown"
try { $gitHash = (git -C $srcDir rev-parse --short HEAD 2>$null).Trim() } catch {}
if (-not $gitHash) { $gitHash = "unknown" }
$gitDirty = ""
try { if (git -C $srcDir status --porcelain 2>$null) { $gitDirty = "x" } } catch {}
$gitStamp = "g$gitHash$gitDirty"
Write-Host "=== Version stamp: $gitStamp ===" -ForegroundColor Cyan

$targetModDir = "D:\steam\steamapps\common\Kenshi\mods\DesolationSystem"

Write-Host "=== Building Unified Plugin: DesolationSystem ===" -ForegroundColor Cyan

$cppFile = "$srcDir\DesolationSystem.cpp"
$objFile = "$outDir\DesolationSystem.obj"
$dllFile = "$outDir\DesolationSystem.dll"

& $clang -m64 /O2 /MD /std:c++17 /utf-8 /EHsc -w `
    "-I$kenshiLibInc" `
    "-I$kenshiLibInc\ogre" `
    "-I$msvcInc" `
    "-I$sdkInc\ucrt" `
    "-I$sdkInc\um" `
    "-I$sdkInc\shared" `
    "-I$boostInc" `
    "/DDESO_GIT_HASH=g$gitHash$gitDirty" `
    -c "$cppFile" /Fo"$objFile"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed"
    exit 1
}

$linkArgs = @(
    "/DLL",
    "/NOLOGO",
    "/OUT:$dllFile",
    "$objFile",
    "KenshiLib.lib",
    "user32.lib",
    "kernel32.lib"
)
foreach ($lp in $libPaths) {
    $linkArgs += "/LIBPATH:$lp"
}

& $lld @linkArgs

if ($LASTEXITCODE -ne 0) {
    Write-Error "Linking failed"
    exit 1
}

Copy-Item -Path $dllFile -Destination "$targetModDir\DesolationSystem.dll" -Force
# 部署校验 (2026-09-07): 游戏运行中目标 DLL 被锁定时 Copy-Item 静默失败 → 部署过期版本
# 却报 SUCCESS, 曾导致'改了没生效'排查浪费。此处强制校验时间戳, 失败立即报错退出。
$srcItem = Get-Item $dllFile
$dstPath = "$targetModDir\DesolationSystem.dll"
$dstItem = Get-Item $dstPath -ErrorAction SilentlyContinue
if (-not $dstItem -or $dstItem.LastWriteTime -lt $srcItem.LastWriteTime.AddSeconds(-1)) {
    Write-Error "部署失败: 目标 $dstPath 未更新 (目标 DLL 被占用? 请先关闭游戏)" -ErrorAction Stop
}
Set-Content -Path "$targetModDir\RE_Kenshi.json" -Value '{"Plugins":["DesolationSystem.dll","DivineGraceUI.dll"]}' -Encoding ASCII

Write-Host "[SUCCESS] DesolationSystem built and deployed successfully to $targetModDir !" -ForegroundColor Green
