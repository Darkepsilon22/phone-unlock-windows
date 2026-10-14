#Requires -RunAsAdministrator
param(
    [datetime]$Since = (Get-Date).AddHours(-1),
    [int]$IdleSeconds = 0
)
$ErrorActionPreference = 'Stop'

$serviceName = 'PhoneUnlockService'
$log = Join-Path $env:ProgramData 'PhoneUnlock\logs\service.log'
$timestampLength = 23
$targetMilliseconds = 3000
$targetCpuPercent = 1
$targetMegabytes = 100

function Get-Percentile([double[]]$sorted, [double]$fraction) {
    $sorted[[math]::Max(0, [math]::Ceiling($fraction * $sorted.Count) - 1)]
}

$durations = New-Object 'System.Collections.Generic.List[double]'
$requests = 0
$succeeded = 0
$authorizedAt = $null
foreach ($line in Get-Content $log -Encoding UTF8) {
    if ($line.Length -lt $timestampLength -or $line -notmatch ' history unlock ') { continue }
    $time = [datetime]::ParseExact($line.Substring(0, $timestampLength), 'yyyy-MM-dd HH:mm:ss.fff', $null)
    if ($time -lt $Since) { continue }
    if ($line -match ' history unlock requested ') {
        ++$requests
    }
    elseif ($line -match ' history unlock authorized ') {
        $authorizedAt = $time
    }
    elseif ($line -match ' history unlock result .* status=0x([0-9A-Fa-f]{8})') {
        if ($matches[1] -eq '00000000') { ++$succeeded }
        if ($authorizedAt) {
            $durations.Add(($time - $authorizedAt).TotalMilliseconds)
            $authorizedAt = $null
        }
    }
}

"Depuis $Since : $requests demandes, $succeeded sessions ouvertes."
if ($durations.Count -gt 0) {
    $sorted = [double[]]($durations | Sort-Object)
    $p95 = Get-Percentile $sorted 0.95
    "Preuve acceptée → résultat Windows ($($sorted.Count) mesures) : min $([math]::Round($sorted[0])) ms, médiane $([math]::Round((Get-Percentile $sorted 0.5))) ms, 95e percentile $([math]::Round($p95)) ms, max $([math]::Round($sorted[-1])) ms."
    "Cible du cahier : 95e percentile ≤ $targetMilliseconds ms → $(if ($p95 -le $targetMilliseconds) { 'atteinte' } else { 'NON atteinte' })."
}
if ($requests -gt 0) {
    "Taux de succès : $succeeded / $requests ($([math]::Round(100 * $succeeded / $requests, 1)) %), cible 98 %."
}

if ($IdleSeconds -gt 0) {
    $processId = (Get-CimInstance Win32_Service -Filter "Name='$serviceName'").ProcessId
    $process = Get-Process -Id $processId
    $cpuBefore = $process.TotalProcessorTime
    $started = Get-Date
    $peakMegabytes = 0
    "Mesure au repos pendant $IdleSeconds s : ne touche pas à la VM."
    while (((Get-Date) - $started).TotalSeconds -lt $IdleSeconds) {
        Start-Sleep -Seconds 10
        $process.Refresh()
        $peakMegabytes = [math]::Max($peakMegabytes, $process.PrivateMemorySize64 / 1MB)
    }
    $elapsed = ((Get-Date) - $started).TotalSeconds
    $cpuPercent = 100 * ($process.TotalProcessorTime - $cpuBefore).TotalSeconds / ($elapsed * [Environment]::ProcessorCount)
    "Processeur moyen au repos : $([math]::Round($cpuPercent, 3)) % (cible < $targetCpuPercent %) → $(if ($cpuPercent -lt $targetCpuPercent) { 'atteinte' } else { 'NON atteinte' })."
    "Mémoire privée maximale : $([math]::Round($peakMegabytes, 1)) Mo (cible < $targetMegabytes Mo) → $(if ($peakMegabytes -lt $targetMegabytes) { 'atteinte' } else { 'NON atteinte' })."
}
