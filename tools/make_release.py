# Builds the release packages of Project ARA: Reborn (one independent zip per mod) into dist\.
# Compiles the DLLs with MinGW-w64 (gcc on PATH, or the ARA_GCC environment variable), generates install.ps1/uninstall.ps1 from the
# templates, adds INSTALL.bat/UNINSTALL.bat (double-click) and the mod README.   Usage: python tools\make_release.py
import os, re, shutil, subprocess, zipfile, hashlib
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, 'dist')
GCC = os.environ.get('ARA_GCC') or shutil.which('gcc') or 'gcc'     # set ARA_GCC to a MinGW-w64 gcc.exe if gcc is not on PATH
def tpl(n): return open(os.path.join(ROOT, 'tools', n), encoding='utf-8').read()
TPL_I, TPL_U = tpl('installer_template.ps1'), tpl('uninstaller_template.ps1')
TPL_LI, TPL_LU = tpl('leader_install_template.ps1'), tpl('leader_uninstall_template.ps1')

RAIL_I = r'''
# 3) Train Station 3D model (optional): edits game packages starting from the originals (backup in Mods\Ferrovias\station_backup)
if ($Station3D) {
    $py = (Get-Command python -ErrorAction SilentlyContinue)
    if (-not $py) { Write-Warning 'Python 3 not found: the Train Station 3D model was not installed (the mod works with the factory model).' }
    else {
        & python (Join-Path $Here 'station\station.py') install $game
        if ($LASTEXITCODE -ne 0) { Write-Warning 'The Train Station 3D model failed; the rest of the mod is installed.' }
        else { Copy-Item (Join-Path $Here 'station') (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Ara History Untold\Mods\Ferrovias') -Recurse -Force }
    }
}
'''
RAIL_U = r'''
# Train Station 3D model: restore the original game packages (before deleting the mod folder, which holds the backup)
$mod = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Ara History Untold\Mods\Ferrovias'
if (Test-Path (Join-Path $mod 'station_backup\installed.json')) {
    $script = Join-Path $mod 'station\station.py'
    if (-not (Test-Path $script)) { $script = Join-Path $Here 'station\station.py' }
    if ((Get-Command python -ErrorAction SilentlyContinue) -and (Test-Path $script)) { & python $script restore $game }
    if ($LASTEXITCODE -ne 0 -or -not (Get-Command python -ErrorAction SilentlyContinue)) {
        throw "Could not restore the Train Station packages. Use Steam's 'Verify integrity of game files' and run the uninstaller again." }
}
'''
# key = zip name, dir = folder in mods\, data = mod folder name used by the game (kept for save compatibility)
MODS = [
    dict(key='AraQoL', dir='AraQoL', name='AraQoL', ver='2.0', proxy='swresample-4.dll', data='',
         src='mods/AraQoL/src', build=['ara_qol.c', 'proxy.def'], libs=[]),
    dict(key='CpuFix', dir='CpuFix', name='ARA CPU Fix', ver='1.0', proxy='avutil-58.dll', data='',
         src='mods/CpuFix/src', build=['cpu_fix.c', 'proxy.def'], libs=[]),
    dict(key='Railroads', dir='Railroads', name='Railroads', ver='1.9', proxy='avformat-60.dll', data='Ferrovias',
         src='mods/Railroads/src', build=['ferrovias.c', 'proxy.def'], libs=[], extra_dirs=['station'],
         params=', [switch]$Station3D', paramdoc=' [-Station3D]', ei=RAIL_I, eu=RAIL_U, bat_args=' -Station3D'),
    # nation mods: each zip carries the shared LEADER BASE (DLL swscale-7 + game package patches, mods\LeaderBase) and its own data
    dict(key='Brazil', dir='Brazil', leader=True, name='Brazil - Getulio Vargas', ver='1.0', proxy='swscale-7.dll', data='Brasil',
         src='mods/LeaderBase/src', build=['ara_unlock.c', 'proxy.def'], libs=['-lwinmm']),
    dict(key='SovietUnion', dir='SovietUnion', leader=True, name='Soviet Union - Joseph Stalin', ver='1.0', proxy='swscale-7.dll', data='URSS',
         src='mods/LeaderBase/src', build=['ara_unlock.c', 'proxy.def'], libs=['-lwinmm']),
]

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
        mdir = os.path.join(ROOT, 'mods', m['dir'])
        if m['data']: shutil.copytree(os.path.join(mdir, 'data', m['data']), os.path.join(d, 'data', m['data']))
        for x in m.get('extra_dirs', []): shutil.copytree(os.path.join(mdir, x), os.path.join(d, x), ignore=shutil.ignore_patterns('__pycache__'))
        shutil.copy2(os.path.join(mdir, 'README.md'), os.path.join(d, 'README.md'))
        if m.get('leader'):
            base = os.path.join(d, 'base'); lb = os.path.join(ROOT, 'mods', 'LeaderBase')
            shutil.copytree(os.path.join(lb, 'patches'), os.path.join(base, 'patches'))
            shutil.copytree(os.path.join(lb, 'files'), os.path.join(base, 'files'))
            shutil.copy2(os.path.join(ROOT, 'tools', 'rpatch.ps1'), os.path.join(base, 'rpatch.ps1'))
            shutil.copy2(os.path.join(lb, 'README.md'), os.path.join(base, 'BASE_README.md'))
            i = TPL_LI.replace('@@LEADER@@', m['data']).replace('@@DISPLAY@@', m['name'])
            u = TPL_LU.replace('@@LEADER@@', m['data']).replace('@@DISPLAY@@', m['name'])
        else:
            rep = {'@@NAME@@': '%s %s' % (m['name'], m['ver']), '@@PROXY@@': m['proxy'], '@@MARKER@@': m['proxy'].replace('.dll', '_orig.'),
                   '@@DATA@@': m['data'], '@@PARAMS@@': m.get('params', ''), '@@PARAMDOC@@': m.get('paramdoc', '')}
            i, u = TPL_I, TPL_U
            for k, v in rep.items(): i = i.replace(k, v); u = u.replace(k, v)
            i = i.replace('@@EXTRA@@', m.get('ei', '')); u = u.replace('@@EXTRA@@', m.get('eu', ''))
        open(os.path.join(d, 'install.ps1'), 'w', encoding='utf-8-sig', newline='\r\n').write(i)
        open(os.path.join(d, 'uninstall.ps1'), 'w', encoding='utf-8-sig', newline='\r\n').write(u)
        open(os.path.join(d, 'INSTALL.bat'), 'w', newline='\r\n').write('@echo off\r\npowershell -NoProfile -ExecutionPolicy Bypass -File "%%~dp0install.ps1"%s\r\npause\r\n' % m.get('bat_args', ''))
        open(os.path.join(d, 'UNINSTALL.bat'), 'w', newline='\r\n').write('@echo off\r\npowershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1"\r\npause\r\n')
        z = os.path.join(DIST, pkg + '.zip')
        with zipfile.ZipFile(z, 'w', zipfile.ZIP_DEFLATED) as zf:
            for b, _, files in os.walk(d):
                for f in files: zf.write(os.path.join(b, f), os.path.relpath(os.path.join(b, f), DIST))
        sums[pkg + '.zip'] = hashlib.sha256(open(z, 'rb').read()).hexdigest()
        print('%-36s %9d bytes' % (pkg + '.zip', os.path.getsize(z)))
    exe = build_installer()
    sums[os.path.basename(exe)] = hashlib.sha256(open(exe, 'rb').read()).hexdigest()
    sz = exe[:-4] + '.zip'                      # the Setup in a zip (Nexus Mods and other sites that prefer archives)
    with zipfile.ZipFile(sz, 'w', zipfile.ZIP_DEFLATED) as zf: zf.write(exe, os.path.basename(exe))
    sums[os.path.basename(sz)] = hashlib.sha256(open(sz, 'rb').read()).hexdigest()
    print('%-36s %9d bytes' % (os.path.basename(sz), os.path.getsize(sz)))
    open(os.path.join(DIST, 'SHA256SUMS.txt'), 'w', newline='\n').write(''.join('%s  %s\n' % (h, f) for f, h in sums.items()))

def build_installer():
    """Single-file installer (installer\\, C# WPF on .NET Framework 4.8) with every zip of dist\\ embedded."""
    proj = os.path.join(ROOT, 'installer')
    subprocess.run(['dotnet', 'build', proj, '-c', 'Release', '--no-incremental', '-nologo', '-v', 'q'], check=True)
    ver = re.search(r'<Version>([^<]+)</Version>', open(os.path.join(proj, 'RebornSetup.csproj'), encoding='utf-8').read()).group(1)
    exe = os.path.join(DIST, 'ProjectARA-Reborn-Setup-%s.exe' % ver)
    shutil.copy2(os.path.join(proj, 'bin', 'Release', 'net48', 'ProjectARA-Reborn-Setup.exe'), exe)
    print('%-36s %9d bytes' % (os.path.basename(exe), os.path.getsize(exe)))
    return exe

if __name__ == '__main__':
    main()
