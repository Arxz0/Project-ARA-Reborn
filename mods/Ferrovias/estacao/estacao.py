# Train Station do mod Ferrovias (v4 = v3 + decalque do trilho na faixa dos portoes: FX_ChimneySmoke_001 vira Decal_GEN_MAC_TrainStationRail,
#   linha nova em TerrainDecals/DecalDefinitions.csv e 2 texturas novas no TerrainDecals_DecalDefinitions.OxTexturePackage das duas pastas;
#   + caixas apagadas nos 2 portoes centrais; v3 em estacao_v3.py). (v3: so o predio oeste da fabrica, movido para a direita; v2 em estacao_v2_chamines.py): asset 3D proprio GEN_MAC_TrainStation (fabrica GEN_MAC_Factory sem as 3 chamines,
# sem o guindaste animado e sem a fumaca das chamines), sem mudar a fabrica de verdade.
#   Improvements_04_ENL_ATM.OxAssetInfoPackage (PackagedData e PackagedDataHD): registro novo (copia do da fabrica, paleta e
#     TimelineSet compartilhados) + MeshDesc proprio (copia; nos do guindaste e da fumaca com a lista de estados trocada por
#     um estado inexistente = nunca aparecem).
#   Improvements_04_ENL_ATM.OxModelPackage (as duas pastas): entrada nova mm3("Improvements_04_ENL_ATM_GEN_MAC_TrainStation")
#     com a malha da fabrica e os triangulos das chamines degenerados (mesmos tamanhos, nada se desloca).
#   ZData_IndusClientData.OxZDataLibPackage (so PackagedData): o visual sem uso "Improvement_PostOffice" (nenhuma melhoria do
#     jogo nem dos mods usa) recebe o elemento da fabrica com o asset trocado para GEN_MAC_TrainStation.
#     O mod usa .Asset = "Improvement_PostOffice" em imp_TrainStation.
# Sempre parte dos ORIGINAIS guardados em BACKUP (copiados na 1a execucao; refeitos se a Steam atualizar os pacotes).
# uso: python estacao.py [instalar|restaurar] [pasta do jogo]   (sem a pasta, acha pela Steam)
# Project ARA: Reborn - mod Ferrovias. Requer Python 3.10+ e numpy.
import struct, sys, os, shutil, hashlib, json, ctypes, re
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pkg_ai import Pkg, mm3

