param([Parameter(Mandatory = $true)][string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'SoulCombatLab.uproject'
$buildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$automationTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path -LiteralPath $buildTool) -or -not (Test-Path -LiteralPath $automationTool)) {
    throw 'EngineRoot must point to an Unreal Engine installation.'
}

# Editor 和 Game 共用构建入口，避免权限检查、日志路径和构建参数重复维护。
foreach ($target in 'SoulCombatLabEditor', 'SoulCombatLab') {
    & (Join-Path $PSScriptRoot 'BuildUnrealTarget.ps1') -EngineRoot $EngineRoot -Target $target
}

# 使用英文 Cook，避免引擎烟雾测试受区域格式影响。
$archivePath = Join-Path $projectRoot 'Builds\PlayableDemo'
& $automationTool BuildCookRun "-project=$projectFile" -noP4 -unattended -utf8output -platform=Win64 -clientconfig=Development -skipbuild -cook -stage -pak -archive "-archivedirectory=$archivePath" '-map=/Game/Maps/L_CombatLab' '-AdditionalCookerOptions=-culture=en'
if ($LASTEXITCODE -ne 0) { throw "Packaging failed ($LASTEXITCODE)" }
Write-Output "Demo: $archivePath\Windows\SoulCombatLab.exe"
