param([string]$Lang = "zh")   # zh=中文版(默认) | en=英文版(-DDESO_ENGLISH)

. "$PSScriptRoot\env_loader.ps1"
$gameModsDir = "D:\steam\steamapps\common\Kenshi\mods"
$p = "DivineGraceUI"
$suffix = if ($Lang -eq "en") { "_EN" } else { "" }

Write-Host "Building $p ($Lang) ..." -ForegroundColor Cyan
$src = "D:\kenshimods\KenshiPluginDev\$p\$p.cpp"
$obj = "D:\kenshimods\KenshiPluginDev\build_test\$p$suffix.obj"
$dll = "D:\kenshimods\KenshiPluginDev\build_test\$p$suffix.dll"

$libArgs = $libPaths | ForEach-Object { "/LIBPATH:$_" }

# 语言编译开关: L_(中文, 英文) 宏按 DESO_ENGLISH 选侧
$desoInc = "D:\kenshimods\KenshiPluginDev\DesolationSystem"
$langArgs = @()
if ($Lang -eq "en") { $langArgs += "-DDESO_ENGLISH" }

& $clang -m64 /c /O2 /MD /std:c++17 /utf-8 /EHsc -w @langArgs `
    "/I$kenshiLibInc" "/I$desoInc" "/I$msvcInc" "/I$sdkInc\ucrt" "/I$sdkInc\um" "/I$sdkInc\shared" "/Fo$obj" "$src"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Compilation failed for $p ($Lang) with code $LASTEXITCODE" -ForegroundColor Red
    exit 1
}

& $lld /DLL "/OUT:$dll" "$obj" @libArgs KenshiLib.lib kernel32.lib user32.lib gdi32.lib
if ($LASTEXITCODE -ne 0) {
    Write-Host "Linking failed for $p ($Lang) with code $LASTEXITCODE" -ForegroundColor Red
    exit 1
}

if ($Lang -eq "zh") {
    # 中文版: 部署到 DesolationSystem 目录 (RE_Kenshi.json 双插件加载)
    $desoTargetDir = "$gameModsDir\DesolationSystem"
    if (Test-Path $desoTargetDir) {
        Copy-Item -Path $dll -Destination "$desoTargetDir\$p.dll" -Force
# 部署校验 (2026-09-07): 游戏运行中目标 DLL 被锁定时 Copy-Item 静默失败 → 部署过期版本
# 却报 SUCCESS, 曾导致'改了没生效'排查浪费。此处强制校验时间戳, 失败立即报错退出。
$srcItem = Get-Item $dll
$dstPath = "$desoTargetDir\$p.dll"
$dstItem = Get-Item $dstPath -ErrorAction SilentlyContinue
if (-not $dstItem -or $dstItem.LastWriteTime -lt $srcItem.LastWriteTime.AddSeconds(-1)) {
    Write-Error "部署失败: 目标 $dstPath 未更新 (目标 DLL 被占用? 请先关闭游戏)" -ErrorAction Stop
}
    }
    # 独立 mods\DivineGraceUI 部署已移除 (2026-09-06): UI DLL 仅随 DesolationSystem 文件夹
    # 分发; 独立文件夹无 .mod 不被加载, 纯残留, 已清理不再生成
    Write-Host "DivineGraceUI (zh) deployed successfully!" -ForegroundColor Green
} else {
    # 英文版: 产物留在 build_test, 由同步流程拷入 backup/DesolationSystem_EN
    Write-Host "DivineGraceUI (en) built: $dll" -ForegroundColor Green
}
