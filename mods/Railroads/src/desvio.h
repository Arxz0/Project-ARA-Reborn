// --- 1.6: desvio da linha principal ate a Train Station -------------------------------------------------------------
// A estacao (asset GEN_MAC_TrainStation, estacao.py) tem o trilho pintado num decalque na faixa dos portoes: x = DV_XS no
// espaco do modelo, ao longo do eixo Y local. Esta parte faz a linha principal passar por essa faixa: pega a corrente de
// trechos de trilho mais proxima da estacao (ate ~230 de cada lado, parando em juncao com outra estrada) e redistribui os
// pontos desses trechos por uma rota nova: ponta da corrente -> faixa dos portoes -> outra ponta, contornando o predio.
// As pontas da corrente nao mudam; os trechos e a quantidade de pontos sao os do proprio jogo (so x/y/z mudam).
//  * Estacoes: o jogo nao guarda a rotacao junto da posicao. Uma thread em prioridade baixa varre a memoria do proprio
//    processo (ReadProcessMemory em pedacos de 1 MB) a cada DV_SCAN_S segundos atras do registro visual da estacao
//    {hash Improvement_PostOffice, +0x18 hash imp_TrainStation, +0x24 -1, +0x28 1.0, +0x30 x, y} e, para estacao nova, da
//    matriz da instancia (linhas X,Y,Z + posicao igual), de onde sai cos/sen. So varre se ha ferrovia no mapa.
//  * Leituras da memoria do jogo nesta thread: ReadProcessMemory (nunca derruba o jogo se algo for liberado no meio).
//  * Escrita: so se os pontos atuais ainda forem os originais (ou o nosso alvo); se o jogo refizer o trecho, os novos
//    pontos viram os originais e o desvio e recalculado. Sem estacao/trilho, os pontos originais voltam.
//  * spl_ends (regra das pontas do trilho) usa as pontas ORIGINAIS dos trechos desviados.
// 1.7: os trechos desviados sao guardados pelo numero da LIGACAO (+0x44), nao pelo endereco do registro. Na 1.6, quando o
//    jogo criava trechos novos o vetor de trechos mudava de lugar, a DLL perdia o controle dos trechos ja desviados (que
//    ficavam soltos no mapa, sem voltar) e desviava de novo por cima deles: trilhos a mais em volta das estacoes.
//    "Ainda e o nosso desenho" compara so x/y (tolerancia 0,01). A altura do desvio vem das pontas da corrente (a da
//    matriz da estacao nao e a altura do terreno).
// 1.9: a ESTACAO se alinha a linha (em vez de a linha ir ate a estacao): acha perto dela um trecho reto de 150 un. da
//    linha (ou endireita o mais proximo), e o gancho do desenho (alinha.h) gira e desloca so o VISUAL da estacao para a
//    faixa dos portoes ficar sobre esse trecho, com o predio do lado mais perto de onde o jogo o pos. Funciona com a linha
//    vindo de qualquer direcao, em qualquer mapa; a linha nunca faz laco e nunca cruza o predio.
// 1.8: (a) na passagem de turno o jogo troca o GameCore; a 1.7 apagava o controle dos trechos desviados SEM devolver os
//    pontos originais e desviava de novo por cima (lacos e grampos que ficavam no mapa). O controle agora sobrevive a
//    troca (pela ligacao, que nao muda). (b) rota nova sem lacos (rota.h).
// Ferrovias.ini [Estacao] DesvioDaLinha=0 desliga (relido a cada passada).
#define H_VIZ_STATION     0x19316944u   // mm3("Improvement_PostOffice"): visual usado pela estacao (estacao.py)
#define H_IMP_STATION     0xbca6a09eu   // mm3("imp_TrainStation")
#define DV_XS             48.0f         // centro do trilho na faixa dos portoes (espaco do modelo)
#define DV_YS             60.0f         // meia-faixa coberta pela fundacao
#define DV_BX0 (-6.0f)
#define DV_BX1 42.0f
#define DV_BY0 (-66.0f)
#define DV_BY1 66.0f                    // retangulo do predio (o desvio nao cruza)
#define DV_LSIDE          230.0f        // quanto da corrente (de cada lado do ponto mais proximo) pode ser redesenhado
#define DV_DMAX           320.0f        // estacao mais longe que isso do trilho: sem desvio
#define DV_SCAN_S         40
#define DV_MAXST          32
#define DV_MAXMOD         256
#define DV_MAXCH          16
#define DV_MAXPTS         512
#define DV_MOVE_MAX       180.0f        // 1.9: alinhar nao move o predio mais que isso
typedef struct { float ox, oy, oc, os, tx, ty, tc, ts; volatile LONG on; } dv_al;   // estacao: pos/rot do jogo -> alvo
static dv_al g_al[32]; static volatile LONG g_al_n;

