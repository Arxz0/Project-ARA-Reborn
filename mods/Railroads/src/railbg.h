// --- 1.7: decisao trilho/estrada calculada fora do desenho ----------------------------------------------------------
// Ate a 1.6 o desenho de cada trecho decidia na hora (cache de 1 s por numero de ligacao) com uma tabela de pontas
// guardada quando cada trecho era desenhado pela primeira vez e nunca refeita. Na passagem de turno o jogo refaz a rede
// (os numeros das ligacoes mudam e o vetor de trechos muda de lugar): a tabela velha deixava de casar e as ruas do centro
// das regioes voltavam a ser desenhadas como estrada ate o turno recomecar ("pop-in" de pedacos de estrada soltos).
// Agora uma thread de fundo le o vetor INTEIRO de trechos a cada 0,5 s (so ReadProcessMemory, nunca chama funcao do jogo;
// a busca de handle do jogo, 0x2c9250, reorganiza o mapa e nao pode rodar fora da thread dele), aplica a mesma regra da
// 1.4 com dados da mesma leitura e publica um conjunto de trechos de trilho identificados pela GEOMETRIA (pontas, tipo,
// quantidade de pontos), nao pelo numero da ligacao. O desenho so procura o trecho nesse conjunto.
//  * Leitura inconsistente (vetor/contagens mudaram no meio) e descartada; o conjunto anterior continua valendo.
//  * Trecho que era trilho e deixa de ser continua trilho por TR_HOLD_MS (perda real de ferrovia e rara; os estados
//    intermediarios da passagem de turno nao).
#define RVA_SPL_BASE      0x23e0e0d0ull // vetor de trechos (0x50 B) do WorldView (0x5c4608)
#define RVA_SPL_COUNT     0x23e0e0c8ull // quantidade (0x5c6d05)
#define TR_PERIOD_MS 500
#define TR_HOLD_MS   60000u
#define SG_MAX 8192
#define SG_TOL2 (2.0f * 2.0f)                             // distancia maxima (ao quadrado) entre pontas iguais
#define TR_TAB_BITS 16
#define TR_TAB (1u << TR_TAB_BITS)

static int rpm(unsigned long long a, void *dst, size_t n) {
    SIZE_T got = 0;
    return a && ReadProcessMemory(GetCurrentProcess(), (void *)a, dst, n, &got) && got == n;
}
static int wpm(unsigned long long a, const void *src, size_t n) {
    SIZE_T got = 0;
    return WriteProcessMemory(GetCurrentProcess(), (void *)a, src, n, &got) && got == n;
}
static int rpm_big(unsigned long long a, unsigned char *dst, size_t n) {   // em pedacos de ate 8 MB
    for (size_t o = 0; o < n; o += (size_t)8 << 20) if (!rpm(a + o, dst + o, (n - o < ((size_t)8 << 20)) ? n - o : ((size_t)8 << 20))) return 0;
    return 1;
}
static int mod_orig_ends_c(unsigned c, unsigned n, float *p);   // desvio.h: pontas originais de trecho desviado

// ---- conjunto publicado (chave = geometria) ----
typedef struct { unsigned long long k; unsigned tick, pad; } tr_ent;
static tr_ent g_trtab[3][TR_TAB];
static tr_ent *volatile g_trpub;                           // NULL = nenhuma leitura ainda
static int g_trcur;
static unsigned long long tr_key(const float *e, unsigned t, unsigned n) {   // e = x0, y0, x1, y1
    long long a[4];
    for (int i = 0; i < 4; i++) a[i] = llrintf(e[i] * 8.0f);
    if (a[2] < a[0] || (a[2] == a[0] && a[3] < a[1])) { long long x = a[0], y = a[1]; a[0] = a[2]; a[1] = a[3]; a[2] = x; a[3] = y; }
    unsigned long long h = 0x9e3779b97f4a7c15ull ^ ((unsigned long long)t << 32 | n);
    for (int i = 0; i < 4; i++) { h ^= (unsigned long long)a[i] + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); h *= 0xbf58476d1ce4e5b9ull; h ^= h >> 31; }
    return h ? h : 1;
}
static int tr_find(const tr_ent *T, unsigned long long k) {
    for (unsigned i = (unsigned)k & (TR_TAB - 1), m = 0; m < TR_TAB; i = (i + 1) & (TR_TAB - 1), m++) {
        if (T[i].k == k) return 1;
        if (!T[i].k) return 0;
    }
    return 0;
}
static int tr_put(tr_ent *T, unsigned *cnt, unsigned long long k, unsigned tick) {
    if (*cnt >= TR_TAB / 2) return 0;
    for (unsigned i = (unsigned)k & (TR_TAB - 1);; i = (i + 1) & (TR_TAB - 1)) {
        if (T[i].k == k) { if ((int)(tick - T[i].tick) > 0) T[i].tick = tick; return 1; }
        if (!T[i].k) { T[i].k = k; T[i].tick = tick; (*cnt)++; return 1; }
    }
}
static int tr_lookup(const float *e, unsigned t, unsigned n) {
    const tr_ent *T = g_trpub;
    return T && tr_find(T, tr_key(e, t, n));
}

