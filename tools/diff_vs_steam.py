# Compares the game folder with the Steam manifest: MODIFIED files (different sha1) and files that do NOT exist in the
# manifest (added by mods). Usage: diff_vs_steam.py game_folder file.manifest [out.json]
import sys, os, hashlib, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from steam_manifest import load
game, man = sys.argv[1], sys.argv[2]
files, _ = load(man)
ref = {m['name'].decode().replace('/', '\\').lower(): m for m in files if m.get('size') or m.get('sha', '0' * 40) != '0' * 40}
def sha1(p):
    h = hashlib.sha1()
    with open(p, 'rb') as f:
        while True:
            b = f.read(16 << 20)
            if not b: break
            h.update(b)
    return h.hexdigest()
mod, extra = [], []
for base, dirs, fs in os.walk(game):
    for f in fs:
        p = os.path.join(base, f); rel = os.path.relpath(p, game); k = rel.lower()
        m = ref.get(k)
        if not m: extra.append(rel); continue
        sz = os.path.getsize(p)
        if sz != m['size'] or sha1(p) != m['sha']: mod.append((rel, sz, m['size']))
print('MODIFIED:'); [print('  %s (%d bytes; original %d)' % x) for x in mod]
print('NOT IN MANIFEST: %d' % len(extra))
for e in extra:
    if not e.lower().startswith(('assets\\savegames', 'bin\\ara_')) and not e.lower().endswith(('.log', '.ini')): print('  ' + e)
if len(sys.argv) > 3: json.dump({'mod': mod, 'extra': extra}, open(sys.argv[3], 'w'), indent=1)