typedef struct { float x, y, c, s, z; int rot; } dv_station;
typedef struct { unsigned c, n; unsigned char *spl; unsigned long long pts; float *orig, *tgt; int live, seen; } dv_mod;
static dv_station g_dvst[DV_MAXST]; static volatile LONG g_dvst_n;
static dv_mod g_dvmod[DV_MAXMOD]; static volatile LONG g_dvmod_n;
static CRITICAL_SECTION g_dvcs;
static char logged[DV_MAXST];                               // por estacao: 1 = motivo de nao desviar ja logado, 2 = desvio logado

// pontas originais de um trecho desviado (chamado pela leitura da regra do trilho)
static int mod_orig_ends_c(unsigned c, unsigned np, float *p) {
    LONG n = g_dvmod_n; if (n <= 0) return 0;
    int ok = 0;
    EnterCriticalSection(&g_dvcs);
    for (LONG i = 0; i < g_dvmod_n; i++) {
        dv_mod *m = &g_dvmod[i];
        if (m->c == c && m->n == np && m->orig) {
            p[0] = m->orig[0]; p[1] = m->orig[1]; p[2] = m->orig[(m->n - 1) * 4]; p[3] = m->orig[(m->n - 1) * 4 + 1]; ok = 1; break;
        }
    }
    LeaveCriticalSection(&g_dvcs);
    return ok;
}

// ---- varredura das estacoes ----
#define DV_CHUNK (1u << 20)
static unsigned char *g_dvbuf;
typedef struct { float x, y; } dv_xy;
static int dv_scan(int pass, dv_xy *pos, int *npos, dv_station *pend, int npend) {
    if (!g_dvbuf) g_dvbuf = (unsigned char *)VirtualAlloc(NULL, DV_CHUNK + 128, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_dvbuf) return 0;
    MEMORY_BASIC_INFORMATION mbi; unsigned long long a = 0x10000; int found = 0; unsigned chunks = 0;
    while (a < 0x7ffffffe0000ull && VirtualQuery((void *)a, &mbi, sizeof mbi)) {
        unsigned long long base = (unsigned long long)mbi.BaseAddress, end = base + mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && mbi.Protect == PAGE_READWRITE &&
            !(base <= (unsigned long long)g_dvbuf && (unsigned long long)g_dvbuf < end)) {
            for (unsigned long long p = base; p < end; p += DV_CHUNK) {
                size_t n = (size_t)((end - p < DV_CHUNK) ? end - p : DV_CHUNK);
                size_t rn = n + ((p + n + 64 <= end) ? 64 : 0);
                SIZE_T got = 0;
                if (!ReadProcessMemory(GetCurrentProcess(), (void *)p, g_dvbuf, rn, &got) || got < 0x40) continue;
                const unsigned *w = (const unsigned *)g_dvbuf; size_t nw = got / 4;
                if (pass == 0) {
                    for (size_t i = 0; i + 14 <= nw && i < n / 4; i++) {
                        if (w[i] != H_VIZ_STATION || w[i + 6] != H_IMP_STATION || w[i + 9] != 0xffffffffu || w[i + 10] != 0x3f800000u) continue;
                        float x = *(const float *)&w[i + 12], y = *(const float *)&w[i + 13];
                        if (!(x == x) || !(y == y) || fabsf(x) > 1e6f || fabsf(y) > 1e6f) continue;
                        int dup = 0;
                        for (int k = 0; k < *npos; k++) if (fabsf(pos[k].x - x) < 0.01f && fabsf(pos[k].y - y) < 0.01f) { dup = 1; break; }
                        if (!dup && *npos < DV_MAXST) { pos[*npos].x = x; pos[*npos].y = y; (*npos)++; found++; }
                    }
                } else {
                    for (size_t i = 12; i + 4 <= nw && i < n / 4 + 12; i++) {
                        for (int k = 0; k < npend; k++) {
                            dv_station *s = &pend[k];
                            unsigned bx, by; memcpy(&bx, &s->x, 4); memcpy(&by, &s->y, 4);
                            if (s->rot || w[i] != bx || w[i + 1] != by) continue;
                            const float *f = (const float *)&w[i - 12];       // matriz: X(0..3) Y(4..7) Z(8..11) pos(12..15)
                            float c = f[0], sn = f[1];
                            if (fabsf(c * c + sn * sn - 1) > 2e-3f || f[2] != 0 || f[3] != 0 || fabsf(f[4] + sn) > 2e-3f || fabsf(f[5] - c) > 2e-3f ||
                                f[6] != 0 || f[7] != 0 || f[8] != 0 || f[9] != 0 || f[10] < 0.5f || f[10] > 2.0f || f[11] != 0 || f[15] != 1.0f) continue;
                            s->c = c; s->s = sn; s->z = f[14]; s->rot = 1; found++;
                        }
                    }
                }
                if (++chunks % 64 == 0) Sleep(1);
            }
        }
        a = end;
    }
    return found;
}

