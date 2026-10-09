param([string]$Source = 'stm32mp157d-atk.dtb',
      [string]$Output = 'linux/dts/stm32mp157d-atk-led1-off.dtb')
$ErrorActionPreference='Stop'
if ((Get-FileHash -LiteralPath $Source).Hash -ne '4AEF93A297581C0C95156F1A020B56F34987AEE9C7AAE6DEAAC80F05DFC514C8') {
    throw 'Unexpected source DTB; re-audit before patching'
}
if ([IO.Path]::GetFullPath($Source) -eq [IO.Path]::GetFullPath($Output)) { throw 'Do not overwrite source' }
# This is an artifact builder for the exact audited FDT v17, not a general editor.
$bytes=[IO.File]::ReadAllBytes((Resolve-Path $Source))
function Read32([int]$i) { [uint32](([uint64]$bytes[$i]*16777216)+([uint64]$bytes[$i+1]*65536)+([uint64]$bytes[$i+2]*256)+$bytes[$i+3]) }
$total=Read32 4; $start=Read32 8; $strings=Read32 12; $size=Read32 36
if($total -ne $bytes.Length -or $start+$size -ne $strings) {throw 'Unexpected layout'}
$p=[int]$start; $stack=[Collections.Generic.List[string]]::new(); $match=-1
while($p -lt $strings) {
 $token=Read32 $p; $p+=4
 switch($token) {
  1 { $e=$p; while($bytes[$e]){$e++}; $stack.Add([Text.Encoding]::ASCII.GetString($bytes,$p,$e-$p)); $p=($e+4)-band -4 }
  2 { $stack.RemoveAt($stack.Count-1) }
  3 {
   $len=[int](Read32 $p); $off=Read32 ($p+4); $p+=8
   $e=[int]($strings+$off); $end=$e; while($bytes[$end]){$end++}
   $key=[Text.Encoding]::ASCII.GetString($bytes,$e,$end-$e)
   if(($stack -join '/') -eq '/leds/led1' -and $key -eq 'status') {
    if($match -ne -1 -or $len -ne 5 -or [Text.Encoding]::ASCII.GetString($bytes,$p,5) -ne "okay`0") {throw 'Unexpected status'}
    $match=$p
   }
   $p=($p+$len+3)-band -4
  }
  4 {}
  9 {$p=[int]$strings}
  default {throw "Bad token $token"}
 }
}
if($match -lt 0){throw 'No led1 status'}
# Replace padded 8-byte okay property with padded 12-byte disabled property.
$result=[byte[]]::new($bytes.Length+4)
[Array]::Copy($bytes,0,$result,0,$match)
$replacement=[Text.Encoding]::ASCII.GetBytes("disabled`0")
[Array]::Copy($replacement,0,$result,$match,9)
[Array]::Copy($bytes,$match+8,$result,$match+12,$bytes.Length-$match-8)
function Write32([int]$i,[uint32]$value) {
 $result[$i]=[byte](($value -shr 24)-band 255); $result[$i+1]=[byte](($value -shr 16)-band 255)
 $result[$i+2]=[byte](($value -shr 8)-band 255); $result[$i+3]=[byte]($value-band 255)
}
Write32 ($match-8) 9
Write32 4 ($total+4); Write32 12 ($strings+4); Write32 36 ($size+4)
# Reversibility check proves all unrelated bytes are unchanged.
$restored=[byte[]]::new($bytes.Length)
[Array]::Copy($result,0,$restored,0,$match)
[Array]::Copy($bytes,$match,$restored,$match,8)
[Array]::Copy($result,$match+12,$restored,$match+8,$bytes.Length-$match-8)
foreach($offset in @(4,12,36,($match-8))){[Array]::Copy($bytes,$offset,$restored,$offset,4)}
for($i=0;$i -lt $bytes.Length;$i++){if($bytes[$i] -ne $restored[$i]){throw "Unexpected difference $i"}}
[IO.File]::WriteAllBytes([IO.Path]::GetFullPath($Output),$result)
Get-FileHash -LiteralPath $Output
"PASS: exact-source checked, reversible led1-only FDT patch; bytes=$($result.Length)"
