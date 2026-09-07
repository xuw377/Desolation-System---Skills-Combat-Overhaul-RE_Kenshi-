# build_skeleton_limbs.ps1
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\env_loader.ps1"
$gameModsDir = "D:\steam\steamapps\common\Kenshi\mods"
$p = "SkeletonLimbHotSwap"

Write-Host "=== Building Plugin: $p ===" -ForegroundColor Cyan
$src = "D:\kenshimods\KenshiPluginDev\$p\$p.cpp"
$obj = "D:\kenshimods\KenshiPluginDev\build_test\$p.obj"
$dll = "D:\kenshimods\KenshiPluginDev\build_test\$p.dll"

$libArgs = $libPaths | ForEach-Object { "/LIBPATH:$_" }

& $clang /c /O2 /EHsc -w "/I$kenshiLibInc" "/I$boostInc" "/I$msvcInc" "/I$sdkInc\ucrt" "/I$sdkInc\um" "/I$sdkInc\shared" "/Fo$obj" "$src"
if ($LASTEXITCODE -ne 0) { 
    Write-Host "Compilation failed for $p with code $LASTEXITCODE" -ForegroundColor Red
    exit 1
}

& $lld /DLL "/OUT:$dll" "$obj" @libArgs KenshiLib.lib MyGUIEngine_x64.lib kernel32.lib user32.lib libucrt.lib libvcruntime.lib libcmt.lib
if ($LASTEXITCODE -ne 0) { 
    Write-Host "Linking failed for $p with code $LASTEXITCODE" -ForegroundColor Red
    exit 1
}

$targetDir = "$gameModsDir\$p"
if (-not (Test-Path $targetDir)) { New-Item -ItemType Directory -Force -Path $targetDir | Out-Null }
Copy-Item -Path $dll -Destination "$targetDir\$p.dll" -Force
Set-Content -Path "$targetDir\RE_Kenshi.json" -Value '{"Plugins":["SkeletonLimbHotSwap.dll"]}' -Encoding ASCII

# (P3 治理: 不再覆盖 .mod - 曾误用 CrossbowSynergy 模板污染本地 mod 数据, 需手动维护)
Write-Host "[SUCCESS] $p deployed successfully to $targetDir ! (.mod 未改动)" -ForegroundColor Green
