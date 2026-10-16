# Project ARA: Reborn - instalador do lider @@LEADER@@ (gerado por tools\make_release.py a partir deste modelo).
# Uso: clique duas vezes em INSTALAR.bat, ou:  powershell -ExecutionPolicy Bypass -File install.ps1 [-GamePath "...\Ara History Untold"]
# Os mods de lider (Brasil e URSS) dividem a mesma BASE: a DLL bin\swscale-7.dll e as pecas 3D/emblemas gravadas nos pacotes do
# jogo. A base e aplicada por patch a partir dos arquivos ORIGINAIS da Steam (conferidos pelo SHA-1); os trechos originais que
# mudam ficam guardados em Documentos\My Games\Ara History Untold\Mods\_RebornLideres para o desinstalador devolver.
param([string]$GamePath = "", [string]$ModsPath = "")
$ErrorActionPreference = 'Stop'
$Leader = '@@LEADER@@'
$Here   = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $Here 'base\rpatch.ps1')
$Patches = @(
    @('assets\PackagedData\Characters.OxAssetInfoPackage',   'Characters.OxAssetInfoPackage.rpatch'),
    @('assets\PackagedDataHD\Characters.OxAssetInfoPackage', 'Characters.OxAssetInfoPackage.rpatch'),
    @('assets\PackagedData\Characters.OxModelPackage',       'Characters.OxModelPackage.rpatch'),
    @('assets\PackagedDataHD\Characters.OxModelPackage',     'Characters.OxModelPackage.rpatch'),
    @('assets\PackagedData\Characters.OxTexturePackage',     'Characters.OxTexturePackage.rpatch'),
    @('assets\PackagedData\UI.OxTexturePackage',             'UI.OxTexturePackage.rpatch'),
    @('assets\UI\Art\Icons\RGBEmblems.xml',                  'RGBEmblems.xml.rpatch')
)
$NewFiles = @('assets\GameCore\Flags\flg_Brazil_Default.xml', 'assets\GameCore\Flags\flg_SovietUnion_Default.xml')

function Find-Game {
    if ($GamePath -and (Test-Path (Join-Path $GamePath 'bin\AraHistoryUntold.exe'))) { return (Resolve-Path $GamePath).Path }
    $libs = @()
    foreach ($k in @(@('HKCU:\Software\Valve\Steam', 'SteamPath'), @('HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath'))) {
        try { $v = (Get-ItemProperty -Path $k[0] -Name $k[1] -ErrorAction Stop).($k[1]); if ($v) { $libs += $v } } catch {}
    }
    foreach ($s in @($libs)) {
        $vdf = [IO.Path]::Combine($s, 'steamapps', 'libraryfolders.vdf')
        if (Test-Path -LiteralPath $vdf -ErrorAction SilentlyContinue) { foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += $m.Groups[1].Value.Replace('\\', '\') } }
    }
    foreach ($l in $libs) {
        $g = [IO.Path]::Combine($l, 'steamapps', 'common', 'Ara History Untold')
        try { if (Test-Path -LiteralPath ([IO.Path]::Combine($g, 'bin', 'AraHistoryUntold.exe'))) { return $g } } catch {}
    }
    throw 'Nao achei o ARA: History Untold. Rode de novo com -GamePath "C:\caminho\Ara History Untold".'
}
function Is-Proxy([string]$file) { return [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($file)).Contains('swscale-7_orig.') }

$game = Find-Game
Write-Host "Jogo: $game"
if (Get-Process AraHistoryUntold -ErrorAction SilentlyContinue) { throw 'Feche o jogo antes de instalar.' }
$mods  = if ($ModsPath) { $ModsPath } else { Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Ara History Untold\Mods' }
$state = Join-Path $mods '_RebornLideres'
New-Item -ItemType Directory -Force $state | Out-Null

# 1) base: pacotes do jogo (o patch confere o original antes e o resultado depois)
$drive = (Get-Item $game).PSDrive.Name
$free = (Get-PSDrive $drive).Free
if ($free -lt 4.5GB) { throw ("Espaco livre insuficiente no disco {0}: (precisa de ~4,5 GB temporarios; ha {1:N1} GB)." -f $drive, ($free / 1GB)) }
$i = 0
foreach ($p in $Patches) {
    $i++; $target = Join-Path $game $p[0]; $bak = Join-Path $state (($p[0] -replace '[\\:]', '_') + '.rbak')
    Write-Host ("[{0}/{1}] {2}..." -f $i, $Patches.Count, $p[0]) -NoNewline
    $r = [RPatch]::Apply($target, (Join-Path $Here ('base\patches\' + $p[1])), $bak)
    switch ($r) {
        0 { Write-Host ' aplicado' }
        1 { Write-Host ' ja estava aplicado' }
        2 { Write-Host ''; throw "$($p[0]) nao e o arquivo original da Steam (outra versao do jogo ou outro mod mexeu nele). Use Verificar integridade dos arquivos na Steam e instale de novo." }
        default { Write-Host ''; throw "Falha ao montar $($p[0]); nada foi trocado nesse arquivo." }
    }
}
foreach ($f in $NewFiles) {
    $dst = Join-Path $game $f; New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
    Copy-Item (Join-Path $Here ('base\files\' + $f)) $dst -Force
}

# 2) DLL dos lideres (bin\swscale-7.dll; a original vira swscale-7_orig.dll)
$bin = Join-Path $game 'bin'; $target = Join-Path $bin 'swscale-7.dll'; $orig = Join-Path $bin 'swscale-7_orig.dll'
if (-not (Test-Path $orig)) {
    if (Is-Proxy $target) { throw 'swscale-7.dll ja e a DLL do mod, mas a original sumiu. Use Verificar integridade dos arquivos na Steam e instale de novo.' }
    Rename-Item $target 'swscale-7_orig.dll'
} elseif ((Test-Path $target) -and -not (Is-Proxy $target)) { Copy-Item $target $orig -Force }
Copy-Item (Join-Path $Here 'bin\swscale-7.dll') $target -Force
Write-Host 'Instalado: bin\swscale-7.dll (base dos lideres)'

# 3) dados do lider
Copy-Item (Join-Path $Here "data\$Leader") $mods -Recurse -Force
Set-Content (Join-Path $state "$Leader.instalado") (Get-Date -Format s)
Write-Host "Instalado: Mods\$Leader (marque o mod ao criar um JOGO NOVO)"
Write-Host ''
Write-Host "$Leader instalado." -ForegroundColor Green
