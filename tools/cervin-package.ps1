# Cervin : prépare les fichiers d'une release Windows à partir d'une build compilée.
#
#   tools\cervin-package.ps1                       build\release, version de dist\frizti_version_label.txt
#   tools\cervin-package.ps1 -Build build\pgo-use  une autre build (par ex. la build clang PGO)
#   tools\cervin-package.ps1 -Suffix PGO           ajoute « -PGO » au nom du zip
#
# Produit dans E:\Dev\cervin-releases\<version>\ :
#   Cervin-<version>-Windows-x64.zip           à publier : Cervin.exe, Qt, runtime C++, licences
#   Cervin-<version>-Windows-x64-symbols.zip   les .pdb, pour lire les journaux de crash
#   SHA256SUMS.txt                             les empreintes des deux zips
# Ne met jamais de clés, de firmware, de jeux ni de fichiers NVIDIA dans le zip.

param(
    [string]$Build = 'build\release',
    [string]$Version = '',
    [string]$Suffix = '',
    [string]$Out = 'E:\Dev\cervin-releases'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$bin = Join-Path (Join-Path $repo $Build) 'bin'
if (-not $Version) {
    $Version = (Get-Content (Join-Path $repo 'dist\frizti_version_label.txt') -Raw).Trim()
}
$name = "Cervin-$Version-Windows-x64" + $(if ($Suffix) { "-$Suffix" } else { '' })
$exe = Join-Path $bin 'Cervin.exe'
if (-not (Test-Path $exe)) { throw "Cervin.exe introuvable dans $bin : compile d'abord." }

$commit = (& git -C $repo rev-parse --short=10 HEAD).Trim()
$dirty = (& git -C $repo status --porcelain --untracked-files=no)
if ($dirty) { Write-Warning "Des fichiers suivis sont modifiés sans être commités : la release ne correspondra pas exactement au commit $commit." }

$dest = Join-Path $Out $Version
$stage = Join-Path $dest $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

# Le programme et ce que windeployqt a mis à côté (Qt, plugins, compilateur de shaders DirectX).
Copy-Item $exe $stage
foreach ($pattern in @('Qt6*.dll', 'dxcompiler.dll', 'dxil.dll', 'qt.conf', 'nx2_switches.txt')) {
    Get-ChildItem $bin -Filter $pattern -File | Copy-Item -Destination $stage
}
Copy-Item (Join-Path $bin 'plugins') (Join-Path $stage 'plugins') -Recurse

# Le runtime C++ de Visual Studio, à côté de l'exe : un PC avec un runtime plus ancien ne lancerait
# pas Cervin sinon (fichiers redistribuables de Microsoft).
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -prerelease -products * -property installationPath
$crt = Get-ChildItem (Join-Path $vs 'VC\Redist\MSVC') -Directory |
    Where-Object { $_.Name -match '^\d' } | Sort-Object Name -Descending | Select-Object -First 1
$crtDir = Get-ChildItem (Join-Path $crt.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
Get-ChildItem $crtDir.FullName -Filter '*.dll' | Copy-Item -Destination $stage

# Licences (GPL-3.0 et celles des composants) et où trouver le code source exact.
Copy-Item (Join-Path $repo 'LICENSE.txt') $stage
Copy-Item (Join-Path $repo 'LICENSES') (Join-Path $stage 'LICENSES') -Recurse
$readme = @"
Cervin $Version - émulateur Switch 2 expérimental pour Windows
================================================================

Lancer : double-clic sur Cervin.exe. Aucun fichier à installer.

À fournir soi-même (jamais inclus) : vos jeux, vos clés (prod.keys) et votre firmware,
issus de votre propre console.

Données (paramètres, sauvegardes, cache) : %APPDATA%\fritchii, partagé avec Fritchii.
Pour une version portable, créez un dossier « user » à côté de Cervin.exe.

DLSS : utilise le DLSS fourni par l'application NVIDIA (cartes GeForce RTX).

Un problème ? Aide > Exporter le journal, puis ouvrez une issue avec le zip :
https://github.com/basthaly/Cervin/issues

Cervin est un logiciel libre sous licence GPL-3.0 (voir LICENSE.txt et LICENSES\).
Code source exact de cette version : https://github.com/basthaly/Cervin/tree/$commit
Basé sur Fritchii (Frizti), Trudy (joshpack), Eglantine, Lichii, Eden, yuzu et Sudachi.
Shader precompile system by Frizti (Fritchii).
"@
Set-Content -Path (Join-Path $stage 'LISEZMOI.txt') -Value $readme -Encoding UTF8

# Les zips.
$zip = Join-Path $dest "$name.zip"
$symbols = Join-Path $dest "$name-symbols.zip"
Remove-Item $zip, $symbols -ErrorAction SilentlyContinue
# Pas Compress-Archive : sous Windows PowerShell 5.1 il écrit des chemins avec « \ » et une entrée
# vide par dossier, que la mise à jour intégrée (QuaZip) ne sait pas décompresser (v1.0.4).
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($zip, 'Create')
try {
    $root = Split-Path -Parent $stage
    foreach ($file in Get-ChildItem $stage -Recurse -File) {
        $entry = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entry, 'Optimal')
    }
} finally { $archive.Dispose() }
$pdbs = Get-ChildItem $bin -Filter 'Cervin.pdb' -File
if ($pdbs) { Compress-Archive -Path $pdbs.FullName -DestinationPath $symbols -CompressionLevel Optimal }

$sums = foreach ($file in @($zip, $symbols) | Where-Object { Test-Path $_ }) {
    "{0}  {1}" -f (Get-FileHash $file -Algorithm SHA256).Hash.ToLower(), (Split-Path $file -Leaf)
}
Set-Content -Path (Join-Path $dest 'SHA256SUMS.txt') -Value $sums -Encoding ASCII
Remove-Item $stage -Recurse -Force

Write-Host "Release $Version (commit $commit) prête dans $dest :" -ForegroundColor Green
Get-ChildItem $dest -File | ForEach-Object { "  {0,-50} {1,8:N1} Mo" -f $_.Name, ($_.Length / 1MB) }
