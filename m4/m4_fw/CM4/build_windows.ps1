param(
    [Parameter(Mandatory=$true)][string]$ToolchainBin,
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Target = 'm4_fw_CM4_normal_sram_v2',
    [switch]$Bringup,
    [switch]$PresenceOnly,
    [switch]$ServoPrepare,
    [switch]$ShutdownAck,
    [switch]$ServoManual,
    [switch]$ServoRadar,
    [switch]$ServoOneShot
)
$ErrorActionPreference = 'Stop'
$gcc = Join-Path $ToolchainBin 'arm-none-eabi-gcc.exe'
if (!(Test-Path -LiteralPath $gcc)) { throw "Compiler missing: $gcc" }
Push-Location $PSScriptRoot
try {
    # Read the source/include lists from Makefile; build each source directly
    # to avoid make/sh process-spawn failures in Windows Unicode paths.
    $makeText = Get-Content -Raw -LiteralPath 'Makefile'
    function Read-MakeList([string]$Name) {
        $match = [regex]::Match($makeText, '(?ms)^' + $Name + '\s*=\s*(.*?)(?=\r?\n\r?\n)')
        if (!$match.Success) { throw "Missing Makefile list: $Name" }
        return (($match.Groups[1].Value -replace '\\\r?\n', ' ').Trim() -split '\s+')
    }
    $sources = @(Read-MakeList 'C_SOURCES') + @(Read-MakeList 'ASM_SOURCES')
    $includes = @(Read-MakeList 'C_INCLUDES')
    $defs = @(Read-MakeList 'DEFS')
    if ($ServoOneShot) {
        if (!$ServoManual -or !$ServoRadar) { throw 'ServoOneShot requires ServoManual and ServoRadar' }
        $defs += '-DSERVO_ONESHOT_TEST'
    }
    if ($ServoRadar) {
        if (!$ServoManual) { throw 'ServoRadar requires ServoManual' }
        $defs += '-DPRESENCE_OUT_ONLY', '-DSERVO_RADAR_TEST'
    }
    if ($ServoManual) {
        if ($PresenceOnly -or $ServoPrepare -or $ShutdownAck) { throw 'ServoManual is a standalone diagnostic mode' }
        $ServoPrepare = $true
        $defs += '-DSERVO_MANUAL_TEST', '-DPRESENCE_SHUTDOWN_ACK'
    }
    if ($ServoPrepare -and $PresenceOnly) { throw 'Select only one diagnostic mode' }
    if ($Bringup -or $PresenceOnly -or $ServoPrepare) { $defs += '-DRPMSG_BRINGUP_ONLY', '-D__LOG_TRACE_IO_' }
    if ($ServoPrepare) { $defs += '-DSERVO_PREPARE_ONLY' }
    if ($PresenceOnly) { $defs += '-DPRESENCE_OUT_ONLY' }
    if ($ShutdownAck) {
        if (!$PresenceOnly) { throw 'ShutdownAck requires PresenceOnly (no actuators)' }
        $defs += '-DPRESENCE_SHUTDOWN_ACK'
    }
    $out = "build/$Target"
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    $cpu = @('-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard')
    $objects = @()
    foreach ($source in $sources) {
        $obj = "$out/$([IO.Path]::GetFileNameWithoutExtension($source)).o"
        if ($objects -contains $obj) { throw "Object-name collision: $source" }
        $objects += $obj
        & $gcc @cpu @defs @includes -O0 -g3 -Wall -fdata-sections -ffunction-sections -MMD -MP -c $source -o $obj
        if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
    }
    $elf = "$out/$Target.elf"
    & $gcc @cpu @objects '-specs=nano.specs' '-TSTM32MP157DAAX_RAM.ld' "-Wl,-Map=$out/$Target.map,--cref" '-Wl,--gc-sections' -o $elf
    if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
    & (Join-Path $PSScriptRoot 'verify_vectors.ps1') -Elf $elf -ToolchainBin $ToolchainBin -CheckTim5:$ServoManual
    & (Join-Path $ToolchainBin 'arm-none-eabi-size.exe') $elf
    if ($LASTEXITCODE -ne 0) { throw 'Size check failed' }
    Get-Item -LiteralPath $elf | Select-Object FullName,Length
    Get-FileHash -Algorithm SHA256 -LiteralPath $elf | Format-List
} finally {
    Pop-Location
}
