param([string]$GameDir=$PSScriptRoot,[string]$OutputPath)
$ErrorActionPreference='Stop'
$game=(Resolve-Path -LiteralPath $GameDir).Path
if(!$OutputPath){$OutputPath=Join-Path $game 'OptiScaler-Diagnostics.txt'}
$writer=[IO.StreamWriter]::new($OutputPath,$false,[Text.UTF8Encoding]::new($false))
try {
 $writer.WriteLine('OptiScaler consolidated diagnostics')
 $writer.WriteLine('Collected: '+(Get-Date -Format o))
 $writer.WriteLine('Game directory: '+$game)
 foreach($name in @('OptiScaler.dll','dxgi.dll','winmm.dll','version.dll','winhttp.dll','wininet.dll','dbghelp.dll','dlssnr_amd_pass1.dll','dlssnr_amd_pass2.dll','dlssnr_amd_pass3.dll')){
  $p=Join-Path $game $name
  if(Test-Path -LiteralPath $p -PathType Leaf){$f=Get-Item -LiteralPath $p;$writer.WriteLine(('MODULE {0} size={1} version={2} SHA256={3}' -f $name,$f.Length,$f.VersionInfo.FileVersion,(Get-FileHash -LiteralPath $p).Hash))}
 }
 $writer.WriteLine('GPU below belongs to the collection computer; archived logs may come from another machine.')
 try {$writer.WriteLine((Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,Status | Format-List | Out-String))}catch{$writer.WriteLine('GPU inventory unavailable: '+$_.Exception.Message)}
 foreach($name in @('OptiScaler.ini','dlssnr_on_amd.ini','OptiScaler.log','amd_presr.log','amd_bridge.log','dlssnr_on_amd.log','dlss-enabler.log')){
  $p=Join-Path $game $name
  $writer.WriteLine("`n===== $name =====")
  if(!(Test-Path -LiteralPath $p -PathType Leaf)){$writer.WriteLine('[not present]');continue}
  $f=Get-Item -LiteralPath $p
  $writer.WriteLine(('Last write: {0:o}; bytes: {1}' -f $f.LastWriteTime,$f.Length))
  # Read a bounded snapshot while allowing the game to continue logging.
  $stream=[IO.File]::Open($p,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
  try {
   $remaining=$stream.Length
   if($remaining -gt 32MB){$stream.Seek($remaining-32MB,[IO.SeekOrigin]::Begin)|Out-Null;$remaining=32MB;$writer.WriteLine('[last 32 MiB; initial partial line possible]')}
   $buffer=New-Object byte[] 65536
   $decoder=[Text.Encoding]::UTF8.GetDecoder();$chars=New-Object char[] 65537
   while($remaining -gt 0){$n=$stream.Read($buffer,0,[int][Math]::Min($buffer.Length,$remaining));if(!$n){break};$remaining-=$n;$count=$decoder.GetChars($buffer,0,$n,$chars,0,$remaining -eq 0);$writer.Write($chars,0,$count)}
  }finally{$stream.Dispose()}
 }
}finally{$writer.Dispose()}
Write-Output $OutputPath
