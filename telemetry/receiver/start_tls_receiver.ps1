param(
    [Parameter(Mandatory=$true)][string]$ServerIP,
    [Parameter(Mandatory=$true)][string]$BoardIP,
    [Parameter(Mandatory=$true)][string]$Bundle
)
$ErrorActionPreference = 'Stop'
$python = 'C:\Users\Acer\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Get-NetIPAddress -InterfaceAlias WLAN -AddressFamily IPv4 | Where-Object IPAddress -eq $ServerIP)) { throw 'WLAN IPv4 changed' }
$manifest = Get-Content -LiteralPath (Join-Path $Bundle 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.server_ip -ne $ServerIP) { throw 'Certificate server IP differs; reissue, never disable verification' }
if (Get-NetTCPConnection -LocalPort 18766 -State Listen -ErrorAction SilentlyContinue) { throw '18766 already listening' }
& $python -u (Join-Path $PSScriptRoot 'tls_receiver.py') --bind $ServerIP --allow $BoardIP --cert-dir (Join-Path $Bundle 'server')
if ($LASTEXITCODE -ne 0) { throw "TLS receiver exited: $LASTEXITCODE" }
