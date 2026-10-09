# Manual Administrator PowerShell action; does not change existing probe rules.
param(
    [Parameter(Mandatory=$true)][string]$ServerIP,
    [Parameter(Mandatory=$true)][string]$BoardIP
)
$ErrorActionPreference = 'Stop'
foreach ($value in @($ServerIP,$BoardIP)) {
    $parsed = [System.Net.IPAddress]::Parse($value)
    if ($parsed.AddressFamily -ne 'InterNetwork' -or $parsed.ToString() -ne $value) { throw 'Require explicit single IPv4 addresses' }
}
if ($ServerIP -eq $BoardIP) { throw 'Server and board must differ' }
if (-not (Get-NetIPAddress -InterfaceAlias WLAN -AddressFamily IPv4 | Where-Object IPAddress -eq $ServerIP)) { throw 'WLAN address differs; reconfirm certificate IP' }
$name = 'AccessControl-LAN-mTLS-EventTest-18767'
if (Get-NetFirewallRule -Name $name -ErrorAction SilentlyContinue) { throw 'Rule already exists; inspect rather than widening it' }
$python = 'C:\Users\Acer\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Test-Path -LiteralPath $python)) { throw 'Python unavailable' }
New-NetFirewallRule -Name $name -DisplayName 'Access Control synthetic event mTLS test' -Direction Inbound -Action Allow -Protocol TCP -LocalAddress $ServerIP -LocalPort 18767 -RemoteAddress $BoardIP -InterfaceAlias WLAN -Profile Any -Program $python
