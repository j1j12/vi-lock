param([Parameter(Mandatory=$true)][string]$Elf,
      [Parameter(Mandatory=$true)][string]$ToolchainBin,
      [switch]$CheckTim5)
$ErrorActionPreference = 'Stop'
$symbols = @{}
$lines = & (Join-Path $ToolchainBin 'arm-none-eabi-nm.exe') $Elf
if ($LASTEXITCODE -ne 0) { throw 'nm failed' }
foreach ($line in $lines) {
    if ($line -match '^([0-9a-fA-F]+)\s+([TtWw])\s+(\w+)$') {
        $symbols[$Matches[3]] = @{Address=[Convert]::ToUInt32($Matches[1],16); Kind=$Matches[2]}
    }
}
$binary = "$Elf.vectors.bin"
& (Join-Path $ToolchainBin 'arm-none-eabi-objcopy.exe') -O binary --only-section=.isr_vector $Elf $binary
if ($LASTEXITCODE -ne 0) { throw 'Vector extraction failed' }
$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $binary).Path)
$checks = @{SVC_Handler=11; PendSV_Handler=14; SysTick_Handler=15; HardFault_Handler=3}
$header = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '../Drivers/CMSIS/Device/ST/STM32MP1xx/Include/stm32mp157dxx_cm4.h')
$irqNames = @('IPCC_RX1', 'IPCC_TX1', 'TIM6')
if ($CheckTim5) { $irqNames += 'TIM5' }
foreach ($irq in $irqNames) {
    $m = [regex]::Match($header, $irq + '_IRQn\s*=\s*(\d+)')
    if (!$m.Success) { throw "IRQ number missing: $irq" }
    $checks[$irq + '_IRQHandler'] = 16 + [int]$m.Groups[1].Value
}
foreach ($name in $checks.Keys) {
    $sym = $symbols[$name]
    if (!$sym -or $sym.Kind -cne 'T' -or $sym.Address -eq $symbols['Default_Handler'].Address) {
        throw "Invalid interrupt handler: $name resolves to weak/default/missing symbol"
    }
    $entry = [BitConverter]::ToUInt32($bytes, 4 * $checks[$name])
    if ($entry -ne ($sym.Address -bor 1)) { throw "Vector does not target $name" }
}
Write-Output "Vector audit PASS: SysTick/SVC/PendSV/HardFault/TIM6/IPCC_RX1/IPCC_TX1 ($Elf)"
if ($CheckTim5) { Write-Output 'Additional TIM5 cutoff vector audit PASS' }
