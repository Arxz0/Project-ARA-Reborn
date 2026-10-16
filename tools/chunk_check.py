# Checks a file chunk by chunk against the chunk SHA-1s of the Steam manifest and lists the chunks that differ.
# Usage: chunk_check.py file relative_path_in_game [manifest]
import sys, os, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from steam_manifest import fields, varint
import struct
MAN = sys.argv[3] if len(sys.argv) > 3 else r'C:\Program Files (x86)\Steam\depotcache\2021881_2028503611726990938.manifest'
def chunks_of(rel):
    d = open(MAN, 'rb').read(); i = 0
    while i + 8 <= len(d):
        magic, n = struct.unpack_from('<II', d, i); i += 8
        blob = d[i:i + n]; i += n
        if magic != 0x71F617D0: continue
        for f, v in fields(blob):
            if f != 1: continue
            name = None; ch = []
            for g, w in fields(v):
                if g == 1: name = w.decode()
                elif g == 6:
                    c = {}
                    for h, x in fields(w):
                        if h == 1: c['sha'] = x.hex()
                        elif h == 3: c['off'] = x
                        elif h == 4: c['len'] = x
                    ch.append(c)
            if name and name.lower() == rel.lower(): return sorted(ch, key=lambda c: c.get('off', 0))
    return []
if __name__ == '__main__':
    path, rel = sys.argv[1], sys.argv[2]
    ch = chunks_of(rel); bad = []
    with open(path, 'rb') as f:
        for c in ch:
            f.seek(c.get('off', 0)); b = f.read(c['len'])
            if hashlib.sha1(b).hexdigest() != c['sha']: bad.append(c)
    print(len(ch), 'chunks;', len(bad), 'different')
    for c in bad[:50]: print('  off %d len %d' % (c.get('off', 0), c['len']))
