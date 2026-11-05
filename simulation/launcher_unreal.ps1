# Starts the native Unreal simulator. No external physics service is required.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$projectDir = Join-Path $root 'unreal\SimPlaneurUE'
$project = Join-Path $projectDir 'SimPlaneurUE.uproject'
$version = (Get-Content -Raw -LiteralPath $project | ConvertFrom-Json).EngineAssociation
$engineName = "UE_$version"
$locations = @()
$manifest = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
if (Test-Path -LiteralPath $manifest) {
    $locations += (Get-Content -Raw -LiteralPath $manifest | ConvertFrom-Json).InstallationList |
        Where-Object AppName -eq $engineName | ForEach-Object InstallLocation
}
$locations += Join-Path $env:ProgramFiles "Epic Games\$engineName"
$editor = $locations | ForEach-Object { Join-Path $_ 'Engine\Binaries\Win64\UnrealEditor.exe' } |
    Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (!$editor) { throw "Unreal Engine $version introuvable." }
$commandlet = Join-Path (Split-Path $editor) 'UnrealEditor-Cmd.exe'
$engineRoot = [IO.Path]::GetFullPath((Join-Path (Split-Path $editor) '..\..\..'))
$module = Join-Path $projectDir 'Binaries\Win64\UnrealEditor-SimPlaneurUE.dll'
if (($args -contains '--prepare') -or !(Test-Path -LiteralPath $module)) {
    $build = Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'
    & $build SimPlaneurUEEditor Win64 Development $project -WaitMutex
    if ($LASTEXITCODE -ne 0) { throw 'Compilation Unreal impossible. Verifier Visual Studio Build Tools C++ et le SDK Windows.' }
}