// ---- regioes com ferrovia (so leitura) ----
typedef struct { unsigned long long rr, pool; int *pairs; unsigned npairs, cap; } rg_ctx;
static rg_ctx g_rg;
static signed char g_rgv[1 << 16]; static unsigned g_rgst[1 << 16], g_rgstamp = 1;
static int rg_load(unsigned char *g) {
    unsigned char m[0x20]; unsigned long long rr, pool;
    if (!rpm((unsigned long long)(g + GC_ROAD_MAP), m, sizeof m) || !rpm((unsigned long long)(g + GC_REGION_ROADS), &rr, 8) ||
        !rpm((unsigned long long)(g + GC_ROAD_POOL), &pool, 8)) return 0;
    unsigned lin = *(unsigned *)m, sorted = *(unsigned *)(m + 8), n = lin + sorted;
    unsigned long long arr = *(unsigned long long *)(m + 0x18);
    if (n > (1u << 20) || !rr || !pool || (n && !arr)) return 0;
    if (n > g_rg.cap) { free(g_rg.pairs); g_rg.cap = n + 256; g_rg.pairs = (int *)malloc((size_t)g_rg.cap * 8); if (!g_rg.pairs) { g_rg.cap = 0; return 0; } }
    if (n && !rpm(arr, g_rg.pairs, (size_t)n * 8)) return 0;
    g_rg.rr = rr; g_rg.pool = pool; g_rg.npairs = n;
    if (++g_rgstamp == 0) { memset(g_rgst, 0, sizeof g_rgst); g_rgstamp = 1; }
    return 1;
}
static int rg_rail_raw(unsigned r) {                       // 1 sim, 0 nao, -1 nao deu para ler
    unsigned char e[0xc]; if (!rpm(g_rg.rr + (unsigned long long)r * 0xc, e, sizeof e)) return -1;
    int h = *(int *)(e + 8);
    if (h == -1 || e[0] != 1) return 0;
    for (unsigned k = 0; k < g_rg.npairs; k++) if (g_rg.pairs[k * 2] == h) {
        unsigned tpl; if (!rpm(g_rg.pool + (unsigned long long)(unsigned)g_rg.pairs[k * 2 + 1] * 0x30 + 0x10, &tpl, 4)) return -1;
        return tpl == RAIL_ID;
    }
    return 0;
}
static int rg_rail(unsigned r) {                           // com cache por leitura
    if (r >= (1u << 16)) return rg_rail_raw(r);
    if (g_rgst[r] != g_rgstamp) { g_rgv[r] = (signed char)rg_rail_raw(r); g_rgst[r] = g_rgstamp; }
    return g_rgv[r];
}

// ---- leitura e regra ----
typedef struct { unsigned c, ra, rb, t, n; float p[4], q[4]; } seg;   // p = pontas originais (regra), q = pontas atuais (chave)
static seg g_sg[SG_MAX]; static LONG g_sg_n;
static volatile int g_rail_intra;
static volatile LONG g_rc_calls, g_tr_inter, g_tr_intra, g_tr_held, g_tr_segs, g_tr_gaps;
static unsigned char *g_trarr, *g_trconn; static size_t g_trarr_cap, g_trconn_cap;
static int touch(const float *a, const float *b) {          // alguma ponta de a encosta em alguma ponta de b
    for (int i = 0; i < 4; i += 2) for (int k = 0; k < 4; k += 2) {
        float dx = a[i] - b[k], dy = a[i + 1] - b[k + 1];
        if (dx * dx + dy * dy <= SG_TOL2) return 1;
    }
    return 0;
}
static int uf_find(int *P, int x) { while (P[x] != x) { P[x] = P[P[x]]; x = P[x]; } return x; }
typedef struct { unsigned r; int i; } rg_ix;
static int rgix_cmp(const void *a, const void *b) { unsigned x = ((const rg_ix *)a)->r, y = ((const rg_ix *)b)->r; return x < y ? -1 : x > y; }

