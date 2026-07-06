param([string]$Agent = 'Codex', [string]$PromptFile)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $ProjectRoot
$env:TERM = 'xterm-256color'
function Find-HyperAIStudioCommand {
  param([string]$Name)
  $CandidateNames = @($Name)
  if ([IO.Path]::GetExtension($Name) -eq '') { $CandidateNames = @("$Name.exe", "$Name.cmd", "$Name.bat", "$Name.ps1", $Name) }
  $Dirs = @()
  if (-not [string]::IsNullOrWhiteSpace($env:PATH)) { $Dirs += ($env:PATH -split ';') }
  if (-not [string]::IsNullOrWhiteSpace($env:APPDATA)) { $Dirs += (Join-Path $env:APPDATA 'npm') }
  if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    $Dirs += (Join-Path $env:LOCALAPPDATA 'npm')
    $Dirs += (Join-Path $env:LOCALAPPDATA 'Programs\Microsoft VS Code\bin')
    $Dirs += (Join-Path $env:LOCALAPPDATA 'Programs\Cursor\resources\app\bin')
    $Dirs += (Join-Path $env:LOCALAPPDATA 'Programs\cursor\resources\app\bin')
  }
  if (-not [string]::IsNullOrWhiteSpace($env:USERPROFILE)) {
    $Dirs += (Join-Path $env:USERPROFILE '.local\bin')
    $Dirs += (Join-Path $env:USERPROFILE '.claude\local')
    $Dirs += (Join-Path $env:USERPROFILE '.codex\bin')
  }
  if (-not [string]::IsNullOrWhiteSpace(${env:ProgramFiles})) {
    $Dirs += (Join-Path ${env:ProgramFiles} 'Microsoft VS Code\bin')
    $Dirs += (Join-Path ${env:ProgramFiles} 'Cursor\resources\app\bin')
    $Dirs += (Join-Path ${env:ProgramFiles} 'nodejs')
  }
  if (-not [string]::IsNullOrWhiteSpace(${env:ProgramFiles(x86)})) {
    $Dirs += (Join-Path ${env:ProgramFiles(x86)} 'Microsoft VS Code\bin')
    $Dirs += (Join-Path ${env:ProgramFiles(x86)} 'nodejs')
  }
  foreach ($Dir in ($Dirs | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)) {
    foreach ($CandidateName in $CandidateNames) {
      $Candidate = Join-Path $Dir $CandidateName
      if (Test-Path -LiteralPath $Candidate) { return (Resolve-Path -LiteralPath $Candidate).Path }
    }
  }
  $Command = Get-Command $Name -ErrorAction SilentlyContinue
  if ($Command) { return $Command.Source }
  return $null
}
function Invoke-HyperAIStudioCommand {
  param([string]$CommandPath, [string[]]$CommandArgs = @())
  if ([IO.Path]::GetExtension($CommandPath) -eq '.ps1') { & powershell -NoProfile -ExecutionPolicy Bypass -File $CommandPath @CommandArgs }
  else { & $CommandPath @CommandArgs }
}
if ([string]::IsNullOrWhiteSpace($PromptFile)) { throw 'PromptFile is required.' }
if (!(Test-Path -LiteralPath $PromptFile)) { throw "Prompt file not found: $PromptFile" }
$Prompt = Get-Content -Raw -LiteralPath $PromptFile
Set-Clipboard -Value $Prompt
Write-Host 'HyperAIStudio prompt copied to clipboard.' -ForegroundColor Green
Write-Host "Prompt file: $PromptFile"
Write-Host 'Opening the selected agent from the project root when it is available. Paste the copied prompt if the app opens without an initial message.'
$AgentKey = $Agent.ToLowerInvariant()
if ($AgentKey -like '*codex*') {
  $CodexCommand = Find-HyperAIStudioCommand 'codex'
  if ($CodexCommand) {
    $CodexArgs = @()
    $CodexConfigCandidates = @()
    $CodexConfigCandidates += (Join-Path $ProjectRoot '.codex\config.toml')
    if (-not [string]::IsNullOrWhiteSpace($env:CODEX_HOME)) { $CodexConfigCandidates += (Join-Path $env:CODEX_HOME 'config.toml') }
    if (-not [string]::IsNullOrWhiteSpace($env:USERPROFILE)) { $CodexConfigCandidates += (Join-Path $env:USERPROFILE '.codex\config.toml') }
    if (-not [string]::IsNullOrWhiteSpace($HOME)) { $CodexConfigCandidates += (Join-Path $HOME '.codex\config.toml') }
    foreach ($CodexConfig in ($CodexConfigCandidates | Select-Object -Unique)) {
      if ((Test-Path -LiteralPath $CodexConfig) -and ((Get-Content -Raw -LiteralPath $CodexConfig) -match "(?im)^\s*service_tier\s*=\s*['`"](?!fast['`"]|flex['`"])[^'`"]+['`"]")) {
        $CodexArgs += @('-c', 'service_tier=fast')
        Write-Host 'Codex config contains an unsupported service_tier; launching with service_tier=fast override.' -ForegroundColor Yellow
        break
      }
    }
    $CodexArgs += @('app', $ProjectRoot)
    Write-Host "Opening Codex Desktop from the project root: $CodexCommand" -ForegroundColor Cyan; Invoke-HyperAIStudioCommand $CodexCommand $CodexArgs
  }
  else { Write-Host 'Codex CLI was not found in PATH or common user install folders.' -ForegroundColor Yellow }
} elseif ($AgentKey -like '*claude*') {
  $ClaudeCommand = Find-HyperAIStudioCommand 'claude'
  if ($ClaudeCommand) { Write-Host "Opening Claude Code from the project root: $ClaudeCommand" -ForegroundColor Cyan; Invoke-HyperAIStudioCommand $ClaudeCommand }
  else { Write-Host 'Claude Code CLI was not found in PATH or common user install folders.' -ForegroundColor Yellow }
} elseif ($AgentKey -like '*gemini*') {
  $GeminiCommand = Find-HyperAIStudioCommand 'gemini'
  if ($GeminiCommand) { Write-Host "Opening Gemini from the project root: $GeminiCommand" -ForegroundColor Cyan; Invoke-HyperAIStudioCommand $GeminiCommand }
  else { Write-Host 'Gemini CLI was not found in PATH or common user install folders.' -ForegroundColor Yellow }
} elseif ($AgentKey -like '*cursor*') {
  $CursorCommand = Find-HyperAIStudioCommand 'cursor'
  if ($CursorCommand) { Write-Host "Opening Cursor from the project root: $CursorCommand" -ForegroundColor Cyan; Invoke-HyperAIStudioCommand $CursorCommand @('.') }
  else { Write-Host 'Cursor CLI was not found in PATH or common user install folders. Open Cursor manually from this project root and paste the copied prompt.' -ForegroundColor Yellow }
} elseif ($AgentKey -like '*vscode*' -or $AgentKey -like '*vs code*' -or $AgentKey -like '*copilot*') {
  $CodeCommand = Find-HyperAIStudioCommand 'code'
  if ($CodeCommand) { Write-Host "Opening VS Code from the project root: $CodeCommand" -ForegroundColor Cyan; Invoke-HyperAIStudioCommand $CodeCommand @('.') }
  else { Write-Host 'VS Code CLI was not found in PATH or common user install folders. Open VS Code manually from this project root and paste the copied prompt into Copilot Agent mode.' -ForegroundColor Yellow }
}