# Epic template content is installed locally, not redistributed in the public repository.
$treeSource = Join-Path $engineRoot 'Templates\TemplateResources\Standard\ArchVis\Content\SampleScene\Tree'
$treeDestination = Join-Path $projectDir 'Content\ArchVis\SampleScene\Tree'
if (!(Test-Path -LiteralPath $treeSource)) {
    throw 'Ressources ArchVis introuvables. Installer les modeles et packs de fonctionnalites Unreal dans Epic Games Launcher.'
}
foreach ($asset in Get-ChildItem -LiteralPath $treeSource -File -Recurse) {
    $relative = $asset.FullName.Substring($treeSource.Length).TrimStart('\')
    $destination = Join-Path $treeDestination $relative
    if (!(Test-Path -LiteralPath $destination)) {
        [IO.Directory]::CreateDirectory((Split-Path $destination)) | Out-Null
        Copy-Item -LiteralPath $asset.FullName -Destination $destination
    }
}
if ($args -contains '--test-physics') {
    $work = [IO.Directory]::CreateDirectory((Join-Path $root 'work')).FullName
    $testLog = Join-Path $work "physics-tests-$PID.log"
    & $commandlet $project '-ExecCmds=Automation RunTests SimPlaneur.Physics' '-TestExit=Automation Test Queue Empty' "-abslog=$testLog" -unattended -nop4 -nosplash -NullRHI
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $testEvents = Select-String -LiteralPath $testLog -Pattern 'Test Completed\. Result=|NATIVE_COMPARE |VALLEY_AIR |Automation Test Queue Empty [0-9]+ tests performed'
    $testEvents | ForEach-Object { Write-Output $_.Line }
    # TestExit can return success even when an individual automation test failed.
    if (!($testEvents -match 'Queue Empty [1-9][0-9]* tests performed') -or ($testEvents -match 'Result=\{Fail')) { exit 1 }
    exit 0
}

function MissingAircraftAssets {
    foreach ($model in "VJX2022","Fox7mEffile","JS3_9m","Snipe2","LongBowMk2","Apus4","Nubis") {
        if ($model -eq 'LongBowMk2' -and !(Test-Path -LiteralPath (Join-Path $projectDir 'SourceAssets/LongBowMk2/LongBowMk2.glb'))) { continue }
        if (!(Test-Path -LiteralPath (Join-Path $projectDir "Content/Aircraft/$model/$model.uasset"))) { $model }
    }
    foreach ($prefix in 'Topy','K3600') {
        @("${prefix}Couleur","M_${prefix}Peinture","T_${prefix}_color","T_${prefix}_normal","T_${prefix}_metallic","T_${prefix}_roughness") |
            Where-Object { !(Test-Path -LiteralPath (Join-Path $projectDir "Content\Aircraft\$_.uasset")) }
    }
}
if (MissingAircraftAssets) {
    $embeddedImporter = Join-Path $projectDir 'Scripts\import_topy.py'
    & $commandlet $project "-ExecutePythonScript=$embeddedImporter" -unattended -nop4
    if ($LASTEXITCODE -ne 0 -or (MissingAircraftAssets)) { throw "Echec de l'import des planeurs dans Unreal." }
}

if ($args -contains '--prepare') {
    Write-Host 'Projet compile et ressources pretes. Lancer avec Lancer-Unreal.cmd.'
    exit 0
}

if ($args.Count -eq 0) {
    # A double-click should leave only the simulator window on the desktop.
    Start-Process -FilePath $editor -ArgumentList ('"{0}" -game' -f $project)
    exit 0
}

$gameArgs = @($project, '-game')
if ($args -contains '--capture-site') { $gameArgs += '-CaptureSite' }
if ($args -contains '--smoke') {
    $gameArgs += '-SimSmoke','-windowed','-ResX=1280','-ResY=800'
    if ($args -contains '--menu-proof') {
        $work = [IO.Directory]::CreateDirectory((Join-Path $root 'work')).FullName
        $gameArgs += '-MenuProof', "-GameUserSettingsINI=$(Join-Path $work 'menu-proof-settings.ini')"
    }
    if ($args -contains '--bungee-proof') {
        $work = [IO.Directory]::CreateDirectory((Join-Path $root 'work')).FullName
        $bungeeLog = Join-Path $work 'bungee-proof.log'
        $gameArgs += "-abslog=$bungeeLog", '-BungeeProof', "-GameUserSettingsINI=$(Join-Path $work 'bungee-proof-settings.ini')"
    }
    if ($args -contains '--meadow-proof') { $gameArgs += '-MeadowProof' }
    if ($args -contains '--pilot-proof') {
        $work = [IO.Directory]::CreateDirectory((Join-Path $root 'work')).FullName
        $pilotLog = Join-Path $work 'pilot-proof.log'
        $gameArgs += "-abslog=$pilotLog", '-PilotProof', "-GameUserSettingsINI=$(Join-Path $work 'pilot-proof-settings.ini')"
    }
    if ($args -contains '--cirque-proof') {
        $work = [IO.Directory]::CreateDirectory((Join-Path $root 'work')).FullName
        $cirqueLog = Join-Path $work 'cirque-proof.log'
        $gameArgs += "-abslog=$cirqueLog", '-CirqueProof', "-GameUserSettingsINI=$(Join-Path $work 'cirque-proof-settings.ini')"
    }
    if ($args -contains '--landing-proof') {
        $work = [IO.Directory]::CreateDirectory((Join-Path $root 'work')).FullName
        $landingLog = Join-Path $work 'landing-proof.log'
        $gameArgs += "-abslog=$landingLog", '-LandingProof', "-GameUserSettingsINI=$(Join-Path $work 'landing-proof-settings.ini')"
    }
    if ($args -contains '--winch-proof') {
        $work = [IO.Directory]::CreateDirectory((Join-Path $root 'work')).FullName
        $winchLog = Join-Path $work 'winch-proof.log'
        $gameArgs += "-abslog=$winchLog", '-WinchProof', "-GameUserSettingsINI=$(Join-Path $work 'winch-proof-settings.ini')"
    }
    if ($args -contains '--contact-proof') { $gameArgs += '-ContactProof' }
    if ($args -contains '--offscreen') {
        $gameArgs += '-RenderOffscreen','-unattended','-NoSplash','-ForceRes','-ExecCmds=r.SetRes 1280x800w'
    }
}
& $editor @gameArgs | Out-Host
if ($args -contains '--bungee-proof') {
    $events = Select-String -LiteralPath $bungeeLog -Pattern 'BUNGEE_PROOF'
    $events | ForEach-Object { Write-Output $_.Line }
    if (!($events -match 'BUNGEE_PROOF complete') -or ($events -match 'success=0')) { exit 1 }
}
if ($args -contains '--winch-proof') {
    $events = Select-String -LiteralPath $winchLog -Pattern 'WINCH_PROOF'
    $events | ForEach-Object { Write-Output $_.Line }
    if (!($events -match 'WINCH_PROOF complete') -or ($events -match 'success=0')) { exit 1 }
}
if ($args -contains '--landing-proof') {
    $events = Select-String -LiteralPath $landingLog -Pattern 'LANDING_PROOF'
    $events | ForEach-Object { Write-Output $_.Line }
    if (!($events -match 'LANDING_PROOF complete') -or ($events -match 'success=0')) { exit 1 }
}
if ($args -contains '--pilot-proof') {
    $events = Select-String -LiteralPath $pilotLog -Pattern 'PILOT_PROOF'
    $events | ForEach-Object { Write-Output $_.Line }
    if (!($events -match 'PILOT_PROOF complete') -or ($events -match 'success=0')) { exit 1 }
}
if ($args -contains '--cirque-proof') {
    $events = Select-String -LiteralPath $cirqueLog -Pattern 'CIRQUE_PROOF'
    $events | ForEach-Object { Write-Output $_.Line }
    if (!($events -match 'CIRQUE_PROOF complete') -or ($events -match 'success=0')) { exit 1 }
}
exit $LASTEXITCODE