def find_game():
    if len(sys.argv) > 2 and os.path.isdir(sys.argv[2]): return sys.argv[2]
    if os.environ.get('ARA_GAME') and os.path.isdir(os.environ['ARA_GAME']): return os.environ['ARA_GAME']
    import winreg
    libs = []
    for hive, key, val in ((winreg.HKEY_CURRENT_USER, r'Software\Valve\Steam', 'SteamPath'), (winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath')):
        try:
            with winreg.OpenKey(hive, key) as k: libs.append(winreg.QueryValueEx(k, val)[0])
        except OSError: pass
    for steam in list(libs):
        vdf = os.path.join(steam, 'steamapps', 'libraryfolders.vdf')
        if os.path.exists(vdf): libs += [p.replace('\\\\', '\\') for p in re.findall(r'"path"\s+"([^"]+)"', open(vdf, encoding='utf-8', errors='ignore').read())]
    for lib in libs:
        g = os.path.join(lib, 'steamapps', 'common', 'Ara History Untold')
        if os.path.exists(os.path.join(g, 'bin', 'AraHistoryUntold.exe')): return g
    sys.exit('Nao achei o jogo. Use: python estacao.py instalar "C:\\caminho\\Ara History Untold"')

def documents():
    buf = ctypes.create_unicode_buffer(260)
    ctypes.windll.shell32.SHGetFolderPathW(None, 5, None, 0, buf)   # CSIDL_PERSONAL (Documentos, mesmo redirecionado)
    return buf.value

GAME = os.path.join(find_game(), 'assets')
BACKUP = os.environ.get('ARA_ESTACAO_BACKUP') or os.path.join(documents(), 'My Games', 'Ara History Untold', 'Mods', 'Ferrovias', 'estacao_backup')
PKG = 'Improvements_04_ENL_ATM'
NEW = 'GEN_MAC_TrainStation'
DONOR = 'GEN_MAC_Factory'
NEW_MD = '/GEN/MAC/%s/%s.MeshDesc' % (NEW, NEW)
DONOR_MD = '/GEN/MAC/%s/%s.MeshDesc' % (DONOR, DONOR)
HIDE_NODES = ['Anim_MACCrane_Crane', 'Anim_MACCrane_Direct', 'Anim_MACCrane_Wheel',
              'FX_ChimneySmoke_002', 'FX_ChimneySmoke_003']
# v4: o ponto FX_ChimneySmoke_001 vira o decalque do trilho (mesmos estados do decalque da fundacao, na origem do modelo)
RAIL_NODE_OLD = 'FX_ChimneySmoke_001'
RAIL_NODE = 'Decal_GEN_MAC_TrainStationRail'          # o jogo tira o "Decal_" e procura GEN_MAC_TrainStationRail no CSV
FOUND_NODE = 'Decal_GEN_MAC_Factory'
DECAL = 'GEN_MAC_TrainStationRail'
DECAL_DIR = 'Improvements/GEN/MAC/GEN_MAC_TrainStation/'
DECAL_SRC = os.path.join(HERE, 'decal')
DECAL_TEX = {DECAL_DIR + DECAL + '_Color.DDS': os.path.join(DECAL_SRC, DECAL + '_Color.DDS'),
             DECAL_DIR + DECAL + '_Property.DDS': os.path.join(DECAL_SRC, DECAL + '_Property.DDS')}
DECAL_ROW = '%s,280,280,ZNOShaders.TerrainLayerMaterial.IndusDecal,1,PostTrails,%s,,,,Neutral_Normal.DDS,%s,' % (DECAL, DECAL_DIR + DECAL + '_Color.DDS', DECAL_DIR + DECAL + '_Property.DDS')
CSV_REL = os.path.join('TerrainDecals', 'DecalDefinitions.csv')
DECAL_PKG = 'TerrainDecals_DecalDefinitions.OxTexturePackage'
# v4: caixas dentro dos 2 portoes centrais (espaco do modelo ANTES do deslocamento DX): componente inteiro dentro da caixa
BAYS = [(-36.0, -11.0, -16.5, -3.5), (-36.0, -11.0, 1.5, 14.5)]
NEVER = mm3(b'AraFerroviasNunca')
SPLIT_X = -13.0                  # m: centro a leste disto = bloco principal + anexo (apagado)
DX = 50.0                        # m: quanto o predio do lado oeste anda para a direita
HIDE_Z = -3000.0                 # altura (m) para onde vao os pontos da fumaca e do guindaste
VIZ_SLOT = 'Improvement_PostOffice'
VIZ_DONOR = 'Improvement_Factory'
VIZ_BLOCK = 'IndusClientData.LivingWorldCity.Improvements'
# colunas das chamines (espaco do modelo, Z para cima): componente inteiro dentro da coluna e acima de 12 m
CHIMNEY_BOX = [(-12.9, -3.6, -1.2, 8.6), (-12.9, -3.6, 14.7, 23.7), (-12.9, -3.6, 30.1, 39.6)]
KEEP_NODES = ['Anim_BuildStateCraneA_001', 'Deco_A2_Imp2x2_BuildState_B_001', 'Decal_GEN_Construct_2x2', 'Anim_BuildStateHuman001_001', 'Anim_BuildStateHuman002_001', 'Anim_BuildStateHuman002_002', 'Anim_BuildStateHuman002_003', 'Anim_BuildStateHuman002_004', 'Anim_BuildStateHuman003_001', 'Anim_BuildStateHuman003_002', 'Anim_BuildStateHuman003_003', 'Anim_BuildStateHuman004_001', 'Anim_BuildStateHuman004_002', 'Anim_BuildStateHuman005_001', 'Anim_BuildStateHuman005_002', 'Anim_BuildStateHuman006_001', 'Anim_BuildStateHuman007_001', 'Anim_BuildStateHuman007_002', 'Anim_BuildStateHuman008_001', 'Anim_BuildStateHuman008_002', 'Anim_BuildStateHuman009_001', 'Anim_BuildStateHuman009_002', 'Anim_BuildStateHuman010_001', 'Anim_BuildStateHuman011_001', 'Anim_BuildStateHuman011_002', 'Anim_BuildStateHuman011_003']   # pontos da obra (Anim_BuildState*/Deco_*BuildState*) ficam onde estao
FILES = [('PackagedData', PKG + '.OxAssetInfoPackage'), ('PackagedData', PKG + '.OxModelPackage'),
         ('PackagedDataHD', PKG + '.OxAssetInfoPackage'), ('PackagedDataHD', PKG + '.OxModelPackage'),
         ('PackagedData', 'ZData_IndusClientData.OxZDataLibPackage'),
         ('PackagedData', DECAL_PKG), ('PackagedDataHD', DECAL_PKG), ('.', CSV_REL)]

def zrbf_fix(b):
    b = bytearray(b); dl = struct.unpack_from('<I', b, 0x20)[0]
    struct.pack_into('<I', b, 0x2c, mm3(bytes(b[0x30:0x30 + dl])))
    struct.pack_into('<I', b, 0x24, mm3(bytes(b[:0x24])))
    return bytes(b)

def zrbf_grow(b):   # bloco cujo conteudo cresceu: refaz tamanhos e hashes
    b = bytearray(b)
    while len(b) % 16: b.append(0)
    struct.pack_into('<I', b, 4, len(b)); struct.pack_into('<I', b, 0x20, len(b) - 0x30)
    return zrbf_fix(b)

def ref(D, o):
    a, e, n, h = struct.unpack_from('<4I', D, o)
    return bytes(D[a:a + n]) if 0 < n < 0x400 and a + n <= len(D) and mm3(bytes(D[a:a + n])) == h else None

# ---------- AssetInfo ----------
def meshdesc_hidden(blk):
    assert zrbf_fix(blk) == bytes(blk), 'MeshDesc: cabecalho nao confere'
    b = bytearray(blk); D = memoryview(b)[0x30:]
    ini, fim, cnt, _ = struct.unpack_from('<4I', D, 0)
    assert (fim - ini) == cnt * 0x80
    done = set()
    for i in range(cnt):
        o = ini + i * 0x80; nm = ref(D, o)
        if nm and nm.decode() in HIDE_NODES:
            a, e, n = struct.unpack_from('<3I', D, o + 0x60)
            assert e - a == n * 4
            for k in range(n): struct.pack_into('<I', D, a + 4 * k, NEVER)
            done.add(nm.decode())
    assert done == set(HIDE_NODES), ('nos nao achados', set(HIDE_NODES) - done)
    recs = {}
    for i in range(cnt):
        o = ini + i * 0x80; nm = ref(D, o)
        if nm: recs[nm.decode()] = o
    ro, fo = recs[RAIL_NODE_OLD], recs[FOUND_NODE]
    a1, e1, n1 = struct.unpack_from('<3I', D, ro + 0x60); a2, e2, n2 = struct.unpack_from('<3I', D, fo + 0x60)
    assert n1 == n2 and e1 - a1 == n1 * 4, 'listas de estados com tamanhos diferentes'
    D[a1:e1] = bytes(D[a2:e2])
    del D
    # nome novo no fim do bloco (+0 = nome do ponto, +0x30 = nome do no)
    hdr, body = bytearray(b[:0x30]), bytearray(b[0x30:0x30 + struct.unpack_from('<I', b, 0x20)[0]])
    while len(body) % 16: body.append(0)
    sa = len(body); nb = RAIL_NODE.encode(); body += nb + bytes(1)
    while len(body) % 16: body.append(0)
    rr = struct.pack('<4I', sa, sa + len(nb), len(nb), mm3(nb))
    body[ro:ro + 16] = rr; body[ro + 0x30:ro + 0x40] = rr
    return zrbf_grow(hdr + body)

def patch_assetinfo(src):
    p = Pkg(src)
    assert mm3(NEW_MD.encode()) not in p.blocks, 'asset novo ja existe (use os originais)'
    lst = [e for e in p.ent if p.name(e[0]) == PKG][0]
    blk = bytearray(p.blocks[lst[0]]); hdr = blk[:0x30]; D = bytearray(blk[0x30:0x30 + struct.unpack_from('<I', blk, 0x20)[0]])
    assert zrbf_fix(blk) == bytes(blk), 'lista de assets: cabecalho nao confere'
    ini, fim, cnt, _ = struct.unpack_from('<4I', D, 0)
    assert fim - ini == cnt * 0xC0
    recs = [bytes(D[ini + i * 0xC0:ini + (i + 1) * 0xC0]) for i in range(cnt)]
    byname = {ref(D, ini + i * 0xC0 + 0x40).decode(): i for i in range(cnt)}
    assert NEW not in byname
    def align():
        while len(D) % 16: D.append(0)
    def put_str(s):
        align(); a = len(D); D.extend(s + b'\0'); align()
        return struct.pack('<4I', a, len(D), len(s), mm3(s))
    r = bytearray(recs[byname[DONOR]])
    assert ref(D, ini + byname[DONOR] * 0xC0 + 0x10) == DONOR_MD.encode()
    r[0x10:0x20] = put_str(NEW_MD.encode()); r[0x40:0x50] = put_str(NEW.encode())
    align(); a = len(D)
    for x in recs + [bytes(r)]: D.extend(x)
    struct.pack_into('<4I', D, 0, a, len(D), cnt + 1, 0)
    p.blocks[lst[0]] = zrbf_grow(hdr + D)
    # MeshDesc proprio (o bloco nao guarda o proprio caminho)
    md = meshdesc_hidden(p.blocks[mm3(DONOR_MD.encode())]); h = mm3(NEW_MD.encode())
    p.blocks[h] = md
    don = [e for e in p.ent if e[0] == mm3(DONOR_MD.encode())][0]
    last = max(e[2] for e in p.ent if p.name(e[0]) != 'ZData::SHARED_TYPE_ENCODING')
    p.ent.append([h, don[1], last + 1, 0, 0, 0, 0, 0]); p.strs[h] = NEW_MD.encode()
    return p.build()

# ---------- ModelPackage ----------
def mesh_no_chimneys(B):
    """v3 (blueprint do usuario, 15/10): apaga o bloco principal + anexo (tudo com centro a leste de SPLIT_X: salao, galpoes,
    chamines, plataforma do guindaste) e move o predio do lado oeste DX metros para a direita, para ele ficar na borda
    ao lado da faixa por onde passa o trilho. Triangulos apagados viram degenerados (tamanhos iguais); pontos do modelo
    (pessoas, lampadas, fogo, arvores) do lado oeste andam junto, os do lado leste descem para HIDE_Z."""
    B = bytearray(B)
    secs = [struct.unpack_from('<2I', B, 0x10 + 8 * i) for i in range(8)]
    subs = [struct.unpack_from('<HHIII', B, secs[0][0] + 16 * i) for i in range(secs[0][1] // 16)]
    ni, k57, io, ib = struct.unpack_from('<4I', B, secs[3][0])
    w = 4 if ib >= ni * 4 else 2                     # PackagedData: u32; PackagedDataHD (sem o LOD0): u16
    assert ib == ((ni * w + 15) & ~15) or ib == ni * w, 'tamanho dos indices inesperado'
    ind = np.frombuffer(bytes(B[io:io + ni * w]), np.uint32 if w == 4 else np.uint16).copy()
    st = struct.unpack_from('<4I', B, secs[5][0])
    pos = np.frombuffer(bytes(B[st[2]:st[2] + st[0] * 12]), np.float32).reshape(-1, 3).copy()
    key = np.round(pos * 100).astype(np.int64); _, inv = np.unique(key, axis=0, return_inverse=True); inv = inv.ravel()
    total = 0; report = []; move = np.zeros(len(pos), bool)
    for s in subs:
        a, n = s[3], s[4]; t = ind[a:a + 3 * n].reshape(-1, 3).astype(np.int64)
        tt = inv[t]; par = np.arange(inv.max() + 1)
        def find(x):
            r = x
            while par[r] != r: r = par[r]
            while par[x] != r: par[x], x = r, par[x]
            return r
        for p0, p1, p2 in tt:
            r0 = find(p0); par[find(p1)] = r0; par[find(p2)] = r0
        lab = np.array([find(x) for x in tt[:, 0]])
        kill = np.zeros(len(t), bool)
        crates = 0
        for c in np.unique(lab):
            m = lab == c; v = np.unique(t[m]); mn, mx = pos[v].min(0), pos[v].max(0)
            if (mn[0] + mx[0]) / 2 >= SPLIT_X: kill |= m
            elif any(mn[0] >= x0 and mx[0] <= x1 and mn[1] >= y0 and mx[1] <= y1 for x0, x1, y0, y1 in BAYS) and mx[2] < 7 and mx[2] - mn[2] > 0.6:
                kill |= m; crates += int(m.sum())   # caixas dos 2 portoes centrais
        if crates: report.append('caixas %d' % crates)
        move[np.unique(t[~kill])] = True
        tri = ind[a:a + 3 * n].reshape(-1, 3)
        tri[kill] = tri[kill][:, :1]            # triangulo degenerado: os 3 indices iguais
        ind[a:a + 3 * n] = tri.ravel(); total += int(kill.sum()); report.append(int(kill.sum()))
    B[io:io + ni * w] = ind.tobytes()
    pos[move, 0] += DX
    B[st[2]:st[2] + st[0] * 12] = pos.astype(np.float32).tobytes()
    # caixa envolvente (S2): min xyz, 0, max xyz
    keep = pos[move]; struct.pack_into('<3fI3f', B, secs[2][0], *keep.min(0), 0, *keep.max(0))
    s1 = secs[1][0]; nn = struct.unpack_from('<I', B, s1 + 8)[0] >> 16
    o_nodes, o_m1, o_m2 = struct.unpack_from('<I', B, s1 + 0xc)[0], struct.unpack_from('<I', B, s1 + 0x18)[0], struct.unpack_from('<I', B, s1 + 0x1c)[0]
    assert o_m2 - o_m1 == nn * 0x40, 'pontos do modelo: layout inesperado'
    fixed = {mm3(n.encode()) for n in KEEP_NODES}; shifted = hidden = 0
    for i in range(nn):
        h = struct.unpack_from('<I', B, s1 + o_nodes + i * 0x40 + 0x28)[0]
        x, y, z = struct.unpack_from('<3f', B, s1 + o_m1 + i * 0x40 + 0x30)
        # nos na origem (as 2 malhas do predio ficam em x=0.000015, nao 0: na v3 iam para baixo da terra e o predio sumia)
        if h in fixed or h == mm3(RAIL_NODE_OLD.encode()) or (abs(x) < 0.01 and abs(y) < 0.01 and abs(z) < 0.01): continue
        if h in {mm3(n.encode()) for n in HIDE_NODES} or x >= SPLIT_X:
            for base in (o_m1, o_m2): struct.pack_into('<f', B, s1 + base + i * 0x40 + 0x38, HIDE_Z)
            hidden += 1
        else:
            for base in (o_m1, o_m2): struct.pack_into('<f', B, s1 + base + i * 0x40 + 0x30, x + DX)
            shifted += 1
    # ponto do decalque do trilho: FX_ChimneySmoke_001 renomeado (3 copias do hash no S1) e posto na origem
    ho, hn = struct.pack('<I', mm3(RAIL_NODE_OLD.encode())), struct.pack('<I', mm3(RAIL_NODE.encode()))
    s1end = s1 + secs[1][1]; occ = [i for i in range(s1, s1end - 3, 4) if B[i:i + 4] == ho]
    assert len(occ) == 3, ('copias do hash do ponto', len(occ))
    for i in occ: B[i:i + 4] = hn
    ri = [i for i in range(nn) if B[s1 + o_nodes + i * 0x40 + 0x28:s1 + o_nodes + i * 0x40 + 0x2c] == hn][0]
    for base in (o_m1, o_m2): struct.pack_into('<3f', B, s1 + base + ri * 0x40 + 0x30, 0.0, 0.0, 0.0)
    report.append('pontos: %d movidos, %d escondidos; decalque do trilho no ponto %d' % (shifted, hidden, ri))
    return bytes(B), total, report

def patch_models(src):
    d = bytearray(open(src, 'rb').read())
    ver, cnt, hs, isz, ds, dsz = struct.unpack_from('<6I', d, 0)
    assert (ver, hs, isz, ds, ds + dsz) == (3, 0x20, cnt * 0xF0, 0x20 + cnt * 0xF0, len(d))
    E = {struct.unpack_from('<I', d, 0x20 + i * 0xF0)[0]: bytes(d[0x20 + i * 0xF0:0x20 + (i + 1) * 0xF0]) for i in range(cnt)}
    kd = mm3(('%s_%s' % (PKG, DONOR)).encode()); kn = mm3(('%s_%s' % (PKG, NEW)).encode())
    assert kd in E and kn not in E
    q = list(struct.unpack('<30Q', E[kd]))
    blob, total, rep = mesh_no_chimneys(d[ds + q[2]:ds + q[2] + q[3]])
    assert len(blob) == q[3] and total > 0
    extra = bytearray()
    while (dsz + len(extra)) % 16: extra.append(0)
    q[0] = kn; q[2] = dsz + len(extra); extra += blob
    E[kn] = struct.pack('<30Q', *q)
    keys = sorted(E); n = len(keys)
    out = bytearray(struct.pack('<6I', ver, n, 0x20, n * 0xF0, 0x20 + n * 0xF0, dsz + len(extra))) + d[0x18:0x20]
    out += b''.join(E[k] for k in keys) + d[ds:] + extra
    return out, total, rep

# ---------- IndusClientData ----------
def patch_viz(src):
    p = Pkg(src)
    e = [x for x in p.ent if p.name(x[0]) == VIZ_BLOCK][0]
    b = bytearray(p.blocks[e[0]]); assert zrbf_fix(b) == bytes(b), 'bloco de visuais: cabecalho nao confere'
    D = memoryview(b)[0x30:]
    hit = None
    for o in range(0, 0x100, 4):
        a, en, n, z = struct.unpack_from('<4I', D, o)
        if n > 100 and en - a == n * 0x80: hit = (a, n); break
    assert hit, 'lista de visuais nao achada'
    a, n = hit; slot = donor = None
    for i in range(n):
        h = struct.unpack_from('<I', D, a + i * 0x80)[0]
        if h == mm3(VIZ_SLOT.encode()): assert slot is None; slot = a + i * 0x80
        if h == mm3(VIZ_DONOR.encode()): assert donor is None; donor = a + i * 0x80
    assert slot is not None and donor is not None
    D[slot + 4:slot + 0x80] = bytes(D[donor + 4:donor + 0x80])
    struct.pack_into('<I', D, slot + 4, mm3(NEW.encode()))
    del D
    p.blocks[e[0]] = zrbf_fix(b)
    return p.build()

# ---------- decalque: texturas e CSV ----------
def patch_texpkg(src):
    d = open(src, 'rb').read()
    ver, cnt, hs, isz, ds, dsz = struct.unpack_from('<6I', d, 0)
    assert (ver, hs, isz, ds, ds + dsz) == (2, 0x20, cnt * 0x30, 0x20 + cnt * 0x30, len(d)), 'pacote de texturas inesperado'
    E = {struct.unpack_from('<Q', d, 0x20 + i * 0x30)[0]: list(struct.unpack_from('<6Q', d, 0x20 + i * 0x30)) for i in range(cnt)}
    new = [(mm3(p.encode()), open(f, 'rb').read()) for p, f in DECAL_TEX.items()]
    for k, _ in new: assert k not in E, 'textura do decalque ja existe (use os originais)'
    n2 = cnt + len(new); ds2 = 0x20 + n2 * 0x30; sh = ds2 - ds
    for e in E.values(): e[2] += sh
    tail = bytearray(); pos = len(d) + sh
    for k, raw in new:
        E[k] = [k, 0, pos + len(tail), len(raw), 0, 0]; tail += raw
        while len(tail) % 4: tail += bytes(1)
    out = bytearray(struct.pack('<6I', ver, n2, 0x20, n2 * 0x30, ds2, len(d) + sh + len(tail) - ds2)) + d[0x18:0x20]
    for k in sorted(E): out += struct.pack('<6Q', *E[k])
    assert len(out) == ds2
    return bytes(out) + d[ds:] + bytes(tail)

def patch_csv(src):
    t = open(src, 'rb').read()
    assert DECAL.encode() + b',' not in t, 'linha do decalque ja existe (use os originais)'
    if not t.endswith(b'\r\n'): t += b'\r\n'
    return t + DECAL_ROW.encode() + b'\r\n'

def md5(path): return hashlib.md5(open(path, 'rb').read()).hexdigest()

def main(cmd):
    os.makedirs(BACKUP, exist_ok=True)
    man = os.path.join(BACKUP, 'instalado.json')                     # md5 do que este instalador gravou no jogo
    inst = json.load(open(man)) if os.path.exists(man) else {}
    for sub, f in FILES:
        g = os.path.join(GAME, sub, f); bk = os.path.join(BACKUP, sub, f); key = sub + '/' + f
        os.makedirs(os.path.dirname(bk), exist_ok=True)
        # sem backup, ou o arquivo do jogo nao e o que instalamos (Steam atualizou/verificou): o atual vira o original
        if not os.path.exists(bk) or (inst.get(key) and md5(g) != inst[key] and md5(g) != md5(bk)):
            shutil.copy2(g, bk); print('backup:', sub, f)
    if cmd == 'restaurar':
        for sub, f in FILES:
            shutil.copy2(os.path.join(BACKUP, sub, f), os.path.join(GAME, sub, f)); print('restaurado:', sub, f)
        if os.path.exists(man): os.remove(man)
        return
    out = {}
    for sub in ('PackagedData', 'PackagedDataHD'):
        out[(sub, PKG + '.OxAssetInfoPackage')] = patch_assetinfo(os.path.join(BACKUP, sub, PKG + '.OxAssetInfoPackage'))
        m, total, rep = patch_models(os.path.join(BACKUP, sub, PKG + '.OxModelPackage'))
        out[(sub, PKG + '.OxModelPackage')] = m
        print('%s: malha da estacao com %d triangulos de chamine removidos %s' % (sub, total, rep))
    out[('PackagedData', 'ZData_IndusClientData.OxZDataLibPackage')] = patch_viz(os.path.join(BACKUP, 'PackagedData', 'ZData_IndusClientData.OxZDataLibPackage'))
    for sub in ('PackagedData', 'PackagedDataHD'):
        out[(sub, DECAL_PKG)] = patch_texpkg(os.path.join(BACKUP, sub, DECAL_PKG))
    out[('.', CSV_REL)] = patch_csv(os.path.join(BACKUP, '.', CSV_REL))
    # confere que os pacotes novos ainda abrem com o leitor antes de gravar
    for (sub, f), data in out.items():
        tmp = os.path.join(BACKUP, '_teste.bin'); open(tmp, 'wb').write(data)
        if f.endswith('Package') and 'Model' not in f and 'Texture' not in f: Pkg(tmp)
        os.remove(tmp)
    for (sub, f), data in out.items():
        g = os.path.join(GAME, sub, f); open(g, 'wb').write(data); print('instalado: %s\\%s (%d bytes)' % (sub, f, len(data)))
        inst[sub + '/' + f] = hashlib.md5(data).hexdigest()
    json.dump(inst, open(man, 'w'), indent=1)

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'instalar')
