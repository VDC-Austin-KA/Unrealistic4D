param([int]$Port = 8000, [string]$UrlPath = '/mcp', [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$Endpoint = "http://127.0.0.1:$Port$UrlPath"
$StatusPath = Join-Path $PSScriptRoot 'hyperai-status.json'
function Write-HyperAIStatus([string]$State, [string]$Message, [int]$ToolCount = 0) {
  [pscustomobject]@{ generatedAt = (Get-Date).ToString('o'); ready = ($State -eq 'Ready'); state = $State; endpoint = $Endpoint; tools = $ToolCount; message = $Message } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $StatusPath -Encoding UTF8
}
function Convert-McpJson([string]$Content) {
  $Text = $Content.Trim()
  if ($Text.StartsWith('event:') -or $Text.StartsWith('data:')) {
    $DataLines = @()
    foreach ($Line in ($Text -split "`r?`n")) { if ($Line.StartsWith('data:')) { $DataLines += $Line.Substring(5).TrimStart() } }
    $Text = ($DataLines -join "`n")
  }
  return $Text | ConvertFrom-Json
}
$Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$LastError = ''
while ((Get-Date) -lt $Deadline) {
  try {
    $Headers = $null
    $InitBody = @{ jsonrpc = '2.0'; id = 1; method = 'initialize'; params = @{ protocolVersion = '2025-11-25'; capabilities = @{}; clientInfo = @{ name = 'HyperAIStudio.Wait'; version = '1.0.0' } } } | ConvertTo-Json -Depth 10
    $Init = Invoke-WebRequest -Uri $Endpoint -Method Post -ContentType 'application/json' -Body $InitBody -UseBasicParsing -TimeoutSec 5
    $SessionId = $Init.Headers['Mcp-Session-Id']
    if ($SessionId -is [array]) { $SessionId = $SessionId[0] }
    if ([string]::IsNullOrWhiteSpace($SessionId)) { throw 'Initialize did not return Mcp-Session-Id.' }
    $Headers = @{ 'Mcp-Session-Id' = $SessionId }
    Invoke-WebRequest -Uri $Endpoint -Method Post -ContentType 'application/json' -Headers $Headers -Body '{"jsonrpc":"2.0","method":"notifications/initialized"}' -UseBasicParsing -TimeoutSec 5 | Out-Null
    $List = Invoke-WebRequest -Uri $Endpoint -Method Post -ContentType 'application/json' -Headers $Headers -Body '{"jsonrpc":"2.0","id":2,"method":"tools/list"}' -UseBasicParsing -TimeoutSec 10
    $Json = Convert-McpJson $List.Content
    $ToolNames = @($Json.result.tools | ForEach-Object { $_.name })
    $ToolCount = @($ToolNames).Count
    try { Invoke-WebRequest -Uri $Endpoint -Method Delete -Headers $Headers -UseBasicParsing -TimeoutSec 5 | Out-Null } catch {}
    Write-HyperAIStatus 'Ready' "tools/list reachable with $ToolCount advertised tools." $ToolCount
    exit 0
  } catch {
    $LastError = $_.Exception.Message
    if ($Headers) { try { Invoke-WebRequest -Uri $Endpoint -Method Delete -Headers $Headers -UseBasicParsing -TimeoutSec 5 | Out-Null } catch {} }
    Write-HyperAIStatus 'Waiting' $LastError 0
    Start-Sleep -Seconds 2
  }
}
Write-HyperAIStatus 'Timeout' "Timed out waiting for $Endpoint. Last error: $LastError" 0
exit 1
