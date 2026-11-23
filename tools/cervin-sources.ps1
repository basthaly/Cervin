# Cervin : ce qui est arrivé dans les sources du fork depuis la dernière vérification.
#
#   tools\cervin-sources.cmd              nouveautés depuis la dernière fois marquée « vue »
#   tools\cervin-sources.cmd -Tout        affiche aussi les commits sans mot-clé
#   tools\cervin-sources.cmd -MarquerVu   enregistre l'état actuel comme « vu »
#
# Sources suivies : Fritchii (nouvelles releases fritchii-v*), Trudy (main) et Eglantine (nx2).
# Les commits sont classés par mots-clés : [MKW], [RTX/Perf], [OoT]. Ceux que Cervin a déjà repris
# (cherry-pick -x) sont marqués [déjà repris]. L'état « vu » est gardé dans .git/cervin-sources.json
# (jamais commité).

param(
    [switch]$Tout,
    [switch]$MarquerVu
)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$repo = Split-Path -Parent $PSScriptRoot
$stateFile = Join-Path $repo '.git\cervin-sources.json'

function Invoke-Git {
    $output = & git -C $repo @args 2>$null
    if ($LASTEXITCODE -ne 0) { return @() }
    return @($output)
}

# Ce que la première version de Cervin a déjà examiné (2026-11-23).
$state = [ordered]@{
    fritchii_tag = 'fritchii-v6.5'
    trudy        = '76ffeff7a'
    eglantine    = '835019fec'
}
if (Test-Path $stateFile) {
    $saved = Get-Content $stateFile -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($key in @('fritchii_tag', 'trudy', 'eglantine')) {
        if ($saved.$key) { $state[$key] = $saved.$key }
    }
}

Write-Host 'Récupération des sources (Fritchii, Trudy, Eglantine)...' -ForegroundColor DarkGray
& git -C $repo fetch --quiet fritchii --tags 2>$null
& git -C $repo fetch --quiet trudy main 2>$null
& git -C $repo fetch --quiet eglantine nx2 2>$null

# Commits que Cervin a déjà repris : « (cherry picked from commit <sha>) », ou cités par leur
# numéro dans les messages des commits de Cervin (« From Eglantine 894f2c119 », adaptés à la main).
$picked = @()
foreach ($line in (Invoke-Git log "fritchii-v6.5..HEAD" --format='%B')) {
    foreach ($match in [regex]::Matches($line, '\b[0-9a-f]{9,40}\b')) { $picked += $match.Value }
}
function Test-Picked([string]$sha) {
    foreach ($ref in $picked) { if ($sha.StartsWith($ref)) { return $true } }
    return $false
}

$keywords = [ordered]@{
    'MKW'      = 'mkw|mario ?kart|kart world'
    'RTX/Perf' = 'nvidia|rtx|dlss|tensor|perf|fps|faster|speed ?up|stutter|latency|hitch|frame ?time'
    'OoT'      = '\boot\b|ocarina'
}

function Show-Commits([string]$title, [string]$range) {
    $lines = Invoke-Git log --no-merges --date=short "--format=%H|%ad|%an|%s" $range
    Write-Host ''
    if ($lines.Count -eq 0) {
        Write-Host "$title : rien de nouveau." -ForegroundColor DarkGray
        return 0
    }
    Write-Host "$title : $($lines.Count) commit(s)" -ForegroundColor Cyan
    $quiet = 0
    foreach ($line in $lines) {
        $sha, $date, $author, $subject = $line -split '\|', 4
        $tags = @($keywords.Keys | Where-Object { $subject -match $keywords[$_] })
        $done = Test-Picked $sha
        if ($tags.Count -eq 0 -and -not $Tout) { $quiet++; continue }
        $label = ($tags | ForEach-Object { "[$_]" }) -join ''
        if ($done) { $label += '[déjà repris]' }
        $color = if ($done) { 'DarkGray' } elseif ($tags -contains 'MKW') { 'Yellow' } elseif ($tags.Count -gt 0) { 'Green' } else { 'Gray' }
        Write-Host ("  {0} {1} {2,-12} {3} {4}" -f $sha.Substring(0, 9), $date, $author, $label, $subject) -ForegroundColor $color
    }
    if ($quiet -gt 0) { Write-Host "  ... et $quiet autre(s) sans mot-clé (-Tout pour les voir)" -ForegroundColor DarkGray }
    return $lines.Count
}

$total = 0

# Fritchii : la base de Cervin. Ses releases sont des tags fritchii-vX.Y.
$tags = Invoke-Git tag --list 'fritchii-v*' --sort=-v:refname
$newest = if ($tags.Count -gt 0) { $tags[0] } else { $state.fritchii_tag }
Write-Host ''
if ($newest -ne $state.fritchii_tag) {
    $newTags = @()
    foreach ($tag in $tags) { if ($tag -eq $state.fritchii_tag) { break }; $newTags += $tag }
    Write-Host "Fritchii : nouvelle(s) release(s) $($newTags -join ', ') (Cervin est basé sur $($state.fritchii_tag))" -ForegroundColor Magenta
    $total += Show-Commits "Fritchii $($state.fritchii_tag) -> $newest" "$($state.fritchii_tag)..$newest"
} else {
    Write-Host "Fritchii : pas de nouvelle release depuis $($state.fritchii_tag)." -ForegroundColor DarkGray
}

$total += Show-Commits 'Trudy (RTX/Ryzen)' "$($state.trudy)..trudy/main"
$total += Show-Commits 'Eglantine (MKW)' "$($state.eglantine)..eglantine/nx2"

Write-Host ''
if ($MarquerVu) {
    $now = [ordered]@{
        fritchii_tag = $newest
        trudy        = (Invoke-Git rev-parse --short=9 trudy/main)[0]
        eglantine    = (Invoke-Git rev-parse --short=9 eglantine/nx2)[0]
        checked      = (Get-Date -Format 'yyyy-MM-dd HH:mm')
    }
    $now | ConvertTo-Json | Set-Content -Path $stateFile -Encoding UTF8
    Write-Host 'État enregistré comme « vu » : la prochaine vérification partira d''ici.' -ForegroundColor DarkGray
} elseif ($total -gt 0) {
    Write-Host 'Pour intégrer quelque chose, donne le numéro (les 9 premiers caractères) à Claude.' -ForegroundColor DarkGray
    Write-Host 'Quand tout est examiné : tools\cervin-sources.cmd -MarquerVu' -ForegroundColor DarkGray
}