static unsigned g_tr_logs, g_tr_hdr_logs;
static void rail_snapshot(void) {
    unsigned char *g = *(unsigned char **)(g_base + RVA_GC_PTR);
    if (!g || !g_rail_data) return;
    unsigned ns = *(unsigned *)(g_base + RVA_SPL_COUNT); unsigned long long sb = *(unsigned long long *)(g_base + RVA_SPL_BASE);
    unsigned char ch[0x10]; if (!rpm((unsigned long long)(g + GC_CONN_COUNT), ch, sizeof ch)) return;
    unsigned nconn = *(unsigned *)ch; unsigned long long cb = *(unsigned long long *)(ch + 8);
    if (!sb || !ns || ns > 2000000 || !cb || !nconn || nconn > 4000000) return;
    static unsigned long long l_sb; static unsigned l_ns, l_nconn; static unsigned char *l_g;
    if ((l_sb && (sb != l_sb || ns != l_ns || nconn != l_nconn || g != l_g)) && g_tr_hdr_logs < 40) {
        g_tr_hdr_logs++;
        qlog("Ferrovia: o jogo refez a rede (trechos %u -> %u, vetor %s, ligacoes %u -> %u%s).", l_ns, ns, sb != l_sb ? "mudou de lugar" : "no mesmo lugar", l_nconn, nconn, g != l_g ? ", GameCore trocou" : "");
    }
    l_sb = sb; l_ns = ns; l_nconn = nconn; l_g = g;
    if (!rg_load(g)) return;
    size_t an = (size_t)ns * 0x50, cn = (size_t)nconn * 0x18;
    if (an > g_trarr_cap) { free(g_trarr); g_trarr_cap = an + 0x40000; g_trarr = (unsigned char *)malloc(g_trarr_cap); if (!g_trarr) { g_trarr_cap = 0; return; } }
    if (cn > g_trconn_cap) { free(g_trconn); g_trconn_cap = cn + 0x40000; g_trconn = (unsigned char *)malloc(g_trconn_cap); if (!g_trconn) { g_trconn_cap = 0; return; } }
    if (!rpm_big(cb, g_trconn, cn) || !rpm_big(sb, g_trarr, an)) return;
    LONG n = 0;
    for (unsigned i = 0; i < ns && n < SG_MAX; i++) {
        unsigned char *r = g_trarr + (size_t)i * 0x50;
        unsigned t = *(unsigned *)(r + SPL_TYPE) & 0x3fff, c = *(unsigned *)(r + SPL_CONN), np = *(unsigned *)(r + SPL_NPTS);
        unsigned long long pp = *(unsigned long long *)(r + SPL_PTS);
        if (t < 4 || t > 10 || c >= nconn || np < 2 || np > 1000000 || !pp) continue;
        unsigned ra = *(unsigned *)(g_trconn + (size_t)c * 0x18 + 0xc), rb = *(unsigned *)(g_trconn + (size_t)c * 0x18 + 0x10);
        int ha = rg_rail(ra), hb = ra == rb ? ha : rg_rail(rb);
        if (ha != 1 && hb != 1) continue;
        float a[4], b[4];
        if (!rpm(pp, a, 16) || !rpm(pp + (unsigned long long)(np - 1) * 16, b, 16)) continue;
        seg *s = &g_sg[n++]; s->c = c; s->ra = ra; s->rb = rb; s->t = t; s->n = np;
        s->q[0] = a[0]; s->q[1] = a[1]; s->q[2] = b[0]; s->q[3] = b[1];
        if (!mod_orig_ends_c(c, np, s->p)) memcpy(s->p, s->q, sizeof s->p);
    }
    // leitura consistente? (o jogo nao refez a rede no meio)
    unsigned char ch2[0x10];
    if (*(unsigned *)(g_base + RVA_SPL_COUNT) != ns || *(unsigned long long *)(g_base + RVA_SPL_BASE) != sb || *(unsigned char **)(g_base + RVA_GC_PTR) != g ||
        !rpm((unsigned long long)(g + GC_CONN_COUNT), ch2, sizeof ch2) || memcmp(ch, ch2, sizeof ch)) return;
    g_sg_n = n;
    // regra 1.4: inter = trilho se as duas regioes tem ferrovia; interna: encosta em divisa com ferrovia, ou corrente
    // de trechos tipo 8 da regiao (ponta com ponta) que encosta em divisa com ferrovia; encostar em divisa sem ferrovia = estrada
    signed char *rt = (signed char *)calloc(n ? n : 1, 1), *rd = (signed char *)calloc(n ? n : 1, 1);
    int *P = (int *)malloc(sizeof(int) * (n ? n : 1)), *cnt = (int *)calloc(n ? n : 1, sizeof(int));
    rg_ix *ix = (rg_ix *)malloc(sizeof(rg_ix) * (n ? 2 * n : 1));
    if (!rt || !rd || !P || !cnt || !ix) { free(rt); free(rd); free(P); free(cnt); free(ix); return; }
    int ni = 0;
    for (LONG i = 0; i < n; i++) {
        P[i] = i;
        ix[ni].r = g_sg[i].ra; ix[ni++].i = i;
        if (g_sg[i].rb != g_sg[i].ra) { ix[ni].r = g_sg[i].rb; ix[ni++].i = i; }
    }
    qsort(ix, ni, sizeof *ix, rgix_cmp);
    for (int a0 = 0; a0 < ni;) {
        int a1 = a0; while (a1 < ni && ix[a1].r == ix[a0].r) a1++;
        unsigned r = ix[a0].r;
        for (int x = a0; x < a1; x++) {
            seg *s = &g_sg[ix[x].i]; if (s->ra != s->rb) continue;
            for (int y = a0; y < a1; y++) {
                if (x == y) continue;
                seg *e = &g_sg[ix[y].i];
                if (!touch(s->p, e->p)) continue;
                if (e->ra != e->rb) { int o = rg_rail(e->ra == r ? e->rb : e->ra); if (o == 1) rt[ix[x].i] = 1; else if (o == 0) rd[ix[x].i] = 1; }
                else if (s->t == 8 && e->t == 8) { int u = uf_find(P, ix[x].i), v = uf_find(P, ix[y].i); if (u != v) P[u] = v; }
            }
        }
        a0 = a1;
    }
    for (LONG i = 0; i < n; i++) if (rt[i]) cnt[uf_find(P, i)]++;
    signed char *R = (signed char *)calloc(n ? n : 1, 1);
    if (!R) { free(rt); free(rd); free(P); free(cnt); free(ix); return; }
    for (LONG i = 0; i < n; i++) {
        seg *s = &g_sg[i];
        if (s->ra != s->rb) R[i] = rg_rail(s->ra) == 1 && rg_rail(s->rb) == 1;
        else if (g_rail_intra) R[i] = 1;
        else if (rt[i]) R[i] = 1;
        else if (rd[i] || s->t != 8) R[i] = 0;
        else R[i] = cnt[uf_find(P, i)] > 0;
    }
    // 1.8: buraco na linha = rua interna (ex. tipo 4 no cruzamento de uma vila) cujas DUAS pontas encostam em pedacos de
    // trilho da mesma regiao que so se ligam por ela: vira trilho (antes ficava um pedaco de asfalto no meio da ferrovia).
    // Rua que so fecha um laco entre trilhos ja ligados continua rua.
    LONG gaps = 0;
    int *U = (int *)malloc(sizeof(int) * (n ? n : 1));
    for (int a0 = 0; U && a0 < ni;) {
        int a1 = a0; while (a1 < ni && ix[a1].r == ix[a0].r) a1++;
        for (int x = a0; x < a1; x++) U[ix[x].i] = ix[x].i;
        for (int x = a0; x < a1; x++) for (int y = x + 1; y < a1; y++) {           // componentes de trilho nesta regiao
            int i = ix[x].i, j = ix[y].i;
            if (R[i] && R[j] && touch(g_sg[i].p, g_sg[j].p)) { int u2 = uf_find(U, i), v2_ = uf_find(U, j); if (u2 != v2_) U[u2] = v2_; }
        }
        for (int x = a0; x < a1; x++) {
            int i = ix[x].i; seg *s = &g_sg[i];
            if (R[i] || s->ra != s->rb || rd[i] || rg_rail(s->ra) != 1) continue;
            int c0 = -1, c1 = -1;
            for (int y = a0; y < a1; y++) {
                int j = ix[y].i; if (j == i || !R[j]) continue;
                const float *q = g_sg[j].p;
                for (int k2 = 0; k2 < 4; k2 += 2) {
                    float d0x = s->p[0] - q[k2], d0y = s->p[1] - q[k2 + 1], d1x = s->p[2] - q[k2], d1y = s->p[3] - q[k2 + 1];
                    if (d0x * d0x + d0y * d0y <= SG_TOL2) c0 = uf_find(U, j);
                    if (d1x * d1x + d1y * d1y <= SG_TOL2) c1 = uf_find(U, j);
                }
            }
            if (c0 >= 0 && c1 >= 0 && c0 != c1) { R[i] = 2; gaps++; U[c0] = c1; }
        }
        a0 = a1;
    }
    free(U);
    // publica: trilho agora + trilho ha menos de TR_HOLD_MS
    unsigned now = GetTickCount(), tc = 0; LONG ninter = 0, nintra = 0, held = 0;
    int nx = (g_trcur + 1) % 3; tr_ent *T = g_trtab[nx]; memset(T, 0, sizeof g_trtab[0]);
    const tr_ent *old = g_trpub;
    for (LONG i = 0; i < n; i++) {
        seg *s = &g_sg[i]; int rail = R[i] != 0;
        unsigned long long kq = tr_key(s->q, s->t, s->n), kp = tr_key(s->p, s->t, s->n);
        if (rail) {
            tr_put(T, &tc, kq, now); if (kp != kq) tr_put(T, &tc, kp, now);
            if (s->ra != s->rb) ninter++; else nintra++;
        } else if (old && tr_find(old, kq)) {
            held++;
            if (g_tr_logs < 30) { g_tr_logs++; qlog("Ferrovia: trecho (%.0f,%.0f)-(%.0f,%.0f) tipo %u, regioes %u-%u, deixou de ser trilho nesta leitura (divisa com ferrovia %d, sem ferrovia %d); fica trilho por ate %u s.",
                                                    s->q[0], s->q[1], s->q[2], s->q[3], s->t, s->ra, s->rb, rt[i], rd[i], TR_HOLD_MS / 1000); }
        }
    }
    if (old) for (unsigned i = 0; i < TR_TAB; i++) if (old[i].k && now - old[i].tick <= TR_HOLD_MS) tr_put(T, &tc, old[i].k, old[i].tick);
    g_trcur = nx; g_trpub = T;
    g_tr_inter = ninter; g_tr_intra = nintra; g_tr_held = held; g_tr_segs = n; g_tr_gaps = gaps;
    free(rt); free(rd); free(P); free(cnt); free(ix); free(R);
}

