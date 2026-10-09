# Run in Administrator PowerShell only after reconnecting to the board's LAN.
$ErrorActionPreference = 'Stop'
if (-not (Get-NetIPAddress -InterfaceAlias WLAN -AddressFamily IPv4 | Where-Object IPAddress -eq '192.168.101.101')) {
    throw 'Expected WLAN 192.168.101.101. Reconfirm both device addresses first.'
}
$ruleName = 'AccessControl-LAN-Probe-18765'
if (Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue) {
    throw 'Rule already exists; inspect it instead of silently widening it.'
}
$python = 'C:\Users\Acer\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Test-Path -LiteralPath $python)) { throw 'Python runtime unavailable' }
New-NetFirewallRule -Name $ruleName -DisplayName 'Access Control LAN synthetic probe' -Direction Inbound -Action Allow -Protocol TCP -LocalAddress 192.168.101.101 -LocalPort 18765 -RemoteAddress 192.168.101.100 -InterfaceAlias WLAN -Profile Any -Program $python
