param([Parameter(Mandatory=$true)][string]$SourceRoot)
# One-time local import. No deployment, Git operations, or credential traversal.
$ErrorActionPreference = 'Stop'
$dest = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$src = (Resolve-Path -LiteralPath $SourceRoot).Path
if ($src -eq $dest) { throw 'Source and destination must differ' }
foreach ($name in @('m4','linux','telemetry')) {
    if (Test-Path -LiteralPath (Join-Path $dest $name)) { throw "Refuse overwrite: $name" }
}
$records = [Collections.Generic.List[object]]::new()
$skipped = [Collections.Generic.List[string]]::new()
function ImportFile($file) {
    $relative = [IO.Path]::GetRelativePath($src,$file.FullName).Replace('\','/')
    $vendor = $relative -match '^m4/m4_fw/(Drivers|Middlewares)/'
    $allowed = $file.Extension -in @('.c','.h','.cpp','.s','.ld','.mk','.cmake','.in','.ioc','.dts','.dtsi','.pro','.sh','.ps1','.py','.conf','.service','.timer') -or $file.Name -eq 'Makefile' -or $file.Name -eq 'CMakeLists.txt'
    if ($vendor -and ($file.Extension -in @('.md','.txt') -or $file.Name -match '^(LICENSE|COPYING|VERSION)$')) { $allowed=$true }
    if (!$file.Extension -and !$allowed) {
        $stream = $file.OpenRead()
        try { $a=$stream.ReadByte(); $b=$stream.ReadByte() } finally { $stream.Dispose() }
        $allowed = $a -eq 35 -and $b -eq 33 # shebang scripts only
    }
    if (!$allowed) { $skipped.Add($relative); return }
    if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse file: $relative" }
    $target = Join-Path $dest $relative
    New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target
    $hash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hash) { throw "Hash mismatch: $relative" }
    $records.Add([pscustomobject]@{path=$relative;bytes=$file.Length;sha256=$hash})
}
function Walk([string]$directory) {
    foreach ($item in Get-ChildItem -LiteralPath $directory -Force) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse path refused: $($item.Name)" }
        if ($item.PSIsContainer) {
            if ($item.Name -match '^(build.*|Debug|Release|__pycache__|\.git|\.settings|\.vscode)$') { continue }
            Walk $item.FullName
        } else { ImportFile $item }
    }
}
Walk (Join-Path $src 'm4')
Walk (Join-Path $src 'linux')
foreach ($part in @('receiver','sender','event_sender','outbox')) {
    # Deliberately never enumerate receiver's credential or runtime subdirectories.
    Get-ChildItem -LiteralPath (Join-Path $src "telemetry/$part") -File | ForEach-Object { ImportFile $_ }
}
$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $dest 'docs/import-manifest.json') -Encoding utf8
$skipped | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dest 'docs/import-exclusions.json') -Encoding utf8
Write-Output "Imported and SHA256 verified: $($records.Count) files. Credentials/runtime directories never traversed."
