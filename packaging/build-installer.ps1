param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$Version = '0.5.4',
    [switch]$SkipPackage
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$project = Join-Path $root 'unreal\SimPlaneurUE\SimPlaneurUE.uproject'
$archive = Join-Path $root 'work\package'
if (!$SkipPackage) {
    & (Join-Path $root 'Installer-Unreal.cmd')
    if ($LASTEXITCODE -ne 0) { throw 'Preparation des sources impossible.' }
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat') BuildCookRun "-project=$project" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -prereqs -nodebuginfo -unattended -utf8output -nocompileeditor '-map=/Engine/Maps/Entry' "-archivedirectory=$archive"
    if ($LASTEXITCODE -ne 0) { throw 'Echec de la compilation du jeu autonome.' }
}
$game = Join-Path $archive 'Windows'
if (!(Test-Path -LiteralPath (Join-Path $game 'SimPlaneurUE.exe'))) { throw 'Jeu autonome absent.' }
$candidates = @(
    (Join-Path $root 'tools\InnoSetup\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
)
$compiler = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (!$compiler) { throw 'Installer Inno Setup 6, ou placer ISCC.exe dans tools\InnoSetup.' }
$release = [IO.Directory]::CreateDirectory((Join-Path $root 'work\release')).FullName
& $compiler "/DGameDir=$game" "/DAppVersion=$Version" "/DOutputDir=$release" (Join-Path $PSScriptRoot 'ca-plane-rc.iss')
if ($LASTEXITCODE -ne 0) { throw 'Echec de creation de l installateur.' }
$installer = Join-Path $release "CaPlaneRC-$Version-Windows-Setup.exe"
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $release 'SHA256SUMS.txt'), "$hash  $([IO.Path]::GetFileName($installer))`n")
Write-Host "Installateur : $installer"
