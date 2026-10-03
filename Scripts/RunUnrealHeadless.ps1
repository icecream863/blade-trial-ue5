param(
    [Parameter(Mandatory = $true)]
    [string] $ProjectPath,

    [Parameter(Mandatory = $true)]
    [string] $PythonScript,

    [string] $EngineRoot = 'E:\Epic Games\UE_5.8',

    [string[]] $AdditionalArgs = @()
)

$ErrorActionPreference = 'Stop'

$project = (Resolve-Path -LiteralPath $ProjectPath).Path
$script = (Resolve-Path -LiteralPath $PythonScript).Path
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) {
    throw "UnrealEditor-Cmd.exe was not found: $editor"
}

$cache = Join-Path (Split-Path -Parent $project) 'Saved\CodexHeadlessDDC'
New-Item -ItemType Directory -Path $cache -Force | Out-Null

# Installed UE uses Zen by default. A filesystem DDC keeps this commandlet
# independent of the interactive editor's Zen process and user AppData cache.
# -Multiprocess skips startup AutoSDK validation, which launches UBT and writes
# its trace files under user AppData. These flags are for headless probes only.
& $editor $project -run=pythonscript "-script=$script" '-DDC=(Local)' "-LocalDataCachePath=$cache" -Multiprocess -unattended -nop4 -nosplash -nullrhi -nosound @AdditionalArgs
if ($LASTEXITCODE -ne 0) {
    throw "Unreal commandlet failed with exit code $LASTEXITCODE"
}
