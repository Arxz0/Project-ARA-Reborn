# Project ARA: Reborn - desinstalador do lider @@LEADER@@ (gerado por tools\make_release.py a partir deste modelo).
# Remove Mods\@@LEADER@@. Se nenhum outro lider do Reborn continuar instalado, devolve tambem a base: pacotes originais do jogo
# (pelos backups em Mods\_RebornLideres) e a DLL bin\swscale-7.dll original.
param([string]$GamePath = "", [string]$ModsPath = "")
$ErrorActionPreference = 'Stop'
$Leader = '@@LEADER@@'
$Here   = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $Here 'base\rpatch.ps1')
$Patches = @('assets\PackagedData\Characters.OxAssetInfoPackage', 'assets\PackagedDataHD\Characters.OxAssetInfoPackage',
             'assets\PackagedData\Characters.OxModelPackage', 'assets\PackagedDataHD\Characters.OxModelPackage',
             'assets\PackagedData\Characters.OxTexturePackage', 'assets\PackagedData\UI.OxTexturePackage', 'assets\UI\Art\Icons\RGBEmblems.xml')
$NewFiles = @('assets\GameCore\Flags\flg_Brazil_Default.xml', 'assets\GameCore\Flags\flg_SovietUnion_Default.xml')
$Others = @('Brasil', 'URSS') | Where-Object { $_ -ne $Leader }

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
if (Get-Process AraHistoryUntold -ErrorAction SilentlyContinue) { throw 'Feche o jogo antes de desinstalar.' }
$mods  = if ($ModsPath) { $ModsPath } else { Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Ara History Untold\Mods' }
$state = Join-Path $mods '_RebornLideres'
if (Test-Path (Join-Path $mods $Leader)) { Remove-Item (Join-Path $mods $Leader) -Recurse -Force; Write-Host "Removido: Mods\$Leader" }
Remove-Item (Join-Path $state "$Leader.instalado") -ErrorAction SilentlyContinue
$still = $Others | Where-Object { Test-Path (Join-Path $state "$_.instalado") }
if ($still) { Write-Host "A base continua instalada (usada por: $($still -join ', '))."; Write-Host "$Leader desinstalado." -ForegroundColor Green; return }

# ultima lider: devolve a base
$falta = @()
foreach ($p in $Patches) {
    $target = Join-Path $game $p; $bak = Join-Path $state (($p -replace '[\\:]', '_') + '.rbak')
    if (-not (Test-Path $bak)) { $falta += $p; continue }
    switch ([RPatch]::Revert($target, $bak)) { 0 { Write-Host "Original devolvido: $p" } 1 { Write-Host "Ja era o original: $p" } default { $falta += $p } }
}
foreach ($f in $NewFiles) { Remove-Item (Join-Path $game $f) -ErrorAction SilentlyContinue }
$bin = Join-Path $game 'bin'; $target = Join-Path $bin 'swscale-7.dll'; $orig = Join-Path $bin 'swscale-7_orig.dll'
if (Test-Path $orig) {
    if ((Test-Path $target) -and (Is-Proxy $target)) { Remove-Item $target -Force }
    if (-not (Test-Path $target)) { Rename-Item $orig 'swscale-7.dll' } else { Remove-Item $orig -Force }
    Write-Host 'Restaurado: bin\swscale-7.dll original'
}
if ($falta.Count) {
    Write-Warning ("Nao consegui devolver sozinho: " + ($falta -join ', ') + ". Use Verificar integridade dos arquivos na Steam para restaurar esses arquivos.")
} else { Remove-Item $state -Recurse -Force -ErrorAction SilentlyContinue }
Write-Host "$Leader desinstalado." -ForegroundColor Green
