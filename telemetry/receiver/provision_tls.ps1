param(
    [Parameter(Mandatory=$true)][string]$ServerIP,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$python = 'C:\Users\Acer\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Test-Path -LiteralPath $python)) { throw 'Python unavailable' }
if (-not (Get-NetIPAddress -InterfaceAlias WLAN -AddressFamily IPv4 | Where-Object IPAddress -eq $ServerIP)) {
    throw 'ServerIP is not the current WLAN IPv4. Reconfirm addresses.'
}
$target = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $target) { throw 'Output already exists; will not overwrite credentials.' }
$parent = Split-Path -Parent $target
if (-not (Test-Path -LiteralPath $parent -PathType Container)) { throw 'Parent directory must already exist' }
New-Item -ItemType Directory -Path $target | Out-Null
# Protect the empty directory BEFORE Python creates any private material.
$sid = [Security.Principal.WindowsIdentity]::GetCurrent().User
$acl = New-Object Security.AccessControl.DirectorySecurity
$acl.SetAccessRuleProtection($true,$false)
$acl.SetOwner($sid)
$rule = New-Object Security.AccessControl.FileSystemAccessRule($sid,'FullControl','ContainerInherit,ObjectInherit','None','Allow')
$acl.AddAccessRule($rule)
Set-Acl -LiteralPath $target -AclObject $acl
$actual = Get-Acl -LiteralPath $target
if (-not $actual.AreAccessRulesProtected) { throw 'Directory inheritance was not disabled; no keys created' }
$rules = $actual.GetAccessRules($true,$true,[Security.Principal.SecurityIdentifier])
if ($rules.Count -ne 1 -or $rules[0].IdentityReference.Value -ne $sid.Value -or
    $rules[0].AccessControlType -ne 'Allow' -or $rules[0].FileSystemRights -ne 'FullControl') {
    throw 'Private directory ACL verification failed; no keys created'
}
& $python (Join-Path $PSScriptRoot 'issue_credentials.py') --directory $target --server-ip $ServerIP
if ($LASTEXITCODE -ne 0) { throw 'Issuance failed; do not use incomplete bundle or automatically delete it' }
# Python's Windows mode=0700 can create a separate owner/system/admin DACL.
# Normalize every child to the explicit account ACL, then verify each object.
Get-ChildItem -LiteralPath $target -Recurse | ForEach-Object {
    if ($_.PSIsContainer) {
        $grant = '*'+$sid.Value+':(OI)(CI)F'
    } else {
        $grant = '*'+$sid.Value+':F'
    }
    # icacls changes the DACL only; Set-Acl may request SeSecurityPrivilege here.
    & icacls.exe $_.FullName /grant:r $grant /inheritance:r /remove:g '*S-1-5-18' '*S-1-5-32-544' '*S-1-3-4' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Child ACL update failed' }
    $verified = (Get-Acl -LiteralPath $_.FullName).GetAccessRules($true,$true,[Security.Principal.SecurityIdentifier])
    if ($verified.Count -ne 1 -or $verified[0].IdentityReference.Value -ne $sid.Value) { throw 'Child ACL verification failed' }
}
Write-Output "PASS: protected credential bundle at $target. Copy ONLY board subdirectory to the board."
