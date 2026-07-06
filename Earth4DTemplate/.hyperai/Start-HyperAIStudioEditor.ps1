param([switch]$ForceRestart, [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$EngineExe = "D:/EPIC/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe"
$ProjectFile = "C:/Users/minio/Source/Claude Co Work/Earth4D/Unrealistic4D/Earth4DTemplate/Earth4D.uproject"
$Port = 8000
$UrlPath = "/mcp"
if (!(Test-Path -LiteralPath $EngineExe)) { throw "UnrealEditor.exe not found at $EngineExe" }
if (!(Test-Path -LiteralPath $ProjectFile)) { throw "Project file not found at $ProjectFile" }
$escapedProject = $ProjectFile.Replace('\', '\\')
$running = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" | Where-Object { $_.CommandLine -like "*$ProjectFile*" }
if ($ForceRestart -and $running) { $running | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }; $running = $null }
if (-not $running) {
  $args = @($ProjectFile, '-ModelContextProtocolStartServer', "-ModelContextProtocolPort=$Port")
  Start-Process -FilePath $EngineExe -ArgumentList $args -WorkingDirectory (Split-Path -Parent $ProjectFile) | Out-Null
}
& (Join-Path $PSScriptRoot 'Wait-HyperAIStudioMCP.ps1') -Port $Port -UrlPath $UrlPath -TimeoutSeconds $TimeoutSeconds
