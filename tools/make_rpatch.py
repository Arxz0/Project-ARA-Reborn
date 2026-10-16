# Gera um patch binario ORIGINAL -> MODIFICADO que so carrega o que e do mod: o resto vira "copie N bytes do original
# a partir de X". O instalador (tools\rpatch.ps1) refaz o arquivo modificado a partir do arquivo original de quem instala
# e confere o SHA-1 do resultado. Nenhum conteudo do jogo vai no patch (so tabelas de indice e o que os mods acrescentam).
# Formato (little-endian): 'ARAPATCH' u32 versao=1 | u64 tam_orig | sha1_orig[20] | u64 tam_novo | sha1_novo[20] | u32 nops |
#   ops: u8 0 = COPY u64 origem u64 tamanho | u8 1 = DATA u64 tamanho + bytes
# uso: make_rpatch.py original modificado saida.rpatch [texture|model|small]
import sys, os, mmap, struct, hashlib, bisect
MINRUN = 32; STEP = 16

def sha1_file(p):
    h = hashlib.sha1()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(16 << 20), b''): h.update(b)
    return h.digest()

def matchlen(O, so, M, sm, cap):
    """quantos bytes iguais a partir de O[so] e M[sm] (ate cap)"""
    if cap <= 0 or so < 0: return 0
    n = 0; step = 64
    while n < cap:
        k = min(step, cap - n)
        if O[so + n:so + n + k] == M[sm + n:sm + n + k]: n += k; step = min(step * 4, 1 << 22); continue
        lo, hi = 0, k                                            # primeiro byte diferente dentro deste pedaco
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if O[so + n:so + n + mid] == M[sm + n:sm + n + mid]: lo = mid
            else: hi = mid
        if O[so + n:so + n + hi] == M[sm + n:sm + n + hi]: lo = hi
        return n + lo
    return n

def tex_cands(O, M):
    def idx(b):
        ver, cnt, hs, isz, ds, dsz = struct.unpack_from('<6I', b, 0)
        return {struct.unpack_from('<Q', b, 0x20 + i * 0x30)[0]: struct.unpack_from('<6Q', b, 0x20 + i * 0x30) for i in range(cnt)}, ds
    eo, _ = idx(O); em, dsm = idx(M)
    iv = sorted((e[2], e[2] + e[3], (eo[k][2] - e[2]) if k in eo and eo[k][3] == e[3] else None) for k, e in em.items())
    starts = [a for a, _, _ in iv]
    def f(pos):
        i = bisect.bisect_right(starts, pos) - 1
        if i >= 0 and iv[i][0] <= pos < iv[i][1] and iv[i][2] is not None: return [iv[i][2]]
        return []
    return f

def model_cands(O, M):
    dso = struct.unpack_from('<I', O, 0x10)[0]; dsm = struct.unpack_from('<I', M, 0x10)[0]
    tab = [k * 0xF0 for k in range(-16, 17)]
    return lambda pos: tab if pos < dsm else [dso - dsm]

def small_cands(O, M):
    """ancoras: pedacos de 16 bytes de M (a cada 64) achados em O dao os deslocamentos candidatos da regiao"""
    where = {}
    for o in range(0, len(O) - 16, 4): where.setdefault(O[o:o + 16], o)
    samp = []
    for s in range(0, len(M) - 20, 64):
        ds = set()
        for j in range(4):
            o = where.get(M[s + j:s + j + 16])
            if o is not None: ds.add(o - (s + j))
        samp.append(sorted(ds))
    def f(pos):
        i = pos // 64; out = []
        for k in range(max(0, i - 4), min(len(samp), i + 5)):
            for d in samp[k]:
                if d not in out: out.append(d)
        return out
    return f

def make(op, mp, outp, kind):
    fo, fm = open(op, 'rb'), open(mp, 'rb')
    O = mmap.mmap(fo.fileno(), 0, access=mmap.ACCESS_READ); M = mmap.mmap(fm.fileno(), 0, access=mmap.ACCESS_READ)
    cf = {'texture': tex_cands, 'model': model_cands, 'small': small_cands}[kind](O, M)
    N, NO = len(M), len(O); ops = []; pos = 0; last = 0; data_bytes = 0
    def cands(p):
        out = [last]
        for d in cf(p) + [0]:
            if d not in out: out.append(d)
        return out
    while pos < N:
        bd, bl = None, 0
        for d in cands(pos):
            if not (0 <= pos + d < NO): continue
            L = matchlen(O, pos + d, M, pos, min(N - pos, NO - pos - d))
            if L > bl: bd, bl = d, L
            if bl >= 1 << 20: break
        if bl >= MINRUN:
            if ops and ops[-1][0] == 0 and ops[-1][1] + ops[-1][2] == pos + bd: ops[-1][2] += bl
            else: ops.append([0, pos + bd, bl])
            pos += bl; last = bd; continue
        start = pos; pos = min(N, pos + STEP)
        while pos < N:
            if any(0 <= pos + d < NO and matchlen(O, pos + d, M, pos, min(MINRUN, N - pos, NO - pos - d)) >= MINRUN for d in cands(pos)): break
            pos = min(N, pos + STEP)
        if ops and ops[-1][0] == 1 and ops[-1][1] + ops[-1][2] == start: ops[-1][2] += pos - start
        else: ops.append([1, start, pos - start])
        data_bytes += pos - start
    with open(outp, 'wb') as f:
        f.write(b'ARAPATCH' + struct.pack('<IQ', 1, NO) + sha1_file(op) + struct.pack('<Q', N) + sha1_file(mp) + struct.pack('<I', len(ops)))
        for t, a, b in ops:
            if t == 0: f.write(struct.pack('<BQQ', 0, a, b))
            else: f.write(struct.pack('<BQ', 1, b)); f.write(M[a:a + b])
    print('%s: %d ops, %d bytes de dados do mod, patch %d bytes' % (os.path.basename(mp), len(ops), data_bytes, os.path.getsize(outp)))

def apply(op, pp, outp):
    """aplicador de referencia (o instalador usa tools\\rpatch.ps1); confere os SHA-1"""
    d = open(pp, 'rb').read(); assert d[:8] == b'ARAPATCH'
    ver, no = struct.unpack_from('<IQ', d, 8); so = d[20:40]; nn = struct.unpack_from('<Q', d, 40)[0]; sn = d[48:68]; nops = struct.unpack_from('<I', d, 68)[0]
    assert sha1_file(op) == so, 'original diferente'
    i = 72; fo = open(op, 'rb')
    with open(outp, 'wb') as f:
        for _ in range(nops):
            t = d[i]; i += 1
            if t == 0:
                a, b = struct.unpack_from('<QQ', d, i); i += 16; fo.seek(a)
                while b: k = min(b, 16 << 20); f.write(fo.read(k)); b -= k
            else:
                b = struct.unpack_from('<Q', d, i)[0]; i += 8; f.write(d[i:i + b]); i += b
    assert sha1_file(outp) == sn, 'resultado diferente'
    return True

if __name__ == '__main__':
    if sys.argv[1] == 'apply': print('ok' if apply(*sys.argv[2:5]) else 'falhou')
    else: make(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else 'small')
