# Monta os pacotes de cada mod do Project ARA: Reborn (um zip por mod, independentes) em dist\.
# Compila as DLLs com o MinGW (E:\RE-TOOLS\mingw64 ou gcc no PATH), gera install.ps1/uninstall.ps1 a partir dos modelos,
# poe INSTALAR.bat/DESINSTALAR.bat (duplo clique) e o README do mod. uso: python tools\make_release.py
import os, shutil, subprocess, zipfile, hashlib, json
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, 'dist')
GCC = r'E:\RE-TOOLS\mingw64\bin\gcc.exe' if os.path.exists(r'E:\RE-TOOLS\mingw64\bin\gcc.exe') else 'gcc'
TPL_I = open(os.path.join(ROOT, 'tools', 'installer_template.ps1'), encoding='utf-8').read()
TPL_U = open(os.path.join(ROOT, 'tools', 'uninstaller_template.ps1'), encoding='utf-8').read()

FERROVIAS_I = r'''
# 3) Train Station 3D (opcional): edita pacotes do jogo a partir dos originais (backup em Mods\Ferrovias\estacao_backup)
if ($Estacao3D) {
    $py = (Get-Command python -ErrorAction SilentlyContinue)
    if (-not $py) { Write-Warning 'Python 3 nao encontrado: Train Station 3D nao instalada (o mod funciona com o visual da fabrica).' }
    else {
        & python -c "import numpy" 2>$null
        if ($LASTEXITCODE -ne 0) { Write-Host 'Instalando numpy (pip)...'; & python -m pip install --user numpy }
        & python (Join-Path $Here 'estacao\estacao.py') instalar $game
        if ($LASTEXITCODE -ne 0) { Write-Warning 'A Train Station 3D falhou; o resto do mod esta instalado.' }
        else { Copy-Item (Join-Path $Here 'estacao') (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Ara History Untold\Mods\Ferrovias') -Recurse -Force }
    }
}
'''
FERROVIAS_U = r'''
# Train Station 3D: devolve os pacotes originais do jogo (antes de apagar a pasta do mod, onde fica o backup)
$mod = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Ara History Untold\Mods\Ferrovias'
if (Test-Path (Join-Path $mod 'estacao_backup\instalado.json')) {
    $script = Join-Path $mod 'estacao\estacao.py'
    if (-not (Test-Path $script)) { $script = Join-Path $Here 'estacao\estacao.py' }
    if ((Get-Command python -ErrorAction SilentlyContinue) -and (Test-Path $script)) { & python $script restaurar $game }
    if ($LASTEXITCODE -ne 0 -or -not (Get-Command python -ErrorAction SilentlyContinue)) {
        throw 'Nao consegui restaurar os pacotes da Train Station 3D. Use Verificar integridade dos arquivos na Steam e rode o desinstalador de novo.' }
}
'''
MODS = [
    dict(key='AraQoL', name='AraQoL', ver='2.0', proxy='swresample-4.dll', marker='swresample-4_orig.', data='',
         src='mods/AraQoL/src', build=['ara_qol.c', 'proxy.def'], libs=[]),
    dict(key='CpuFix', name='ARA CPU Fix', ver='1.0', proxy='avutil-58.dll', marker='avutil-58_orig.', data='',
         src='mods/CpuFix/src', build=['cpu_fix.c', 'proxy.def'], libs=[]),
    dict(key='Ferrovias', name='Ferrovias (Railroads)', ver='1.9', proxy='avformat-60.dll', marker='avformat-60_orig.', data='Ferrovias',
         src='mods/Ferrovias/src', build=['ferrovias.c', 'proxy.def'], libs=[], extra_dirs=['estacao'],
         params=', [switch]$Estacao3D', paramdoc=' [-Estacao3D]', ei=FERROVIAS_I, eu=FERROVIAS_U, bat_args=' -Estacao3D'),
    # lideres: cada zip leva a BASE compartilhada (DLL swscale-7 + patches dos pacotes do jogo, mods\Lideres) e os dados do lider
    dict(key='Brasil', leader=True, name='Brasil - Getulio Vargas', ver='1.0', proxy='swscale-7.dll', marker='swscale-7_orig.', data='Brasil',
         src='mods/Lideres/src', build=['ara_unlock.c', 'proxy.def'], libs=['-lwinmm']),
    dict(key='URSS', leader=True, name='URSS - Josef Stalin', ver='1.0', proxy='swscale-7.dll', marker='swscale-7_orig.', data='URSS',
         src='mods/Lideres/src', build=['ara_unlock.c', 'proxy.def'], libs=['-lwinmm']),
]
TPL_LI = open(os.path.join(ROOT, 'tools', 'leader_install_template.ps1'), encoding='utf-8').read()
TPL_LU = open(os.path.join(ROOT, 'tools', 'leader_uninstall_template.ps1'), encoding='utf-8').read()

