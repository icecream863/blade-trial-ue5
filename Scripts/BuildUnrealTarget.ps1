param(
    [Parameter(Mandatory = $true)][string]$EngineRoot,
    [ValidateSet('SoulCombatLabEditor', 'SoulCombatLab')][string]$Target = 'SoulCombatLabEditor',
    [ValidateSet('Win64')][string]$Platform = 'Win64',
    [ValidateSet('Development', 'DebugGame', 'Shipping')][string]$Configuration = 'Development'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'SoulCombatLab.uproject'
$buildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if (-not (Test-Path -LiteralPath $buildTool)) {
    throw 'EngineRoot must point to an Unreal Engine installation.'
}

# UE 5.8 使用 Windows 系统目录 API，修改 LOCALAPPDATA 不会改变启动 trace 的路径。
# 先只打开并关闭现有文件，不改写内容；让权限错误在脚本中清楚显示，避免 UBT 未捕获异常。
$traceDirectory = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'UnrealBuildTool'
if (Test-Path -LiteralPath $traceDirectory) {
    foreach ($traceFile in Get-ChildItem -LiteralPath $traceDirectory -Filter 'Trace-backup-*.uba' -File) {
        try {
            $stream = [IO.File]::Open($traceFile.FullName, [IO.FileMode]::Open,
                [IO.FileAccess]::ReadWrite, [IO.FileShare]::ReadWrite)
            $stream.Dispose()
        }
        catch {
            # 其他 UBT 进程可能在枚举后轮换或清除旧备份，已不存在的文件无需检查。
            if (-not (Test-Path -LiteralPath $traceFile.FullName)) { continue }
            throw "UBT trace 文件无法写入：$($traceFile.FullName)。当前进程需要该目录的写入权限；构建尚未启动。原始错误：$($_.Exception.Message)"
        }
    }
}

$buildLogDirectory = Join-Path $projectRoot 'Saved\BuildLogs'
New-Item -ItemType Directory -Force -Path $buildLogDirectory | Out-Null
$buildLog = Join-Path $buildLogDirectory "$Target-$Platform-$Configuration.log"
& $buildTool $Target $Platform $Configuration "-Project=$projectFile" "-Log=$buildLog" -WaitMutex -MaxParallelActions=2 -NoUBA -NoUBTMakefiles
if ($LASTEXITCODE -ne 0) {
    throw "Build failed: $Target ($LASTEXITCODE). Log: $buildLog"
}
