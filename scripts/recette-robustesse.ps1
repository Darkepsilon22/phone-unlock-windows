#Requires -RunAsAdministrator
param([int]$LoadSeconds = 60)
$ErrorActionPreference = 'Stop'

$serviceName = 'PhoneUnlockService'
$executable = Join-Path $env:ProgramFiles "PhoneUnlock\$serviceName.exe"
$base = 'https://127.0.0.1:47863'
$work = Join-Path $env:TEMP 'phoneunlock-recette'
$results = New-Object 'System.Collections.Generic.List[object]'

function Add-Result([string]$test, [string]$expected, [string]$actual) {
    $results.Add([pscustomobject]@{
        Essai = $test; Attendu = $expected; Obtenu = $actual; OK = ($expected -split '\|') -contains $actual
    })
}

function Invoke-Https([string]$test, [string]$expected, [string[]]$arguments) {
    $code = & curl.exe -s -k --tlsv1.3 -o NUL -w '%{http_code}' --max-time 10 @arguments
    Add-Result $test $expected "$code"
}

function Get-PipeOutcome([string]$pipeName, [byte[]]$frame) {
    $pipe = New-Object System.IO.Pipes.NamedPipeClientStream('.', $pipeName, [System.IO.Pipes.PipeDirection]::InOut,
        [System.IO.Pipes.PipeOptions]::Asynchronous)
    try {
        $pipe.Connect(3000)
        $pipe.Write($frame, 0, $frame.Length)
        $buffer = New-Object byte[] 64
        $read = $pipe.ReadAsync($buffer, 0, $buffer.Length)
        if (-not $read.Wait(3000)) { return 'aucune réponse' }
        if ($read.Result -eq 0) { return 'fermé' }
        return "réponse de $($read.Result) octets"
    }
    catch [System.UnauthorizedAccessException] {
        return 'accès refusé'
    }
    catch [System.TimeoutException] {
        return 'canal absent'
    }
    catch {
        return 'fermé'
    }
    finally {
        $pipe.Dispose()
    }
}

function Send-PipeFrame([string]$test, [string]$expected, [string]$pipeName, [byte[]]$frame) {
    Add-Result $test $expected (Get-PipeOutcome $pipeName $frame)
}

function Get-ServiceProcess {
    (Get-CimInstance Win32_Service -Filter "Name='$serviceName'").ProcessId
}

New-Item -ItemType Directory -Force $work | Out-Null
$processBefore = Get-ServiceProcess
"Service : processus $processBefore. Essais en cours…"

$large = Join-Path $work 'large.bin'
[IO.File]::WriteAllBytes($large, (New-Object byte[] (20 * 1024)))
$garbage = Join-Path $work 'garbage.bin'
[IO.File]::WriteAllBytes($garbage, [byte[]](0xFF, 0x00, 0xBF, 0x9F, 0x18, 0x1C, 0xFF))

Invoke-Https 'Corps de 20 Ko (T22)' '413' @('-X', 'POST', '-H', 'Content-Type: application/cbor', '--data-binary', "@$large", "$base/v1/pair")
Invoke-Https 'CBOR invalide (T22)' '400' @('-X', 'POST', '-H', 'Content-Type: application/cbor', '--data-binary', "@$garbage", "$base/v1/pair")
Invoke-Https 'Chemin inconnu (T22)' '404' @("$base/v1/inconnu")
Invoke-Https 'Adresse de 8 000 caractères (T22)' '400|404|414' @("$base/v1/$('a' * 8000)")
Invoke-Https 'Méthode inattendue (T22)' '400|403|404|405|411|501' @('-X', 'PUT', "$base/v1/state")
Invoke-Https 'Preuve sans certificat client (T19)' '403' @('-X', 'POST', '--data-binary', "@$garbage", "$base/v1/requests/00000000000000000000000000000000/proof")
$tls12 = & curl.exe -s -k --tlsv1.2 --tls-max 1.2 -o NUL -w '%{http_code}' --max-time 10 "$base/v1/"
Add-Result 'TLS 1.2 seulement (D5)' '000|403' "$tls12"

Send-PipeFrame 'Canal admin : taille annoncée de 4 Go (T22)' 'fermé|aucune réponse' 'PhoneUnlock.Admin' ([byte[]](0xFF, 0xFF, 0xFF, 0xFF, 0x0C, 0x00, 0x00, 0x00))
Send-PipeFrame 'Canal admin : type inconnu (T22)' 'fermé|aucune réponse' 'PhoneUnlock.Admin' ([byte[]](0x00, 0x00, 0x00, 0x00, 0xE7, 0x03, 0x00, 0x00))
Send-PipeFrame 'Canal admin : charge tronquée (T22)' 'fermé|aucune réponse' 'PhoneUnlock.Admin' ([byte[]](0x40, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x02))
Send-PipeFrame 'Canal de la tuile depuis un administrateur (T20)' 'accès refusé' 'PhoneUnlock.LogonUi' ([byte[]](0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00))

$config = Join-Path $work 'load.txt'
Set-Content $config (1..50 | ForEach-Object { "url = `"$base/v1/state`"`noutput = `"NUL`"" }) -Encoding Ascii
"Charge pendant $LoadSeconds s : verrouille maintenant (Windows + L) et rouvre la session avec ton PIN."
$codes = @{}
$deadline = (Get-Date).AddSeconds($LoadSeconds)
while ((Get-Date) -lt $deadline) {
    foreach ($code in (& curl.exe -s -k --tlsv1.3 -Z --parallel-max 50 --max-time 10 -w '%{http_code}\n' -K $config)) {
        $codes[$code] = 1 + $(if ($codes.ContainsKey($code)) { $codes[$code] } else { 0 })
    }
}
$summary = ($codes.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)×$($_.Value)" }) -join ', '
$unexpected = $codes.Keys | Where-Object { $_ -notin '000', '403', '429', '503' }
Add-Result "Charge de $LoadSeconds s, 50 requêtes en parallèle (T25)" 'refus ou saturation seulement' $(if ($unexpected) { $summary } else { 'refus ou saturation seulement' })
"Réponses pendant la charge : $summary"

$processAfter = Get-ServiceProcess
$continuity = if ($processBefore -and $processBefore -eq $processAfter) { 'processus inchangé' } else { "$processBefore puis $processAfter" }
Add-Result 'Service jamais redémarré (T22, T25)' 'processus inchangé' $continuity
$recovered = $false
for ($attempt = 0; $attempt -lt 10 -and -not $recovered; ++$attempt) {
    Start-Sleep -Seconds 3
    $recovered = [bool]((& $executable status) -match 'répond en TLS 1\.3')
}
Add-Result 'status dans les 30 s après la charge' 'répond en TLS 1.3' $(if ($recovered) { 'répond en TLS 1.3' } else { 'ne répond pas' })

Remove-Item $work -Recurse -Force
$results | Format-Table -AutoSize -Wrap
"$(@($results | Where-Object OK).Count) essais conformes sur $($results.Count)."
