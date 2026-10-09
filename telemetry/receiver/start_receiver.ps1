param([switch]$Background)
$ErrorActionPreference = 'Stop'
$python = 'C:\Users\Acer\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Test-Path -LiteralPath $python)) { throw 'Python runtime unavailable; report before changing the script.' }
$address = Get-NetIPAddress -InterfaceAlias WLAN -AddressFamily IPv4 | Where-Object IPAddress -eq '192.168.101.100'
if (-not $address) { throw 'WLAN address changed; do not bind an unrelated interface.' }
if (Get-NetTCPConnection -LocalPort 18765 -State Listen -ErrorAction SilentlyContinue) { throw 'Port 18765 already in use. Do not start duplicate receiver.' }
$script = Join-Path $PSScriptRoot 'receiver.py'
if ($Background) {
    $process = Start-Process -FilePath $python -ArgumentList @('-u',('"'+$script+'"'),'--bind','192.168.101.100','--allow','192.168.101.102') -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -PassThru
    Write-Output "Receiver PID=$($process.Id). No auto-start installed."
} else {
    & $python -u $script --bind 192.168.101.100 --allow 192.168.101.102
}