def build(m):
    src = os.path.join(ROOT, m['src']); out = os.path.join(src, m['proxy'])
    cmd = [GCC, '-O2', '-fno-strict-aliasing', '-Wall', '-Wno-unused-function', '-Wno-misleading-indentation', '-shared', '-o', out] + m['build'] + ['-static-libgcc'] + m['libs']
    env = dict(os.environ); env['PATH'] = os.path.dirname(GCC) + os.pathsep + env['PATH']
    subprocess.run(cmd, cwd=src, check=True, env=env)
    return out

def main():
    shutil.rmtree(DIST, ignore_errors=True); os.makedirs(DIST)
    sums = {}
    for m in MODS:
        dll = build(m)
        pkg = 'Reborn-%s-%s' % (m['key'], m['ver']); d = os.path.join(DIST, pkg)
        os.makedirs(os.path.join(d, 'bin'))
        shutil.copy2(dll, os.path.join(d, 'bin', m['proxy']))
        mdir = os.path.join(ROOT, 'mods', m['key'])
        if m['data']: shutil.copytree(os.path.join(mdir, 'data', m['data']), os.path.join(d, 'data', m['data']))
        for x in m.get('extra_dirs', []): shutil.copytree(os.path.join(mdir, x), os.path.join(d, x), ignore=shutil.ignore_patterns('__pycache__'))
        shutil.copy2(os.path.join(mdir, 'README.md'), os.path.join(d, 'README.md'))
        if m.get('leader'):
            base = os.path.join(d, 'base'); lid = os.path.join(ROOT, 'mods', 'Lideres')
            shutil.copytree(os.path.join(lid, 'patches'), os.path.join(base, 'patches'))
            shutil.copytree(os.path.join(lid, 'files'), os.path.join(base, 'files'))
            shutil.copy2(os.path.join(ROOT, 'tools', 'rpatch.ps1'), os.path.join(base, 'rpatch.ps1'))
            shutil.copy2(os.path.join(lid, 'README.md'), os.path.join(base, 'LEIA-ME_BASE.md'))
            i, u = TPL_LI.replace('@@LEADER@@', m['data']), TPL_LU.replace('@@LEADER@@', m['data'])
        else:
            i = u = None
        rep = {'@@NAME@@': '%s %s' % (m['name'], m['ver']), '@@PROXY@@': m['proxy'], '@@MARKER@@': m['marker'], '@@DATA@@': m['data'],
               '@@PARAMS@@': m.get('params', ''), '@@PARAMDOC@@': m.get('paramdoc', '')}
        if i is None:
            i, u = TPL_I, TPL_U
            for k, v in rep.items(): i = i.replace(k, v); u = u.replace(k, v)
            i = i.replace('@@EXTRA@@', m.get('ei', '')); u = u.replace('@@EXTRA@@', m.get('eu', ''))
        open(os.path.join(d, 'install.ps1'), 'w', encoding='utf-8-sig', newline='\r\n').write(i)
        open(os.path.join(d, 'uninstall.ps1'), 'w', encoding='utf-8-sig', newline='\r\n').write(u)
        open(os.path.join(d, 'INSTALAR.bat'), 'w', newline='\r\n').write('@echo off\r\npowershell -NoProfile -ExecutionPolicy Bypass -File "%%~dp0install.ps1"%s\r\npause\r\n' % m.get('bat_args', ''))
        open(os.path.join(d, 'DESINSTALAR.bat'), 'w', newline='\r\n').write('@echo off\r\npowershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1"\r\npause\r\n')
        z = os.path.join(DIST, pkg + '.zip')
        with zipfile.ZipFile(z, 'w', zipfile.ZIP_DEFLATED) as zf:
            for base, _, files in os.walk(d):
                for f in files: zf.write(os.path.join(base, f), os.path.relpath(os.path.join(base, f), DIST))
        sums[pkg + '.zip'] = hashlib.sha256(open(z, 'rb').read()).hexdigest()
        print('%-34s %8d bytes' % (pkg + '.zip', os.path.getsize(z)))
    open(os.path.join(DIST, 'SHA256SUMS.txt'), 'w', newline='\n').write(''.join('%s  %s\n' % (h, f) for f, h in sums.items()))

if __name__ == '__main__':
    main()
