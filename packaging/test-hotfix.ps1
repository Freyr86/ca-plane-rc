param([string]$Version='0.5.10')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$proof=[IO.Directory]::CreateDirectory((Join-Path $root 'work/hotfix-install-proof')).FullName
$installed=Join-Path $proof 'installed'
$game=Join-Path $root 'work/package/Windows'
$source=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'ca-plane-rc.iss')
# The fixture never replaces the user's installation or creates shortcuts.
$source=$source.Replace('AppId={{52B9BAE4-7204-4D88-95EC-C2CD5F225820}','AppId={{26F9F799-FA26-490F-9309-D34631873522}')
$source=$source.Replace('SetupLogging=yes',"SetupLogging=yes`nCreateUninstallRegKey=no")
$source=$source.Replace('CloseApplications=yes','CloseApplications=no')
$source=$source.Replace('..\unreal\SimPlaneurUE\Build\Windows\Application.ico',(Join-Path $root 'unreal/SimPlaneurUE/Build/Windows/Application.ico'))
$source=[regex]::Replace($source,'(?s)\[Icons\].*?(?=\[Run\])',"[Icons]`n`n")
$iss=Join-Path $proof 'proof.iss'
[IO.File]::WriteAllText($iss,$source,[Text.UTF8Encoding]::new($true))
& (Join-Path $root 'tools/InnoSetup/ISCC.exe') "/DGameDir=$game" "/DAppVersion=$Version" "/DOutputDir=$proof" $iss *> (Join-Path $proof 'compile.log')
if($LASTEXITCODE -ne 0) { throw 'Test installer compilation failed.' }
$installArgs='/SP- /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /NOICONS /DIR="'+$installed+'" /LOG="'+(Join-Path $proof 'install.log')+'"'
$p=Start-Process -FilePath (Join-Path $proof "CaPlaneRC-$Version-Windows-Setup.exe") -ArgumentList $installArgs -WindowStyle Hidden -PassThru -Wait
if($p.ExitCode -ne 0) { throw "Test installation failed: $($p.ExitCode)" }
foreach($relative in @('CaPlaneRCLauncher.exe','SimPlaneurUE/Binaries/Win64/SimPlaneurUE.exe','SimPlaneurUE/Content/Paks/SimPlaneurUE-Windows.ucas')) {
    if((Get-FileHash -LiteralPath (Join-Path $installed $relative)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $game $relative)).Hash) { throw "Installed content differs: $relative" }
}
if((Get-Content -Raw -LiteralPath (Join-Path $installed 'CaPlaneRC-version.txt')) -ne $Version) { throw 'Installed version differs.' }
Write-Output 'INSTALL_PROOF files_match=1'

foreach($scenario in @('CompatibilityProof','MenuProof')) {
    $log=Join-Path $proof "$scenario.log"
    $settings=Join-Path $proof "$scenario.ini"
    $profile=Join-Path $proof "$scenario-user"
    $flags='-dx12 -MenuProof'
    if($scenario -eq 'CompatibilityProof') {
        $flags='-dx11 -MenuProof -CompatibilityProof'
        $fixture=@'
[/Script/Engine.GameUserSettings]
Version=5
bUseVSync=False
FrameRateLimit=0.000000
ResolutionSizeX=1280
ResolutionSizeY=720
LastUserConfirmedResolutionSizeX=1280
LastUserConfirmedResolutionSizeY=720
FullscreenMode=2
LastConfirmedFullscreenMode=2
PreferredFullscreenMode=2

[ScalabilityGroups]
sg.ResolutionQuality=100
sg.ViewDistanceQuality=4
sg.AntiAliasingQuality=4
sg.ShadowQuality=4
sg.GlobalIlluminationQuality=4
sg.ReflectionQuality=4
sg.PostProcessQuality=4
sg.TextureQuality=4
sg.EffectsQuality=4
sg.FoliageQuality=4
sg.ShadingQuality=4
sg.LandscapeQuality=4

[SimPlaneur.Graphics]
GrassDistanceMeters=500

[SimPlaneur.Updates]
Automatic=False

[Hotfix.Preservation]
Sentinel=KeepMyPreferences
'@
        [IO.File]::WriteAllText($settings,$fixture,[Text.UTF8Encoding]::new($false))
    }
    $runArgs='-unattended '+$flags+' -SimSmoke -RenderOffscreen -NoSplash -windowed -ResX=1280 -ResY=800 -ForceRes -GameUserSettingsINI="'+$settings+'" -abslog="'+$log+'" -UserDir="'+$profile+'"'
    $p=Start-Process -FilePath (Join-Path $installed 'CaPlaneRCLauncher.exe') -ArgumentList $runArgs -WindowStyle Hidden -PassThru
    if(!$p.WaitForExit(600000)) { throw "$scenario exceeded ten minutes; inspect the running fixture before retrying." }
    if($p.ExitCode -ne 0) { throw "$scenario returned $($p.ExitCode) through launcher." }
    $expected=if($scenario -eq 'MenuProof') {'MENU_PROOF step=69 pass=1'} else {'COMPATIBILITY_PROOF complete success=1'}
    if(!(Select-String -LiteralPath $log -SimpleMatch $expected -Quiet)) { throw "$scenario completion marker absent." }
    if($scenario -eq 'CompatibilityProof') {
        foreach($entry in @('sg.TextureQuality=4','bUseVSync=False','FrameRateLimit=0.000000','GrassDistanceMeters=500','Sentinel=KeepMyPreferences')) {
            if(!(Select-String -LiteralPath $settings -SimpleMatch $entry -Quiet)) { throw "Saved preference changed: $entry" }
        }
    }
    Write-Output "$scenario complete launcher_exit=0"
}
$uninstaller=Join-Path $installed 'unins000.exe'
$p=Start-Process -FilePath $uninstaller -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART' -WindowStyle Hidden -PassThru -Wait
if($p.ExitCode -ne 0 -or (Test-Path -LiteralPath (Join-Path $installed 'CaPlaneRCLauncher.exe'))) { throw 'Test uninstall incomplete.' }
Write-Output 'INSTALL_PROOF uninstall=1 complete=1'
