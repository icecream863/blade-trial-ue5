param([string]$EngineRoot = 'E:\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $EngineRoot 'Engine\Source\Programs\UnrealBuildTool\UnrealBuildTool.cs'
$projectPath = Join-Path $EngineRoot 'Engine\Source\Programs\UnrealBuildTool\UnrealBuildTool.csproj'
$binaryDirectory = Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool'
$dotnet = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
$repairDirectory = Join-Path $projectRoot ('Saved\Repairs\UbtTrace-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $repairDirectory -Force | Out-Null
$source = [IO.File]::ReadAllText($sourcePath)
$originalAttributes = [IO.File]::GetAttributes($sourcePath)
$old = "`t`t`t`tLog.BackupLogFile(traceFileRef);"
$previous = @'
				// SCL_TRACE_BACKUP_RACE_FIX: another UBT process may rotate the file after Exists().
				try
				{
					Log.BackupLogFile(traceFileRef);
				}
				catch (FileNotFoundException)
				{
					Logger.LogDebug("Trace backup disappeared during rotation: {TraceFile}", traceFileRef);
				}
'@
$new = @'
				// SCL_TRACE_BACKUP_RACE_FIX_V2: serialize diagnostic backup rotation before the build mutex.
				using (System.Threading.Mutex traceBackupMutex = new System.Threading.Mutex(false, "Global\\UnrealBuildTool_TraceBackup"))
				{
					try { traceBackupMutex.WaitOne(); }
					catch (System.Threading.AbandonedMutexException) { /* Ownership is granted after abandonment. */ }
					try
					{
						Log.BackupLogFile(traceFileRef);
					}
					catch (IOException exception)
					{
						Logger.LogWarning("Trace backup rotation failed; continuing build: {Reason}", exception.Message);
					}
					catch (UnauthorizedAccessException exception)
					{
						Logger.LogWarning("Trace backup rotation denied; continuing build: {Reason}", exception.Message);
					}
					finally { traceBackupMutex.ReleaseMutex(); }
				}
'@
if ($source.Contains('SCL_TRACE_BACKUP_RACE_FIX_V2')) { throw 'Trace race patch is already installed.' }
if ($source.Contains('SCL_TRACE_BACKUP_RACE_FIX')) {
    if (-not $source.Contains($previous)) { throw 'Unknown previous trace patch; refusing to overwrite.' }
    $old = $previous
}
if (($source.Split(@($old), [StringSplitOptions]::None)).Length -ne 2) { throw 'Unexpected UBT source; refusing to patch.' }

# 只处理已确认的 trace 备份竞态；保留构建互斥、权限错误和其他异常的原有行为。
Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $repairDirectory 'UnrealBuildTool.cs.original')
foreach ($file in 'UnrealBuildTool.dll','UnrealBuildTool.pdb') {
    $path = Join-Path $binaryDirectory $file
    if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $repairDirectory }
}
$output = Join-Path $repairDirectory 'Compiled'
# 安装版没有原 csproj 的 NuGet 还原文件。复用随引擎分发的同版本依赖，
# 在项目 Saved 中构建 UBT，避免重建或替换 EpicGames.Core 等共享程序集。
$sourceDirectory = Split-Path -Parent $sourcePath
$metadata = Join-Path (Split-Path -Parent $sourceDirectory) 'Shared\MetaData.cs'
$referenceDirectory = Join-Path (Split-Path -Parent $dotnet) 'packs\Microsoft.NETCore.App.Ref'
$referencePack = Get-ChildItem -LiteralPath $referenceDirectory -Directory | Sort-Object Name -Descending | Select-Object -First 1
$frameworkReferences = Join-Path $referencePack.FullName 'ref\net10.0'
$references = foreach ($dll in Get-ChildItem -LiteralPath $binaryDirectory -Filter '*.dll' -File) {
    if ($dll.Name -eq 'UnrealBuildTool.dll' -or (Test-Path -LiteralPath (Join-Path $frameworkReferences $dll.Name))) { continue }
    $escaped = [Security.SecurityElement]::Escape($dll.FullName)
    "<Reference Include=`"$($dll.BaseName)`"><HintPath>$escaped</HintPath><Private>false</Private></Reference>"
}
$compileProject = Join-Path $repairDirectory 'UnrealBuildTool.csproj'
$escapedSource = [Security.SecurityElement]::Escape($sourceDirectory)
$escapedMetadata = [Security.SecurityElement]::Escape($metadata)
$projectText = @"
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework><OutputType>Exe</OutputType>
    <EnableDefaultCompileItems>false</EnableDefaultCompileItems><GenerateAssemblyInfo>false</GenerateAssemblyInfo>
    <GenerateTargetFrameworkAttribute>false</GenerateTargetFrameworkAttribute><Nullable>enable</Nullable>
    <DefineConstants>TRACE</DefineConstants><AllowUnsafeBlocks>true</AllowUnsafeBlocks><NuGetAudit>false</NuGetAudit>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="$escapedSource\**\*.cs" Exclude="$escapedSource\obj\**;$escapedSource\bin\**" />
    <Compile Include="$escapedMetadata" />
    $($references -join "`n")
  </ItemGroup>
</Project>
"@
[IO.File]::WriteAllText($compileProject, $projectText, [Text.UTF8Encoding]::new($false))
try {
    [IO.File]::SetAttributes($sourcePath, $originalAttributes -band (-bnot [IO.FileAttributes]::ReadOnly))
    [IO.File]::WriteAllText($sourcePath, $source.Replace($old,$new), [Text.UTF8Encoding]::new($false))
    & $dotnet build $compileProject --configuration Release --output $output "-p:RestoreSources=$repairDirectory"
    if ($LASTEXITCODE -ne 0) { throw 'Patched UBT compilation failed.' }
    foreach ($file in 'UnrealBuildTool.dll','UnrealBuildTool.pdb') {
        $path = Join-Path $output $file
        if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination (Join-Path $binaryDirectory $file) -Force }
    }
    Write-Output "UBT trace race patch installed. Original files: $repairDirectory"
}
catch {
    Copy-Item -LiteralPath (Join-Path $repairDirectory 'UnrealBuildTool.cs.original') -Destination $sourcePath -Force
    foreach ($file in 'UnrealBuildTool.dll','UnrealBuildTool.pdb') {
        $path = Join-Path $repairDirectory $file
        if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination (Join-Path $binaryDirectory $file) -Force }
    }
    throw
}
finally {
    [IO.File]::SetAttributes($sourcePath, $originalAttributes)
}