// ---- no desenho: so procura o trecho no conjunto ----
static unsigned g_rc_stat_tick, g_rc_stat_logs;
static int spline_rail(unsigned char *spl) {          // 1 = desenhar este trecho como trilho
    InterlockedIncrement(&g_rc_calls);
    unsigned now = GetTickCount();
    { unsigned st = g_rc_stat_tick;                       // diagnostico: quantas vezes por segundo o jogo chama isto
      if (now - st >= 60000 && InterlockedCompareExchange((volatile LONG *)&g_rc_stat_tick, (LONG)now, (LONG)st) == (LONG)st) {
          LONG c = InterlockedExchange(&g_rc_calls, 0);
          if (st && g_rc_stat_logs < 10) { g_rc_stat_logs++; qlog("Ferrovia: desenho de trechos chamado %ld vezes/s (media de 1 min); trechos de trilho: %ld entre regioes, %ld internos (%ld buracos de rua preenchidos; %s); %ld trechos em regioes com ferrovia.",
                                                                  c / 60, g_tr_inter, g_tr_intra, g_tr_gaps, g_rail_intra ? "todas as internas viram trilho" : "so internas ligadas ao trilho", g_tr_segs); }
      } }
    if (!g_rail_data) {                                   // dados do mod ainda nao conferidos
        static unsigned last;
        if (now - last < 2000) return 0;
        last = now; void *d = NULL;
        g_rail_data = ((data_get_fn)(g_base + RVA_DATA_GET))(&d, H_ROADTPL_TYPE, H_ROADTPL_GROUP, RAIL_ID) && d;
        if (!g_rail_data) return 0;
    }
    unsigned t = *(unsigned *)(spl + SPL_TYPE) & 0x3fff, n = *(unsigned *)(spl + SPL_NPTS);
    float *pt = *(float **)(spl + SPL_PTS);
    if (t < 4 || t > 10 || n < 2 || n > 1000000 || !pt || !g_trpub) return 0;
    float e[4] = { pt[0], pt[1], pt[(size_t)(n - 1) * 4], pt[(size_t)(n - 1) * 4 + 1] };
    return tr_lookup(e, t, n);
}
static void rail_read_ini(void) {                         // relido a cada 2 s pela thread de fundo
    static int last = -1; char v[8];
    GetPrivateProfileStringA("Desenho", "TrilhoDentroDaRegiao", "", v, sizeof v, g_inipath);
    if (!v[0]) { WritePrivateProfileStringA("Desenho", "TrilhoDentroDaRegiao", "0", g_inipath); strcpy(v, "0"); }
    int x = atoi(v) == 1;
    if (x != last) { g_rail_intra = x; qlog("Ferrovia: ruas internas das regioes com ferrovia %s.", x ? "viram trilho (como na 1.1)" : "so viram trilho no caminho ate a regiao vizinha com ferrovia (1.4: corrente inteira)"); last = x; }
}
