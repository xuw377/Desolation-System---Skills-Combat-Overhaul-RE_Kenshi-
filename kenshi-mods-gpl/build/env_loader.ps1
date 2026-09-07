# =============================================================================
# env_loader.ps1 - unified environment source loader (P1 governance)
# Usage: dot-source at top of any build_*.ps1:  . "$PSScriptRoot\env_loader.ps1"
# Reads D:\kenshimods\env.json (UTF-8). Falls back to built-in defaults (same
# values as the old hardcoded ones) when missing/corrupt.
# NOTE: keep this file ASCII-only so it parses under Windows PowerShell 5.1
# regardless of BOM.
#
# Exposed variables:
#   $clang $lld $kenshiLibInc $msvcInc $sdkInc $boostInc $libPaths
#   $repoA $repoB $gameRoot $modsDir $outDir $dataDir $kenshiLibDll $modsCfg
#   $desoModDir $desoIni $kenshiLibLib $msvcLib $sdkLibUcrt $sdkLibUm $gameModsDir
# =============================================================================
$envJsonPath = 'D:/kenshimods/env.json'
$envCfg = $null
if (Test-Path $envJsonPath) {
    try { $envCfg = Get-Content $envJsonPath -Raw -Encoding UTF8 | ConvertFrom-Json } catch { $envCfg = $null }
    if ($envCfg) { Write-Host "[env_loader] loaded env from $envJsonPath" -ForegroundColor DarkGray }
    else { Write-Host "[env_loader] WARN: env.json parse failed, using defaults" -ForegroundColor Yellow }
} else {
    Write-Host "[env_loader] WARN: $envJsonPath not found, using defaults" -ForegroundColor Yellow
}

function Get-EnvVal {
    param([string]$Section, [string]$Key, [string]$Fallback)
    if ($envCfg -and $envCfg.$Section -and $envCfg.$Section.PSObject.Properties.Name -contains $Key) {
        $v = $envCfg.$Section.$Key
        if ($null -ne $v) { return ([string]$v).Replace('/', '\') }
    }
    return $Fallback
}

# --- toolchain (legacy variable names kept) ---
$clang        = Get-EnvVal 'toolchain' 'clang'        'C:\Program Files\LLVM\bin\clang-cl.exe'
$lld          = Get-EnvVal 'toolchain' 'lld'          'C:\Program Files\LLVM\bin\lld-link.exe'
$kenshiLibInc = Get-EnvVal 'paths'     'kenshiLibInclude' 'D:\kenshimods\KenshiPluginDev\KenshiLib_Examples_deps\kenshilib\Include'
$msvcInc      = Get-EnvVal 'toolchain' 'msvcInclude'  'C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133\include'
$sdkInc       = Get-EnvVal 'toolchain' 'sdkInclude'   'D:\kenshimods\toolchain\nuget_pkgs\sdkcpp\c\Include\10.0.28000.0'
$boostInc     = Get-EnvVal 'toolchain' 'boostInclude' 'D:\kenshimods\toolchain\boost_1_60_0'

# --- base paths ---
$repoA        = Get-EnvVal 'paths' 'repoA'        'D:\kenshimods'
$repoB        = Get-EnvVal 'paths' 'repoB'        'D:\kenshimods\KenshiPluginDev'
$gameRoot     = Get-EnvVal 'paths' 'gameRoot'     'D:\steam\steamapps\common\Kenshi'
$modsDir      = Get-EnvVal 'paths' 'modsDir'      'D:\steam\steamapps\common\Kenshi\mods'
$dataDir      = Get-EnvVal 'paths' 'dataDir'      'D:\steam\steamapps\common\Kenshi\data'
$kenshiLibDll = Get-EnvVal 'paths' 'kenshiLibDll' 'D:\steam\steamapps\common\Kenshi\KenshiLib.dll'
$modsCfg      = Get-EnvVal 'paths' 'modsCfg'      'D:\steam\steamapps\common\Kenshi\data\mods.cfg'
$desoModDir   = Get-EnvVal 'paths' 'desoModDir'   'D:\steam\steamapps\common\Kenshi\mods\DesolationSystem'
$desoIni      = Get-EnvVal 'paths' 'desoIni'      'D:\steam\steamapps\common\Kenshi\mods\DesolationSystem\DesolationSystem.ini'
$outDir       = Get-EnvVal 'paths' 'outDir'       'D:\kenshimods\KenshiPluginDev\build_test'

# --- linker search paths (superset; only existing dirs kept) ---
$libPaths = @(
    (Get-EnvVal 'paths'     'depsGen'              'D:\kenshimods\KenshiPluginDev\deps_gen'),
    (Get-EnvVal 'paths'     'depsGenKenshiLibLibs' 'D:\kenshimods\KenshiPluginDev\deps_gen\KenshiLib\Libraries'),
    (Get-EnvVal 'toolchain' 'msvcLib'              'C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133\lib\x64'),
    (Get-EnvVal 'paths'     'depsGenWinlibs'       'D:\kenshimods\toolchain\deps_gen\winlibs')
) | Where-Object { Test-Path $_ }

# --- legacy aliases used by older scripts ---
$kenshiLibLib = Get-EnvVal 'paths'     'depsGen'    'D:\kenshimods\KenshiPluginDev\deps_gen'
$msvcLib      = Get-EnvVal 'toolchain' 'msvcLib'    'C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133\lib\x64'
$sdkLibUcrt   = 'D:\kenshimods\toolchain\nuget_pkgs\sdkcpp\c\Lib\10.0.28000.0\ucrt\x64'
$sdkLibUm     = 'D:\kenshimods\toolchain\nuget_pkgs\sdkcpp\c\Lib\10.0.28000.0\um\x64'
$gameModsDir  = $modsDir
