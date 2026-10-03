param([switch]$Screenshots, [string]$EvidenceName = 'SixRegion')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$binary = Join-Path $projectRoot 'Builds\PlayableDemo\Windows\SoulCombatLab\Binaries\Win64\SoulCombatLab.exe'
if (-not (Test-Path -LiteralPath $binary)) { throw 'Build the playable demo first.' }
$userDirectory = Join-Path $projectRoot ('Saved\Validation\' + $EvidenceName + 'PackageUserFinal')
$evidenceDirectory = Join-Path $projectRoot ('Saved\Validation\' + $EvidenceName)
New-Item -ItemType Directory -Path $evidenceDirectory -Force | Out-Null
$results = @()
foreach ($test in @(
    @{Name='SoulCombatLabDemo.RuntimeFlow'; Label='RuntimeFlow'; Log='SixRegionPackagedFlowFinal'; ScreenshotFlag='-SCLDemoScreenshots'},
    @{Name='SoulCombatLabCombat.RuntimePolish'; Label='RuntimePolish'; Log='SixRegionPackagedCombatFinal'; ScreenshotFlag='-SCLCombatScreenshots'},
    @{Name='SoulCombatLabCombat.ComboRecovery'; Label='ComboRecovery'; Log='SixRegionPackagedComboFinal'; ScreenshotFlag=''},
    @{Name='SoulCombatLabCombat.CameraDifficulty'; Label='CameraDifficulty'; Log='SixRegionPackagedCameraFinal'; ScreenshotFlag='-SCLCameraScreenshots'}
)) {
    $logPath = Join-Path $projectRoot ('Saved\Logs\' + $test.Log.Replace('SixRegion', $EvidenceName) + '.log')
    $arguments = @('-unattended', '-RenderOffscreen', '-d3d12', '-nosound', '-culture=en', '-ResX=1280', '-ResY=720', '-Windowed',
        ('-UserDir="' + $userDirectory + '"'),
        ('-ExecCmds="Automation RunTests ' + $test.Name + '"'),
        '-TestExit="Automation Test Queue Empty"', ('-abslog="' + $logPath + '"'))
    if ($Screenshots -and $test.ScreenshotFlag) { $arguments += $test.ScreenshotFlag }
    # GUI executables need explicit waiting; each run stays offscreen and uses isolated settings.
    $process = Start-Process -FilePath $binary -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    $successMarker = 'Test Completed. Result={Success} Name={' + $test.Label + '}'
    $passed = $process.ExitCode -eq 0 -and (Select-String -LiteralPath $logPath -SimpleMatch $successMarker -Quiet)
    $results += [PSCustomObject]@{Test=$test.Name; Passed=[bool]$passed; ExitCode=$process.ExitCode; Log=$logPath}
    Write-Output "$($test.Name): passed=$passed exit=$($process.ExitCode)"
    if (-not $passed) { throw "Validation failed. Inspect $logPath" }
}
$summary = [PSCustomObject]@{
    CompletedAt=(Get-Date -Format o)
    Binary=$binary
    BinarySHA256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
    Tests=$results
    Screenshots=(Join-Path $userDirectory 'Saved\Screenshots')
}
$summary | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidenceDirectory 'PackagedValidation.json') -Encoding utf8
