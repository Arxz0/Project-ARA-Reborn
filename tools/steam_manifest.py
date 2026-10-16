# Le um manifesto de depot da Steam (depotcache\<depot>_<manifest>.manifest) e lista {tamanho, sha1 do conteudo, nome}.
# Serve para conferir se um arquivo e o ORIGINAL da Steam sem precisar baixar nada. uso: steam_manifest.py arquivo.manifest [filtro]
import sys, struct
def varint(b, i):
    r = s = 0
    while True:
        c = b[i]; i += 1; r |= (c & 0x7f) << s; s += 7
        if c < 0x80: return r, i
def fields(b):
    i = 0
    while i < len(b):
        k, i = varint(b, i); f, t = k >> 3, k & 7
        if t == 0: v, i = varint(b, i)
        elif t == 2: n, i = varint(b, i); v = b[i:i + n]; i += n
        elif t == 5: v = b[i:i + 4]; i += 4
        elif t == 1: v = b[i:i + 8]; i += 8
        else: raise ValueError('tipo %d' % t)
        yield f, v
def load(path):
    d = open(path, 'rb').read(); i = 0; files = []; meta = {}
    while i + 8 <= len(d):
        magic, n = struct.unpack_from('<II', d, i); i += 8
        if magic == 0x32C415AB: break
        blob = d[i:i + n]; i += n
        if magic == 0x71F617D0:                                   # ContentManifestPayload
            for f, v in fields(blob):
                if f != 1: continue
                m = {}
                for g, w in fields(v):
                    if g == 1: m['name'] = w
                    elif g == 2: m['size'] = w
                    elif g == 3: m['flags'] = w
                    elif g == 5: m['sha'] = w.hex()
                files.append(m)
        elif magic == 0x1F4812BE:                                 # ContentManifestMetadata
            for f, v in fields(blob): meta[f] = v
    return files, meta
if __name__ == '__main__':
    files, meta = load(sys.argv[1])
    print(len(files), 'arquivos; nomes cifrados:', meta.get(4))
    flt = sys.argv[2].lower().encode() if len(sys.argv) > 2 else b''
    for m in files:
        nm = m.get('name', b'')
        if flt and flt not in nm.lower(): continue
        print(m.get('size'), m.get('sha'), nm.decode('utf-8', 'replace'))