#include "rota.h"

// ---- passada: trechos de trilho, correntes, rotas, escrita ----
typedef struct { unsigned char *spl; unsigned long long pts; unsigned n, t, ra, rb, rail, c; float *p; } dv_spl;   // p = pontos (originais)
static int same_pt(const float *a, const float *b) { return fabsf(a[0] - b[0]) < 0.5f && fabsf(a[1] - b[1]) < 0.5f; }

static dv_mod *mod_find(unsigned c) { for (LONG i = 0; i < g_dvmod_n; i++) if (g_dvmod[i].c == c) return &g_dvmod[i]; return NULL; }
static int xy_eq(const float *a, const float *b, unsigned n) {   // mesmos x/y em todos os pontos
    for (unsigned i = 0; i < n; i++) if (fabsf(a[i * 4] - b[i * 4]) > 0.01f || fabsf(a[i * 4 + 1] - b[i * 4 + 1]) > 0.01f) return 0;
    return 1;
}
static void mod_drop(LONG i) {
    free(g_dvmod[i].orig); free(g_dvmod[i].tgt);
    g_dvmod[i] = g_dvmod[g_dvmod_n - 1]; g_dvmod_n--;
}

static int detour_pass(int enabled) {                    // 1 = escreveu pontos
    unsigned char *g = *(unsigned char **)(g_base + RVA_GC_PTR);
    unsigned ns = *(unsigned *)(g_base + RVA_SPL_COUNT); unsigned long long sb = *(unsigned long long *)(g_base + RVA_SPL_BASE);
    dv_spl *S = NULL; int nS = 0, cap = 0;
    static unsigned char *arr; static size_t arrcap;
    LONG nst = g_dvst_n;
    static unsigned char *mod_gc;
    EnterCriticalSection(&g_dvcs);
    (void)mod_gc;                                         // 1.8: troca de GameCore (todo turno) NAO apaga o controle
    for (LONG i = 0; i < g_dvmod_n; i++) g_dvmod[i].seen = 0;
    LeaveCriticalSection(&g_dvcs);
    int arr_ok = 0;
    if (g && g_rail_data && sb && ns && ns < 2000000 && ((enabled && nst > 0) || g_dvmod_n > 0)) {
        if ((size_t)ns * 0x50 > arrcap) { free(arr); arrcap = (size_t)ns * 0x50 + 0x10000; arr = (unsigned char *)malloc(arrcap); }
        unsigned nconn = *(unsigned *)(g + GC_CONN_COUNT); unsigned long long cb = *(unsigned long long *)(g + GC_CONNS);
        if (arr && rpm(sb, arr, (size_t)ns * 0x50)) {
            arr_ok = 1;
            for (unsigned i = 0; i < ns; i++) {
                unsigned char *r = arr + (size_t)i * 0x50;
                unsigned t = *(unsigned *)(r + SPL_TYPE) & 0x3fff, c = *(unsigned *)(r + SPL_CONN), n = *(unsigned *)(r + SPL_NPTS);
                unsigned long long pp = *(unsigned long long *)(r + SPL_PTS);
                if (t < 4 || t > 10 || c >= nconn || n < 2 || n > DV_MAXPTS || !pp) continue;
                unsigned rr[2]; if (!rpm(cb + (unsigned long long)c * 0x18 + 0xc, rr, 8)) continue;
                int ha = rg_rail(rr[0]) == 1, hb = rr[0] == rr[1] ? ha : rg_rail(rr[1]) == 1;
                if (!ha && !hb) {                                  // sem ferrovia: so interessa se foi desviado (para voltar)
                    EnterCriticalSection(&g_dvcs); int had = mod_find(c) != NULL; LeaveCriticalSection(&g_dvcs);
                    if (!had) continue;
                }
                if (nS == cap) { cap = cap ? cap * 2 : 256; S = (dv_spl *)realloc(S, sizeof(dv_spl) * cap); }
                dv_spl *d = &S[nS]; d->spl = (unsigned char *)(sb + (unsigned long long)i * 0x50); d->pts = pp; d->n = n; d->t = t; d->ra = rr[0]; d->rb = rr[1]; d->c = c;
                d->p = (float *)malloc(n * 16);
                if (!rpm(pp, d->p, n * 16)) { free(d->p); continue; }
                EnterCriticalSection(&g_dvcs);
                dv_mod *m = mod_find(c);
                if (m && m->n != n) { mod_drop((LONG)(m - g_dvmod)); m = NULL; }          // o jogo refez o trecho (outra contagem)
                if (m) {
                    if (xy_eq(d->p, m->tgt, n) || xy_eq(d->p, m->orig, n)) memcpy(d->p, m->orig, n * 16);
                    else { memcpy(m->orig, d->p, n * 16); memcpy(m->tgt, d->p, n * 16); }   // o jogo refez o trecho
                    m->spl = d->spl; m->pts = pp; m->seen = 1;
                }
                LeaveCriticalSection(&g_dvcs);
                { float e[4] = { d->p[0], d->p[1], d->p[(n - 1) * 4], d->p[(n - 1) * 4 + 1] }; d->rail = enabled && ha && hb && tr_lookup(e, t, n); }
                nS++;
            }
        }
    }
    // alvos desta passada
    typedef struct { int si; float *tgt; } dv_tg;
    dv_tg *T = (dv_tg *)calloc(DV_MAXMOD, sizeof(dv_tg)); int nT = 0;
    dv_al al_tgt[DV_MAXST]; memset(al_tgt, 0, sizeof al_tgt);
    char *used = nS ? (char *)calloc(nS, 1) : NULL;
    for (LONG k = 0; k < nst && enabled && nS && T && used; k++) {
        dv_station st = g_dvst[k]; if (!st.rot) continue;
        // ponto de trilho mais proximo
        int bj = -1, bk = 0; float bd = DV_DMAX;
        for (int j = 0; j < nS; j++) {
            if (!S[j].rail || used[j]) continue;
            for (unsigned q = 0; q < S[j].n; q++) { float dx = S[j].p[q * 4] - st.x, dy = S[j].p[q * 4 + 1] - st.y, d = sqrtf(dx * dx + dy * dy); if (d < bd) { bd = d; bj = j; bk = q; } }
        }
        if (bj < 0) { if (!logged[k]) { qlog("Ferrovia: estacao em (%.0f,%.0f) sem trilho a menos de %.0f: sem desvio (o trilho do decalque continua).", st.x, st.y, DV_DMAX); logged[k] = 1; } continue; }
        // corrente: segue a linha pelos trechos de trilho; numa juncao vai pela continuacao mais reta (1.9)
        int ch[DV_MAXCH], rev[DV_MAXCH], nch = 1; ch[0] = bj; rev[0] = 0;
        float lenA = 0, lenB = 0;
        for (int q = 0; q < bk; q++) lenA += v2d((v2){ S[bj].p[q * 4], S[bj].p[q * 4 + 1] }, (v2){ S[bj].p[q * 4 + 4], S[bj].p[q * 4 + 5] });
        for (unsigned q = bk; q + 1 < S[bj].n; q++) lenB += v2d((v2){ S[bj].p[q * 4], S[bj].p[q * 4 + 1] }, (v2){ S[bj].p[q * 4 + 4], S[bj].p[q * 4 + 5] });
        for (int side = 0; side < 2; side++) {
            float acc = side ? lenB : lenA;
            while (acc < DV_LSIDE && nch < DV_MAXCH) {
                int e = side ? ch[nch - 1] : ch[0], er = side ? rev[nch - 1] : rev[0];
                // ponta livre: inicio da corrente (side 0) ou fim (side 1)
                int atEnd = side ? !er : er;                    // a ponta livre e o ultimo ponto do trecho e?
                const float *ep = atEnd ? &S[e].p[(S[e].n - 1) * 4] : &S[e].p[0];
                const float *pp = atEnd ? &S[e].p[(S[e].n - 2) * 4] : &S[e].p[4];   // ponto anterior (direcao de chegada)
                int nx = -1, nxr = 0; float bt = 1e30f;
                for (int j = 0; j < nS; j++) {
                    int a0 = same_pt(&S[j].p[0], ep), a1 = same_pt(&S[j].p[(S[j].n - 1) * 4], ep);
                    if ((!a0 && !a1) || j == e || !S[j].rail || used[j]) continue;
                    int inch = 0; for (int q = 0; q < nch; q++) if (ch[q] == j) inch = 1;
                    if (inch) continue;
                    const float *q1 = a0 ? &S[j].p[4] : &S[j].p[(S[j].n - 2) * 4];
                    float tn = dir_turn((v2){ pp[0], pp[1] }, (v2){ ep[0], ep[1] }, (v2){ ep[0], ep[1] }, (v2){ q1[0], q1[1] });
                    if (tn < bt) { bt = tn; nx = j; nxr = side ? !a0 : !a1; }
                }
                if (nx < 0 || bt > 60) break;
                if (side) { ch[nch] = nx; rev[nch] = nxr; nch++; }
                else { memmove(ch + 1, ch, sizeof(int) * nch); memmove(rev + 1, rev, sizeof(int) * nch); ch[0] = nx; rev[0] = nxr; nch++; }
                for (unsigned q = 0; q + 1 < S[nx].n; q++) acc += v2d((v2){ S[nx].p[q * 4], S[nx].p[q * 4 + 1] }, (v2){ S[nx].p[q * 4 + 4], S[nx].p[q * 4 + 5] });
            }
        }
        int N = 0; for (int q = 0; q < nch; q++) N += S[ch[q]].n; N -= nch - 1;
        // polilinha original da corrente (mundo), de Q1 a Q2; pontos fixos = pontas da corrente e juncoes com outra estrada
        static v2 Lw[DV_RMAX]; static float Sa[DV_RMAX]; static char fx[DV_RMAX]; int M = 0;
        for (int q = 0; q < nch && N <= DV_RMAX; q++) {
            dv_spl *d = &S[ch[q]];
            for (unsigned i = q ? 1 : 0; i < d->n; i++) {
                unsigned ii = rev[q] ? d->n - 1 - i : i; Lw[M].x = d->p[ii * 4]; Lw[M].y = d->p[ii * 4 + 1];
                fx[M] = 0;
                if (i == 0 || i == d->n - 1) {                   // ponta de trecho: fixa se encosta em mais de 2 trechos
                    int deg = 0; for (int j = 0; j < nS; j++) deg += same_pt(&S[j].p[0], &d->p[ii * 4]) + same_pt(&S[j].p[(S[j].n - 1) * 4], &d->p[ii * 4]);
                    fx[M] = deg > 2;
                }
                M++;
            }
        }
        if (M != N) continue;
        fx[0] = fx[M - 1] = 1;
        Sa[0] = 0; for (int i = 1; i < M; i++) Sa[i] = Sa[i - 1] + v2d(Lw[i - 1], Lw[i]);
        v2 cen, ax; float w0, w1; int fix = 0;
        if (!dv_align(Lw, M, (v2){ st.x, st.y }, &cen, &ax, &w0, &w1, &fix)) {
            if (!logged[k]) { qlog("Ferrovia: estacao em (%.0f,%.0f): linha perto dela curta demais (%d trechos) para alinhar; fica como o jogo pos.", st.x, st.y, nch); logged[k] = 1; }
            continue;
        }
        float mv = v2d(cen, (v2){ st.x, st.y });
        if (mv > DV_MOVE_MAX) {
            if (!logged[k]) { qlog("Ferrovia: estacao em (%.0f,%.0f): a linha passa longe (alinhar moveria o predio %.0f un.); fica como o jogo pos.", st.x, st.y, mv); logged[k] = 1; }
            continue;
        }
        al_tgt[k].on = 1; al_tgt[k].tx = cen.x; al_tgt[k].ty = cen.y; al_tgt[k].tc = ax.x; al_tgt[k].ts = ax.y;
        for (int q = 0; q < nch; q++) used[ch[q]] = 1;
        if (fix) {                                              // endireita a janela: pontos livres vao para a corda
            int i0, i1; v2 wa = pl_at(Lw, Sa, M, w0, &i0), wb = pl_at(Lw, Sa, M, w1, &i1);
            float dx = wb.x - wa.x, dy = wb.y - wa.y, ln = dx * dx + dy * dy;
            int s0 = 0;
            for (int q = 0; q < nch && nT < DV_MAXMOD; q++) {
                dv_spl *d = &S[ch[q]]; float *tg = (float *)malloc(d->n * 16); memcpy(tg, d->p, d->n * 16); int moved = 0;
                for (unsigned i = 0; i < d->n; i++) {
                    int li = s0 + (int)i, ii = rev[q] ? (int)(d->n - 1 - i) : (int)i;
                    if (fx[li] || Sa[li] <= w0 || Sa[li] >= w1 || ln < 1) continue;
                    float t = ((Lw[li].x - wa.x) * dx + (Lw[li].y - wa.y) * dy) / ln; if (t < 0) t = 0; if (t > 1) t = 1;
                    tg[ii * 4] = wa.x + dx * t; tg[ii * 4 + 1] = wa.y + dy * t; moved = 1;
                }
                if (moved) { T[nT].si = ch[q]; T[nT].tgt = tg; nT++; } else free(tg);
                s0 += d->n - 1;
            }
        }
        if (logged[k] != 2) {
            qlog("Ferrovia: estacao em (%.0f,%.0f) alinhada a linha: predio movido %.0f un., rotacao %.1f -> %.1f graus, trilho pela faixa dos portoes (%s; corrente de %d trechos).",
                 st.x, st.y, mv, atan2f(st.s, st.c) * 57.29578f, atan2f(ax.y, ax.x) * 57.29578f, fix ? "trecho da linha endireitado" : "linha ja reta ali", nch);
            logged[k] = 2;
        }
    }
    // alinhamento das estacoes: publica para o gancho do desenho (alinha.h)
    for (LONG k = 0; k < nst && k < DV_MAXST; k++) {
        dv_al *g2 = &g_al[k];
        g2->ox = g_dvst[k].x; g2->oy = g_dvst[k].y; g2->oc = g_dvst[k].c; g2->os = g_dvst[k].s;
        g2->tx = al_tgt[k].tx; g2->ty = al_tgt[k].ty; g2->tc = al_tgt[k].tc; g2->ts = al_tgt[k].ts;
        MemoryBarrier(); g2->on = enabled && al_tgt[k].on;
    }
    g_al_n = nst;
    // aplica / desfaz
    int wrote = 0;
    EnterCriticalSection(&g_dvcs);
    for (LONG i = 0; i < g_dvmod_n; i++) g_dvmod[i].live = 0;
    for (int i = 0; i < nT; i++) {
        dv_spl *d = &S[T[i].si]; unsigned c = d->c;
        dv_mod *m = mod_find(c);
        if (!m) {
            if (g_dvmod_n >= DV_MAXMOD) { free(T[i].tgt); continue; }
            m = &g_dvmod[g_dvmod_n++]; m->c = c; m->n = d->n; m->seen = 1;
            m->orig = (float *)malloc(d->n * 16); m->tgt = (float *)malloc(d->n * 16);
            memcpy(m->orig, d->p, d->n * 16); memcpy(m->tgt, d->p, d->n * 16);
        }
        m->spl = d->spl; m->pts = d->pts; m->live = 1;
        float cur[DV_MAXPTS * 4]; unsigned char rec[0x48];
        if (rpm((unsigned long long)d->spl, rec, sizeof rec) && *(unsigned long long *)(rec + SPL_PTS) == d->pts && *(unsigned *)(rec + SPL_NPTS) == d->n &&
            *(unsigned *)(rec + SPL_CONN) == c && rpm(d->pts, cur, d->n * 16)) {
            if (memcmp(cur, T[i].tgt, d->n * 16) && (xy_eq(cur, m->orig, d->n) || xy_eq(cur, m->tgt, d->n))) wrote |= wpm(d->pts, T[i].tgt, d->n * 16);
            memcpy(m->tgt, T[i].tgt, d->n * 16);
        }
        free(T[i].tgt);
    }
    for (LONG i = g_dvmod_n - 1; i >= 0; i--) {
        dv_mod *m = &g_dvmod[i]; if (m->live) continue;
        if (!m->seen) {                                       // ligacao fora do vetor lido nesta passada
            if (arr_ok) mod_drop(i);                          // (vetor lido inteiro e ela nao esta la: o trecho nao existe mais)
            continue;
        }
        float cur[DV_MAXPTS * 4]; unsigned char rec[0x48];
        if (m->n <= DV_MAXPTS && rpm((unsigned long long)m->spl, rec, sizeof rec) && *(unsigned long long *)(rec + SPL_PTS) == m->pts &&
            *(unsigned *)(rec + SPL_NPTS) == m->n && *(unsigned *)(rec + SPL_CONN) == m->c && rpm(m->pts, cur, m->n * 16) && xy_eq(cur, m->tgt, m->n))
            wrote |= wpm(m->pts, m->orig, m->n * 16);         // volta o original
        mod_drop(i);
    }
    LeaveCriticalSection(&g_dvcs);
    for (int j = 0; j < nS; j++) free(S[j].p);
    free(S); free(T); free(used);
    return wrote;
}

