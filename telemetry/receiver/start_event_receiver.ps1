param(
    [Parameter(Mandatory=$true)][string]$ServerIP,
    [Parameter(Mandatory=$true)][string]$BoardIP,
    [string]$Bundle = (Join-Path $PSScriptRoot 'tls-credentials-v1')
)
$ErrorActionPreference = 'Stop'
foreach ($value in @($ServerIP,$BoardIP)) {
    $parsed = [System.Net.IPAddress]::Parse($value)
    if ($parsed.AddressFamily -ne 'InterNetwork' -or $parsed.ToString() -ne $value) { throw 'Require explicit single IPv4 addresses' }
}
if ($ServerIP -eq $BoardIP) { throw 'Server and board must differ' }
$python = 'C:\Users\Acer\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Test-Path -LiteralPath $python)) { throw 'Python unavailable' }
if (-not (Get-NetIPAddress -InterfaceAlias WLAN -AddressFamily IPv4 | Where-Object IPAddress -eq $ServerIP)) { throw 'WLAN IPv4 changed' }
$manifest = Get-Content -LiteralPath (Join-Path $Bundle 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.server_ip -ne $ServerIP) { throw 'Certificate server IP differs; do not disable TLS verification' }
if (Get-NetTCPConnection -LocalPort 18767 -State Listen -ErrorAction SilentlyContinue) { throw '18767 already listening; do not start a second receiver' }
& $python -u (Join-Path $PSScriptRoot 'event_tls_receiver.py') --bind $ServerIP --allow $BoardIP --port 18767 --cert-dir (Join-Path $Bundle 'server')
if ($LASTEXITCODE -ne 0) { throw "Event test receiver exited: $LASTEXITCODE" }
