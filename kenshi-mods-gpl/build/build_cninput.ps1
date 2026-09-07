. "$PSScriptRoot\env_loader.ps1"
$p = "CnInputPanel"
$dir = "D:\kenshimods\KenshiPluginDev\$p"
$gameModsDir = "D:\steam\steamapps\common\Kenshi\mods"

Write-Host "Building $p ..." -ForegroundColor Cyan
$obj = "D:\kenshimods\KenshiPluginDev\build_test\$p.obj"
$dll = "D:\kenshimods\KenshiPluginDev\build_test\$p.dll"
$libArgs = $libPaths | ForEach-Object { "/LIBPATH:$_" }

& $clang -m64 /c /O2 /MD /std:c++17 /utf-8 /EHsc -w "/I$kenshiLibInc" "/I$msvcInc" "/I$sdkInc\ucrt" "/I$sdkInc\um" "/I$sdkInc\shared" "/Fo$obj" "$dir\$p.cpp"
if ($LASTEXITCODE -ne 0) { Write-Host "Compilation failed: $LASTEXITCODE" -ForegroundColor Red; exit 1 }

& $lld /DLL "/OUT:$dll" "$obj" @libArgs KenshiLib.lib kernel32.lib user32.lib gdi32.lib
if ($LASTEXITCODE -ne 0) { Write-Host "Linking failed: $LASTEXITCODE" -ForegroundColor Red; exit 1 }

# 部署: 独立 mod 文件夹 (2026-09-06 独立化)
$targetDir = "$gameModsDir\$p"
if (Test-Path $targetDir) {
    Copy-Item -Path $dll -Destination "$targetDir\$p.dll" -Force
    Copy-Item -Path "$dir\CnInputPinyin.txt" -Destination "$targetDir\CnInputPinyin.txt" -Force
}
Write-Host "$p deployed successfully!" -ForegroundColor Green
