# Project ARA: Reborn - instalador de um mod (gerado por tools\make_installers.py a partir deste modelo).
# Uso: clique duas vezes em INSTALAR.bat, ou:  powershell -ExecutionPolicy Bypass -File install.ps1 [-GamePath "...\Ara History Untold"]@@PARAMDOC@@
param([string]$GamePath = ""@@PARAMS@@)
$ErrorActionPreference = 'Stop'
$ModName   = '@@NAME@@'
$ProxyDll  = '@@PROXY@@'          # DLL do jogo que o mod substitui (a original vira <nome>_orig.dll)
$Marker    = '@@MARKER@@'         # texto que so existe na DLL do mod (para nunca renomear a DLL do mod como se fosse a original)
$DataDir   = '@@DATA@@'           # pasta em Documentos\My Games\Ara History Untold\Mods (vazio = nao tem)
$Here      = Split-Path -Parent $MyInvocation.MyCommand.Path

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
        $g = [IO.Path]::Combine($l, 'steamapps', 'common', 'Ara History Untold')   # Combine: biblioteca num disco que nao existe mais nao derruba a busca
        try { if (Test-Path -LiteralPath ([IO.Path]::Combine($g, 'bin', 'AraHistoryUntold.exe'))) { return $g } } catch {}
    }
    throw 'Nao achei o ARA: History Untold. Rode de novo com -GamePath "C:\caminho\Ara History Untold".'
}
function Has-Marker([string]$file, [string]$text) {
    # proxy do mod = DLL cujas exportacoes encaminham para <nome>_orig.dll (vale para qualquer versao do mod)
    return [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($file)).Contains($text)
}

$game = Find-Game
$bin = Join-Path $game 'bin'
Write-Host "Jogo: $game"
if (Get-Process AraHistoryUntold -ErrorAction SilentlyContinue) { throw 'Feche o jogo antes de instalar.' }

# 1) DLL proxy
$target = Join-Path $bin $ProxyDll
$orig = Join-Path $bin ($ProxyDll -replace '\.dll$', '_orig.dll')
if (-not (Test-Path $orig)) {
    if (-not (Test-Path $target)) { throw "Nao achei $ProxyDll na pasta bin do jogo." }
    if (Has-Marker $target $Marker) { throw "$ProxyDll ja e a DLL do mod, mas a original ($([IO.Path]::GetFileName($orig))) sumiu. Use Verificar integridade dos arquivos na Steam e instale de novo." }
    Rename-Item $target ([IO.Path]::GetFileName($orig))
    Write-Host "Original guardada: $([IO.Path]::GetFileName($orig))"
} elseif ((Test-Path $target) -and -not (Has-Marker $target $Marker)) {
    # a Steam (verificacao/atualizacao) devolveu a DLL original: ela vira a nova _orig
    Copy-Item $target $orig -Force
    Write-Host 'A Steam tinha restaurado a DLL original; atualizei a copia _orig.'
}
Copy-Item (Join-Path $Here "bin\$ProxyDll") $target -Force
Write-Host "Instalado: bin\$ProxyDll ($ModName)"

# 2) dados do mod
if ($DataDir) {
    $docs = [Environment]::GetFolderPath('MyDocuments')
    $mods = Join-Path $docs 'My Games\Ara History Untold\Mods'
    New-Item -ItemType Directory -Force $mods | Out-Null
    Copy-Item (Join-Path $Here "data\$DataDir") $mods -Recurse -Force
    Write-Host "Instalado: Mods\$DataDir (marque o mod ao criar um JOGO NOVO: as regras ficam gravadas no save)"
}
@@EXTRA@@
Write-Host ''
Write-Host "$ModName instalado. Log e configuracao ficam em: $bin" -ForegroundColor Green