static DWORD WINAPI rail_thread(LPVOID unused) {
    (void)unused;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    unsigned last_scan = 0, last_dv = 0; int first = 1;
    for (;;) {
        Sleep(TR_PERIOD_MS);
        rail_snapshot();
        unsigned now = GetTickCount();
        if (now - last_dv < 2000) continue;
        last_dv = now;
        rail_read_ini();
        char v[8]; GetPrivateProfileStringA("Estacao", "DesvioDaLinha", "", v, sizeof v, g_inipath);
        if (!v[0]) { WritePrivateProfileStringA("Estacao", "DesvioDaLinha", "1", g_inipath); strcpy(v, "1"); }
        int en = atoi(v) == 1;
        unsigned char *g = *(unsigned char **)(g_base + RVA_GC_PTR);
        if (en && g && g_rail_data && g_tr_segs > 0 && (first || now - last_scan >= DV_SCAN_S * 1000u)) {
            first = 0; last_scan = now;
            dv_xy pos[DV_MAXST]; int np = 0;
            dv_scan(0, pos, &np, NULL, 0);
            dv_station nst[DV_MAXST]; int nn = 0, pend = 0;
            LONG old = g_dvst_n;
            for (int i = 0; i < np; i++) {
                dv_station s = { pos[i].x, pos[i].y, 1, 0, 0, 0 };
                for (LONG k = 0; k < old; k++) if (g_dvst[k].rot && fabsf(g_dvst[k].x - s.x) < 0.01f && fabsf(g_dvst[k].y - s.y) < 0.01f) { s = g_dvst[k]; break; }
                if (!s.rot) pend++;
                nst[nn++] = s;
            }
            if (pend) dv_scan(1, NULL, NULL, nst, nn);
            int changed = nn != old;
            for (int i = 0; i < nn && !changed; i++) if (nst[i].x != g_dvst[i].x || nst[i].y != g_dvst[i].y || nst[i].rot != g_dvst[i].rot) changed = 1;
            if (changed) {
                for (int i = 0; i < nn; i++) g_dvst[i] = nst[i];
                memset(logged, 0, sizeof logged);
                g_dvst_n = nn;
                int nr = 0; for (int i = 0; i < nn; i++) nr += nst[i].rot;
                qlog("Ferrovia: %d Train Station(s) no mapa (%d com rotacao achada).", nn, nr);
            }
        }
        if (detour_pass(en)) rail_snapshot();               // pontos novos: o conjunto de trilho ja sai com as pontas novas
    }
    return 0;
}
