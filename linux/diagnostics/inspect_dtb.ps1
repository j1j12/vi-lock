param([string]$Path)
$ErrorActionPreference='Stop'
$b=[IO.File]::ReadAllBytes($Path)
function U32([int]$i) { [uint32](([uint64]$b[$i]*16777216)+([uint64]$b[$i+1]*65536)+([uint64]$b[$i+2]*256)+$b[$i+3]) }
if ((U32 0) -ne 3490578157L) { throw 'Invalid FDT magic' }
$p=[int](U32 8); $s=[int](U32 12)
$stack=[Collections.Generic.List[string]]::new(); $nodes=@{}
while ($p -lt $b.Length) {
 $t=U32 $p; $p+=4
 switch ($t) {
 1 { $e=$p; while($b[$e] -ne 0){$e++}; $stack.Add([Text.Encoding]::ASCII.GetString($b,$p,$e-$p)); $nodes[$stack -join '/']=@{}; $p=($e+4)-band -4 }
 2 { $stack.RemoveAt($stack.Count-1) }
 3 {
  $len=[int](U32 $p); $off=[int](U32 ($p+4)); $p+=8
  $e=$s+$off; while($b[$e] -ne 0){$e++}
  $key=[Text.Encoding]::ASCII.GetString($b,$s+$off,$e-$s-$off)
  $v=if($len -eq 0){''}elseif($key -match 'pinmux|phandle|pinctrl-[0-9]+|gpio|^reg$|interrupt'){
   (@(for($i=$p;$i+3 -lt $p+$len;$i+=4){'{0:x8}' -f (U32 $i)}))-join ' '
  }else{[Text.Encoding]::ASCII.GetString($b,$p,$len).Replace([char]0,'|')}
  $nodes[$stack -join '/'][$key]=$v; $p=($p+$len+3)-band -4
 }
 4 {}
 9 { $p=$b.Length }
 default { throw "Invalid token $t" }
 }
}
$nodes.GetEnumerator() | Sort-Object Key | ForEach-Object {
 $_.Key
 $_.Value | ConvertTo-Json -Compress
}
