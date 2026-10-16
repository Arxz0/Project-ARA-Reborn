// --- 1.9: gancho que gira/desloca o visual da Train Station (alvos calculados em desvio.h) ---------------------------
// O jogo monta, a cada quadro, os dados de desenho de cada instancia de modelo em exe+0x94fd90 (chamada por 0x950540):
// r8 = bloco da instancia; matriz de mundo da raiz em bloco + [bloco+0x1c] (linhas X, Y, Z, posicao); bit 1 do byte
// [bloco] = refazer a hierarquia dos nos (0x9508db -> inversa 0x2bbdc0 + hierarquia 0x1686d80). Achado com pontos de
// parada de hardware em 16/10.
// Para cada bloco cuja raiz esta na posicao da estacao (todas as partes do modelo dela), aplica o movimento rigido
// "posicao/rotacao do jogo -> alvo": linhas X,Y giram pelo delta, posicao = alvo + delta * (p - estacao). Guarda a matriz
// original por bloco; se o jogo refizer a matriz, a nova vira a original. Bloco reaproveitado para outra coisa (posicao
// longe da estacao) sai da lista. Sem alvo (estacao sem linha perto), devolve a original.
#define AL_MAXB 64
typedef unsigned long long (*inst_fn)(unsigned, unsigned, unsigned char *, unsigned char *, void *, void *, void *, void *);
static inst_fn g_inst_tramp;
typedef struct { unsigned char *blk; float orig[16], last[16]; float ox, oy; int wrote; } al_blk;
static al_blk g_alb[AL_MAXB]; static int g_alb_n; static volatile LONG g_alb_lock;
static unsigned g_al_logs;
static int m16_eq(const float *a, const float *b) { for (int i = 0; i < 16; i++) if (fabsf(a[i] - b[i]) > 1e-3f) return 0; return 1; }
static void al_target(const dv_al *t, const float *o, float *w) {        // w = movimento rigido aplicado em o
    memcpy(w, o, 64);
    if (!t->on) return;
    float dc = t->tc * t->oc + t->ts * t->os, ds = t->ts * t->oc - t->tc * t->os;   // delta = alvo * inversa(jogo)
    for (int r = 0; r < 2; r++) { float x = o[r * 4], y = o[r * 4 + 1]; w[r * 4] = dc * x - ds * y; w[r * 4 + 1] = ds * x + dc * y; }
    float px = o[12] - t->ox, py = o[13] - t->oy;
    w[12] = t->tx + dc * px - ds * py; w[13] = t->ty + ds * px + dc * py;
}
static unsigned long long hook_inst(unsigned a, unsigned b, unsigned char *blk, unsigned char *out, void *a5, void *a6, void *a7, void *a8) {
    LONG n = g_al_n;
    if (n > 0 && blk) {
        float *M = (float *)(blk + *(unsigned *)(blk + 0x1c));
        int k = -1;                                               // estacao desta matriz (pela posicao original OU pela nossa)
        for (LONG i = 0; i < n && i < 32; i++) {
            const dv_al *t = &g_al[i];
            if ((fabsf(M[12] - t->ox) < 0.5f && fabsf(M[13] - t->oy) < 0.5f) || (t->on && fabsf(M[12] - t->tx) < 0.5f && fabsf(M[13] - t->ty) < 0.5f)) { k = (int)i; break; }
        }
        int known = 0;
        if (k < 0) { for (int i = 0; i < g_alb_n; i++) if (g_alb[i].blk == blk) { known = 1; break; } }
        if (k >= 0 || known) {
            while (InterlockedCompareExchange(&g_alb_lock, 1, 0)) YieldProcessor();
            al_blk *e = NULL; int ei = -1;
            for (int i = 0; i < g_alb_n; i++) if (g_alb[i].blk == blk) { e = &g_alb[i]; ei = i; break; }
            if (!e && k >= 0 && g_alb_n < AL_MAXB && fabsf(M[12] - g_al[k].ox) < 0.5f && fabsf(M[13] - g_al[k].oy) < 0.5f) {
                e = &g_alb[g_alb_n]; ei = g_alb_n++; e->blk = blk; memcpy(e->orig, M, 64); memcpy(e->last, M, 64); e->ox = g_al[k].ox; e->oy = g_al[k].oy; e->wrote = 0;
            }
            if (e) {
                int kk = -1;                                          // estacao do bloco pela posicao original guardada
                for (LONG i = 0; i < n && i < 32; i++) if (fabsf(g_al[i].ox - e->ox) < 0.5f && fabsf(g_al[i].oy - e->oy) < 0.5f) { kk = (int)i; break; }
                if (kk < 0) {                                         // estacao sumiu: devolve a original e esquece o bloco
                    if (e->wrote && m16_eq(M, e->last)) { memcpy(M, e->orig, 64); blk[0] |= 2; }
                    g_alb[ei] = g_alb[--g_alb_n];
                } else {
                    float w[16]; al_target(&g_al[kk], e->orig, w);
                    if (!m16_eq(M, w)) {
                        if (m16_eq(M, e->orig) || (e->wrote && m16_eq(M, e->last))) { memcpy(M, w, 64); memcpy(e->last, w, 64); e->wrote = 1; blk[0] |= 2; }
                        else if (fabsf(M[12] - e->ox) < 0.5f && fabsf(M[13] - e->oy) < 0.5f) {   // o jogo refez a matriz
                            memcpy(e->orig, M, 64); al_target(&g_al[kk], e->orig, w); memcpy(M, w, 64); memcpy(e->last, w, 64); e->wrote = 1; blk[0] |= 2;
                        } else g_alb[ei] = g_alb[--g_alb_n];                 // bloco reaproveitado para outra coisa
                        if (g_al_logs < 12) { g_al_logs++; qlog("Ferrovia: estacao (%.0f,%.0f): bloco %p girado/deslocado para (%.1f,%.1f) rot %.1f graus.", e->ox, e->oy, (void *)blk, w[12], w[13], atan2f(w[1], w[0]) * 57.29578f); }
                    }
                }
            }
            InterlockedExchange(&g_alb_lock, 0);
        }
    }
    return g_inst_tramp(a, b, blk, out, a5, a6, a7, a8);
}
static const unsigned char k_pro_inst[16] = { 0x48,0x8b,0xc4, 0x89,0x48,0x08, 0x56, 0x41,0x56, 0x48,0x81,0xec,0xb8,0x02,0x00,0x00 };
static int alinha_install(void) {
    g_inst_tramp = (inst_fn)ara_hook(g_base + 0x94fd90, k_pro_inst, 16, (void *)hook_inst, "montagem das instancias (alinhar a estacao)");
    return g_inst_tramp != NULL;
}
