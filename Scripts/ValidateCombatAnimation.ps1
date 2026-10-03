param(
    [string]$EngineRoot = 'E:\Epic Games\UE_5.8',
    [ValidateSet('RuntimePolish','GhostPlayerCombo','LockedLocomotion','WeaponSheath','RuntimeFlow','ComboRecovery','PlayerInput','CameraDifficulty','ActionLifecycle','DirectionalDodge','ParryRecovery','JumpAnimation')]
    [string[]]$Only = @(),
    [ValidatePattern('^[A-Za-z0-9-]+$')][string]$LogPrefix = 'CombatAnimation'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $projectRoot 'SoulCombatLab.uproject'
if (-not (Test-Path -LiteralPath $editor)) { throw "Missing editor: $editor" }
$failures = @()

# 逐项用新进程运行，避免一个用例留下的世界状态影响下一个。
foreach ($entry in @(
    @{ Name='SoulCombatLabCombat.JumpAnimation'; Label='JumpAnimation' },
    @{ Name='SoulCombatLabCombat.ParryRecovery'; Label='ParryRecovery' },
    @{ Name='SoulCombatLabCombat.DirectionalDodge'; Label='DirectionalDodge' },
    @{ Name='SoulCombatLabCombat.ActionLifecycle'; Label='ActionLifecycle' },
    @{ Name='SoulCombatLabCombat.RuntimePolish'; Label='RuntimePolish' },
    @{ Name='SoulCombatLabCombat.GhostPlayerCombo'; Label='GhostPlayerCombo' },
    @{ Name='SoulCombatLabCombat.LockedLocomotion'; Label='LockedLocomotion' },
    @{ Name='SoulCombatLabCombat.WeaponSheath'; Label='WeaponSheath' },
    @{ Name='SoulCombatLabCombat.ComboRecovery'; Label='ComboRecovery' },
    @{ Name='SoulCombatLabCombat.PlayerInput'; Label='PlayerInput' },
    @{ Name='SoulCombatLabCombat.CameraDifficulty'; Label='CameraDifficulty' },
    @{ Name='SoulCombatLabDemo.RuntimeFlow'; Label='RuntimeFlow' }
)) {
    if ($Only.Count -gt 0 -and $entry.Label -notin $Only) { continue }
    $log = Join-Path $projectRoot ('Saved\Logs\' + $LogPrefix + '-' + $entry.Label + '.log')
    $args = @(
        ('"' + $project + '"'), '/Game/Maps/L_CombatLab', '-game', '-unattended', '-nop4',
        '-nosplash', '-nosound', '-Multiprocess', '-RenderOffscreen', '-windowed',
        '-DDC=(Local)', ('-LocalDataCachePath="' + (Join-Path $projectRoot 'Saved\CodexHeadlessDDC') + '"'),
        '-ResX=1280', '-ResY=720', '-TestExit="Automation Test Queue Empty"',
        ('-abslog="' + $log + '"'),
        ('-ExecCmds="t.MaxFPS 60,Automation RunTests ' + $entry.Name + '"')
    )
    $process = Start-Process -FilePath $editor -ArgumentList $args -WindowStyle Hidden -Wait -PassThru
    $marker = 'Test Completed. Result={Success} Name={' + $entry.Label + '}'
    $passed = $process.ExitCode -eq 0 -and (Select-String -LiteralPath $log -SimpleMatch $marker -Quiet)
    Write-Output "$($entry.Label): passed=$passed exit=$($process.ExitCode) log=$log"
    if (-not $passed) { $failures += "$($entry.Name): $log" }
}
if ($failures.Count -gt 0) { throw ($failures -join "`n") }
