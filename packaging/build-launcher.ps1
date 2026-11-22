param([string]$OutputDirectory = '', [switch]$Test)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
if (!$OutputDirectory) { $OutputDirectory = Join-Path $root 'work/launcher' }
$out = [IO.Directory]::CreateDirectory($OutputDirectory).FullName
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ Build Tools introuvable.' }
$vc = Get-ChildItem -LiteralPath (Join-Path $vs 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$sdk = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10'
$sdkVersion = Get-ChildItem -LiteralPath (Join-Path $sdk 'Include') -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'um/Windows.h') } | Sort-Object Name -Descending | Select-Object -First 1
if (!$vc -or !$sdkVersion) { throw 'Compilateur C++ ou SDK Windows introuvable.' }
$includes = @("/I$($vc.FullName)/include")
$libraries = @("/LIBPATH:$($vc.FullName)/lib/x64")
foreach ($part in @('ucrt','shared','um','winrt')) { $includes += "/I$($sdkVersion.FullName)/$part" }
foreach ($part in @('ucrt','um')) { $libraries += "/LIBPATH:$sdk/Lib/$($sdkVersion.Name)/$part/x64" }
$cl = Join-Path $vc.FullName 'bin/Hostx64/x64/cl.exe'
$previousPath = $env:PATH
$env:PATH = "$(Join-Path $sdk "bin/$($sdkVersion.Name)/x64");$previousPath"
Push-Location $out
try {
    $common = @('/nologo','/std:c++17','/EHsc','/utf-8','/W4','/MT','/O2') + $includes
    if ($Test) {
        & $cl @common (Join-Path $PSScriptRoot 'launcher/CompatibilityTests.cpp') '/Fe:CompatibilityTests.exe' /link @libraries
        if ($LASTEXITCODE -ne 0) { throw 'Compilation des tests de compatibilité impossible.' }
        & './CompatibilityTests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Échec des tests de compatibilité.' }
        & $cl @common (Join-Path $PSScriptRoot 'launcher/GameProcessTests.cpp') '/Fe:GameProcessTests.exe' /link @libraries
        if ($LASTEXITCODE -ne 0) { throw 'Compilation des tests du processus de jeu impossible.' }
        & './GameProcessTests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Échec des tests du processus de jeu.' }
    }
    & $cl @common (Join-Path $PSScriptRoot 'launcher/Launcher.cpp') '/Fe:CaPlaneRCLauncher.exe' /link @libraries /SUBSYSTEM:WINDOWS /MANIFEST:EMBED user32.lib shell32.lib ole32.lib comctl32.lib advapi32.lib dxgi.lib d3d11.lib
    if ($LASTEXITCODE -ne 0) { throw 'Compilation du lanceur impossible.' }
} finally { Pop-Location; $env:PATH = $previousPath }
