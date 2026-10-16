# Leitura generica do contêiner ZRPF do Characters.OxAssetInfoPackage (para estudar como ACRESCENTAR assets).
# Layout: cabecalho ZRPF (0x50) | ZNOC (0x20) | blocos ZRBF | indice (N x 32) | tabela de strings (M x 16) | pool de strings
#   ZRPF: +4 tam do arquivo, +8 0x50, +0x2c/+0x30 tam do payload (arquivo-0x50), +0x44 mm3(cabecalho[0:0x44]), +0x4c mm3(arquivo[0x50:])
#   ZNOC (0x50): +4 versao 3, +8 off do indice (dwords, rel 0x50), +0xc tam do indice (dwords), +0x10 tam da tabela (dwords), +0x14 tam do pool (dwords)
#   indice: {pathhash, tipo, off(dwords rel 0x50), size(dwords), mm3(bloco), 0,0,0} ordenado por pathhash
#   tabela: {hash, off(dwords rel 0x50), len, 0} ordenada por hash
import struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
def mm3(data, seed=0):
    c1, c2 = 0xcc9e2d51, 0x1b873593; h = seed; n = len(data)
    for i in range(0, n // 4 * 4, 4):
        k = struct.unpack_from('<I', data, i)[0]; k = (k * c1) & 0xffffffff; k = ((k << 15) | (k >> 17)) & 0xffffffff; k = (k * c2) & 0xffffffff
        h ^= k; h = ((h << 13) | (h >> 19)) & 0xffffffff; h = (h * 5 + 0xe6546b64) & 0xffffffff
    t = data[n // 4 * 4:]; k = 0
    if len(t) >= 3: k ^= t[2] << 16
    if len(t) >= 2: k ^= t[1] << 8
    if len(t) >= 1:
        k ^= t[0]; k = (k * c1) & 0xffffffff; k = ((k << 15) | (k >> 17)) & 0xffffffff; k = (k * c2) & 0xffffffff; h ^= k
    h ^= n; h ^= h >> 16; h = (h * 0x85ebca6b) & 0xffffffff; h ^= h >> 13; h = (h * 0xc2b2ae35) & 0xffffffff; h ^= h >> 16
    return h
AI = None
class Pkg:
    def __init__(s, path=AI):
        d = s.d = bytearray(open(path, 'rb').read())
        s.znoc = list(struct.unpack_from('<8I', d, 0x50))
        io = s.znoc[2] * 4 + 0x50; n = s.znoc[3] * 4 // 32
        s.ent = [list(struct.unpack_from('<8I', d, io + i * 32)) for i in range(n)]
        so = io + n * 32; m = s.znoc[4] * 4 // 16
        s.strs = {}
        for i in range(m):
            h, off, ln, z = struct.unpack_from('<4I', d, so + i * 16)
            s.strs[h] = bytes(d[off * 4 + 0x50:off * 4 + 0x50 + ln])
        s.blocks = {}   # pathhash -> bytes do bloco ZRBF inteiro
        for e in s.ent: s.blocks[e[0]] = bytes(d[e[2] * 4 + 0x50:e[2] * 4 + 0x50 + e[3] * 4])
        s.hdr = bytes(d[:0x50]); s.znoc_raw = bytes(d[0x50:0x70])
    def name(s, h): return s.strs.get(h, b'?').decode('latin1')
    def build(s):
        """Remonta o arquivo na ordem original dos blocos (por offset), recalculando offsets, hashes e tamanhos."""
        order = sorted(s.ent, key=lambda e: e[2]) if all(len(s.blocks[e[0]]) == e[3] * 4 for e in s.ent) else s.ent
        order = sorted(s.ent, key=lambda e: (e[2], e[0]))
        out = bytearray(s.hdr) + bytearray(s.znoc_raw); pos = {}
        for e in order:
            b = s.blocks[e[0]]; assert len(b) % 4 == 0
            pos[e[0]] = (len(out) - 0x50) // 4; out += b
        ent = sorted(s.ent, key=lambda e: e[0])
        io = len(out)
        for e in ent:
            b = s.blocks[e[0]]
            out += struct.pack('<8I', e[0], e[1], pos[e[0]], len(b) // 4, mm3(b), 0, 0, 0)
        hs = sorted(s.strs)
        so = len(out); po = so + len(hs) * 16; pool = bytearray(); tab = bytearray()
        for h in hs:
            v = s.strs[h]; tab += struct.pack('<4I', h, (po + len(pool) - 0x50) // 4, len(v), 0)
            pool += v
            while len(pool) % 4: pool += b'\0'
        out += tab + pool
        while len(out) % 16: out += b'\0'
        struct.pack_into('<I', out, 4, len(out))
        struct.pack_into('<II', out, 0x2c, len(out) - 0x50, len(out) - 0x50)
        struct.pack_into('<5I', out, 0x54, s.znoc[1], (io - 0x50) // 4, len(ent) * 8, len(hs) * 4, len(pool) // 4)
        struct.pack_into('<I', out, 0x44, mm3(bytes(out[:0x44])))   # hash de integridade do cabecalho ("Pack file header integrity hash")
        struct.pack_into('<I', out, 0x4c, mm3(bytes(out[0x50:])))
        return out
if __name__ == '__main__':
    p = Pkg(); o = p.build(); orig = open(AI, 'rb').read()
    print('len', len(o), len(orig), 'identico' if bytes(o) == orig else 'DIFERENTE')
    if bytes(o) != orig:
        for i in range(0, min(len(o), len(orig)), 4):
            if o[i:i + 4] != orig[i:i + 4]: print('1a diferenca em %x: %s vs %s' % (i, o[i:i + 16].hex(), orig[i:i + 16].hex())); break
