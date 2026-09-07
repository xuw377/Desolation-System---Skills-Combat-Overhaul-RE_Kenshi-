# build_desolation_all.ps1 — 自动化一键全量编译、打包并部署【绝境系统 (DesolationSystem)】
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\env_loader.ps1"
$gameModsDir = "D:\steam\steamapps\common\Kenshi\mods"
$desolationDir = "$gameModsDir\DesolationSystem"

if (-not (Test-Path $desolationDir)) {
    New-Item -ItemType Directory -Force -Path $desolationDir | Out-Null
}

$plugins = @(
    "PowerSynergy",
    "DodgeSynergy",
    "PerceptionSynergy",
    "ToughnessSynergy",
    "MedicalRoboticsSynergy",
    "WeaponMasterySynergy",
    "CombatMasterySynergy",
    "RangedAthleticsSynergy",
    "CraftingMasterySynergy",
    "CrossbowSynergy",
    "DexCombatSpeed",
    "AttackSpeedCap",
    "SynergyPerks",
    "DivineGraceUI"
)

$libArgs = $libPaths | ForEach-Object { "/LIBPATH:`"$_`"" }

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host " Building & Packaging Unified Mod: 【绝境系统 / DesolationSystem】 " -ForegroundColor Cyan
Write-Host "=========================================================" -ForegroundColor Cyan

foreach ($p in $plugins) {
    Write-Host "-> Building submodule: $p ..." -ForegroundColor Yellow
    $src = "D:\kenshimods\KenshiPluginDev\$p\$p.cpp"
    $obj = "D:\kenshimods\KenshiPluginDev\build_test\$p.obj"
    $dll = "D:\kenshimods\KenshiPluginDev\build_test\$p.dll"

    & $clang /c /O2 /EHsc -w "/I$kenshiLibInc" "/I$msvcInc" "/I$sdkInc\ucrt" "/I$sdkInc\um" "/I$sdkInc\shared" "/Fo$obj" "$src"
    if ($LASTEXITCODE -ne 0) { 
        Write-Host "Compilation failed for $p" -ForegroundColor Red
        exit 1
    }

    $extraLibs = if ($p -eq "DivineGraceUI") { "user32.lib" } else { "" }
    & $lld /DLL "/OUT:$dll" "$obj" @libArgs KenshiLib.lib kernel32.lib $extraLibs libucrt.lib libvcruntime.lib libcmt.lib
    if ($LASTEXITCODE -ne 0) { 
        Write-Host "Linking failed for $p" -ForegroundColor Red
        exit 1
    }

    Copy-Item -Path $dll -Destination "$desolationDir\$p.dll" -Force
}

# 部署统一的 RE_Kenshi.json
$dllList = ($plugins | ForEach-Object { "`"$_`.dll`"" }) -join ",`n    "
$jsonContent = @"
{
  "Plugins": [
    $dllList
  ]
}
"@
Set-Content -Path "$desolationDir\RE_Kenshi.json" -Value $jsonContent -Encoding ASCII

# 复制统一 .mod 模板
$modTemplate = "$gameModsDir\CrossbowSynergy\CrossbowSynergy.mod"
if (Test-Path $modTemplate) { 
    Copy-Item -Path $modTemplate -Destination "$desolationDir\DesolationSystem.mod" -Force 
}

Write-Host "=========================================================" -ForegroundColor Green
Write-Host "【绝境系统】已全量构建并完成整合打包！" -ForegroundColor Green
Write-Host "Mod 路径: $desolationDir" -ForegroundColor Green
Write-Host "=========================================================" -ForegroundColor Green
