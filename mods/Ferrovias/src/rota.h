// --- 1.8: rota do desvio ate a faixa dos portoes (espaco da estacao) ------------------------------------------------
// A 1.6/1.7 ligava as pontas da corrente a faixa pelos cantos do predio: com as duas pontas do mesmo lado a linha
// entrava na faixa e voltava (laco em volta da estacao). Agora a linha so SAI do traçado original num ponto A, corre
// reto pela faixa (com uma reta de aproximacao DV_APRON antes e depois) e VOLTA ao traçado num ponto B. Cada curva tem no
// maximo DV_TURN_MAX graus e a soma das curvas no maximo DV_TURN_SUM: e um desvio em S, nunca um laco. Se a faixa nao
// fica mais ou menos paralela a linha (estacao virada de lado para o trilho), nao ha rota assim e a linha fica reta.
// Funcoes puras (sem memoria do jogo): testadas em dll/test/rota_test.c.
#define DV_APRON     25.0f
#define DV_TURN_MAX  35.0f
#define DV_TURN_SUM  125.0f
#define DV_MINLEG    12.0f
#define DV_RMAX      600

typedef struct { float x, y; } v2;
static float v2d(v2 a, v2 b) { float dx = a.x - b.x, dy = a.y - b.y; return sqrtf(dx * dx + dy * dy); }
static int seg_hits_rect(v2 a, v2 b, float x0, float x1, float y0, float y1) {   // Liang-Barsky
    float dx = b.x - a.x, dy = b.y - a.y, t0 = 0, t1 = 1;
    float pp[4] = { -dx, dx, -dy, dy }, qq[4] = { a.x - x0, x1 - a.x, a.y - y0, y1 - a.y };
    for (int i = 0; i < 4; i++) {
        if (fabsf(pp[i]) < 1e-9f) { if (qq[i] < 0) return 0; }
        else { float t = qq[i] / pp[i]; if (pp[i] < 0) { if (t > t0) t0 = t; } else { if (t < t1) t1 = t; } if (t0 > t1) return 0; }
    }
    return t1 - t0 > 1e-6f;
}
static int seg_hits_bld(v2 a, v2 b) { return seg_hits_rect(a, b, DV_BX0, DV_BX1, DV_BY0, DV_BY1); }
static float dir_turn(v2 a, v2 b, v2 c, v2 d) {           // angulo (graus) entre as direcoes a->b e c->d
    float x1 = b.x - a.x, y1 = b.y - a.y, x2 = d.x - c.x, y2 = d.y - c.y, n1 = sqrtf(x1 * x1 + y1 * y1), n2 = sqrtf(x2 * x2 + y2 * y2);
    if (n1 < 1e-4f || n2 < 1e-4f) return 0;
    float k = (x1 * x2 + y1 * y2) / (n1 * n2); if (k > 1) k = 1; if (k < -1) k = -1;
    return acosf(k) * 57.29578f;
}
// ponto da polilinha L (M pontos, arco acumulado S) no arco s; *seg = segmento que contem s
static v2 pl_at(const v2 *L, const float *S, int M, float s, int *seg) {
    int i = 0; while (i < M - 2 && S[i + 1] < s) i++;
    float len = S[i + 1] - S[i], t = len > 0 ? (s - S[i]) / len : 0; if (t < 0) t = 0; if (t > 1) t = 1;
    *seg = i; return (v2){ L[i].x + (L[i + 1].x - L[i].x) * t, L[i].y + (L[i + 1].y - L[i].y) * t };
}
// rota nova: L[0..] ate A, A -> E_in' -> E_out' -> B, ... L[M-1]; devolve a quantidade de vertices (0 = sem rota boa),
// *strip_i = indice do vertice E_in' (o trecho E_in'->E_out' e a faixa) e *why = angulo da linha com a faixa (diagnostico)
static int dv_plan2(const v2 *L, int M, v2 *route, int *strip_i, float *why) {
    if (M < 2 || M > DV_RMAX) return 0;
    float S[DV_RMAX]; S[0] = 0; for (int i = 1; i < M; i++) S[i] = S[i - 1] + v2d(L[i - 1], L[i]);
    float tot = S[M - 1]; if (tot < 50) return 0;
    // diagnostico: angulo entre a linha (perto da estacao) e a faixa
    { int bi = 0; float bd = 1e30f; for (int i = 0; i + 1 < M; i++) { v2 m = { (L[i].x + L[i + 1].x) / 2, (L[i].y + L[i + 1].y) / 2 }; float d = v2d(m, (v2){ DV_XS, 0 }); if (d < bd) { bd = d; bi = i; } }
      float a = dir_turn(L[bi], L[bi + 1], (v2){ 0, 0 }, (v2){ 0, 1 }); *why = a > 90 ? 180 - a : a; }
    float step = tot / 80; if (step < 5) step = 5;
    float cs[200]; int nc = 0;                                // arcos candidatos para A e B (o ultimo e exatamente o fim)
    for (float s = 0; s < tot && nc < 199; s += step) cs[nc++] = s;
    cs[nc++] = tot;
    float best = 1e30f; int bn = 0;
    for (int dirs = -1; dirs <= 1; dirs += 2) {
        v2 Ei = { DV_XS, -dirs * (DV_YS + DV_APRON) }, Eo = { DV_XS, dirs * (DV_YS + DV_APRON) };
        for (int ca = 0; ca + 1 < nc; ca++) {
            float sa = cs[ca];
            int ia; v2 A = pl_at(L, S, M, sa, &ia);
            if (v2d(A, Ei) < DV_MINLEG || seg_hits_bld(A, Ei)) continue;
            v2 pa0 = sa <= 0 ? L[0] : L[ia], pa1 = sa <= 0 ? L[1] : A;          // direcao da linha chegando em A
            if (sa > 0 && v2d(pa0, pa1) < 1e-3f) { pa0 = L[ia]; pa1 = L[ia + 1]; }
            float tA = dir_turn(pa0, pa1, A, Ei), tEi = dir_turn(A, Ei, Ei, Eo);
            if (tA > DV_TURN_MAX || tEi > DV_TURN_MAX) continue;
            int bad = 0; for (int i = 0; i < ia && !bad; i++) bad = seg_hits_bld(L[i], L[i + 1]);
            if (bad || (sa > 0 && seg_hits_bld(L[ia], A))) continue;
            for (int cb = ca + 1; cb < nc; cb++) {
                float sb = cs[cb];
                int ib; v2 B = pl_at(L, S, M, sb, &ib);
                if (v2d(Eo, B) < DV_MINLEG || seg_hits_bld(Eo, B)) continue;
                v2 pb0 = sb >= tot ? L[M - 2] : B, pb1 = sb >= tot ? L[M - 1] : L[ib + 1];   // direcao da linha saindo de B
                if (v2d(pb0, pb1) < 1e-3f) { pb0 = L[ib]; pb1 = L[ib + 1]; }
                float tEo = dir_turn(Ei, Eo, Eo, B), tB = dir_turn(Eo, B, pb0, pb1), sum = tA + tEi + tEo + tB;
                if (tEo > DV_TURN_MAX || tB > DV_TURN_MAX || sum > DV_TURN_SUM) continue;
                bad = sb < tot && seg_hits_bld(B, L[ib + 1]); for (int i = ib + 1; i + 1 < M && !bad; i++) bad = seg_hits_bld(L[i], L[i + 1]);
                if (bad) continue;
                // vertices: L ate A, A, Ei, Eo, B, L depois de B
                int nv = 0; for (int i = 0; i <= ia; i++) if (S[i] < sa - 1e-3f) nv++;
                nv += 4; for (int i = ib + 1; i < M; i++) if (S[i] > sb + 1e-3f) nv++;
                if (nv > M) continue;
                float len = sa + v2d(A, Ei) + v2d(Ei, Eo) + v2d(Eo, B) + (tot - sb), sc = len + 2.0f * sum;
                if (sc < best) {
                    best = sc; bn = 0;
                    for (int i = 0; i <= ia; i++) if (S[i] < sa - 1e-3f) route[bn++] = L[i];
                    route[bn++] = A; *strip_i = bn; route[bn++] = Ei; route[bn++] = Eo; route[bn++] = B;
                    for (int i = ib + 1; i < M; i++) if (S[i] > sb + 1e-3f) route[bn++] = L[i];
                }
            }
        }
    }
    return bn;
}
// --- 1.9: a estacao se alinha a linha (em vez de a linha ir ate a estacao) ------------------------------------------
// Em coordenadas do mundo. Acha, perto da estacao, uma janela reta de DV_WIN unidades da linha (desvio da corda <= DV_TOL);
// se nao houver, endireita a janela mais proxima (os pontos dela vao para a corda: so deixa a linha MAIS reta). A faixa dos
// portoes (x local = DV_XS) fica sobre o centro da janela, paralela a ela, e o predio do lado mais perto de onde o jogo
// pos a estacao. Devolve 1 e: *cen = novo centro da estacao, *ax = eixo X local (do predio para a linha), *s0/*s1 = arcos
// da janela, *fix = 1 se a janela precisa ser endireitada.
#define DV_WIN 150.0f
#define DV_TOL 4.0f
static int dv_align(const v2 *L, int M, v2 C, v2 *cen, v2 *ax, float *s0, float *s1, int *fix) {
    if (M < 2 || M > DV_RMAX) return 0;
    float S[DV_RMAX]; S[0] = 0; for (int i = 1; i < M; i++) S[i] = S[i - 1] + v2d(L[i - 1], L[i]);
    float tot = S[M - 1]; if (tot < DV_WIN) return 0;
    float best = 1e30f, bestFix = 1e30f; int found = 0; float bs = 0, bfs = 0;
    for (float s = DV_WIN / 2; s <= tot - DV_WIN / 2 + 1e-3f; s += 5) {
        int i0, i1, im; v2 a = pl_at(L, S, M, s - DV_WIN / 2, &i0), b = pl_at(L, S, M, s + DV_WIN / 2, &i1), m = pl_at(L, S, M, s, &im);
        float dx = b.x - a.x, dy = b.y - a.y, ln = sqrtf(dx * dx + dy * dy); if (ln < 1) continue;
        float dev = 0;                                          // maior distancia dos vertices internos ate a corda
        for (int i = i0 + 1; i <= i1; i++) { float d = fabsf((L[i].x - a.x) * dy - (L[i].y - a.y) * dx) / ln; if (d > dev) dev = d; }
        float dc = v2d(m, C);
        if (dev <= DV_TOL) { if (dc < best) { best = dc; bs = s; found = 1; } }
        else if (dc < bestFix) { bestFix = dc; bfs = s; }
    }
    // janela reta so vale se nao estiver muito mais longe que a melhor janela a endireitar
    float s = found && best <= bestFix + 120 ? bs : bfs; *fix = !(found && best <= bestFix + 120);
    if (!found && bestFix >= 1e29f) return 0;
    int i0, i1, im; v2 a = pl_at(L, S, M, s - DV_WIN / 2, &i0), b = pl_at(L, S, M, s + DV_WIN / 2, &i1), m = pl_at(L, S, M, s, &im);
    float dx = b.x - a.x, dy = b.y - a.y, ln = sqrtf(dx * dx + dy * dy); dx /= ln; dy /= ln;
    v2 mid = *fix ? (v2){ (a.x + b.x) / 2, (a.y + b.y) / 2 } : m;
    v2 n1 = { -dy, dx }, n2 = { dy, -dx };                       // as duas normais; o predio fica em mid - DV_XS * n
    v2 c1 = { mid.x - DV_XS * n1.x, mid.y - DV_XS * n1.y }, c2 = { mid.x - DV_XS * n2.x, mid.y - DV_XS * n2.y };
    if (v2d(c1, C) <= v2d(c2, C)) { *cen = c1; *ax = n1; } else { *cen = c2; *ax = n2; }
    *s0 = s - DV_WIN / 2; *s1 = s + DV_WIN / 2;
    return 1;
}
// N amostras sobre a rota: todo vertice e amostra; o resto vai por comprimento (a faixa pesa 2x)
static int dv_resample(const v2 *r, int nr, int strip_i, int N, v2 *out, float *arc) {
    int extra = N - nr; if (extra < 0 || nr < 2 || nr > DV_RMAX) return 0;
    float w[DV_RMAX], tot = 0, seg[DV_RMAX]; int al[DV_RMAX];
    for (int i = 0; i + 1 < nr; i++) { seg[i] = v2d(r[i], r[i + 1]); w[i] = seg[i] * (i == strip_i ? 2.0f : 1.0f); tot += w[i]; }
    if (tot <= 0) return 0;
    int used = 0; float rem[DV_RMAX];
    for (int i = 0; i + 1 < nr; i++) { float x = extra * w[i] / tot; al[i] = (int)x; rem[i] = x - al[i]; used += al[i]; }
    while (used < extra) { int bi = 0; for (int i = 1; i + 1 < nr; i++) if (rem[i] > rem[bi]) bi = i; al[bi]++; rem[bi] = -1; used++; }
    int k = 0; float acc = 0; out[k] = r[0]; arc[k++] = 0;
    for (int i = 0; i + 1 < nr; i++) {
        int m = al[i] + 1;
        for (int j = 1; j <= m; j++) {
            out[k].x = r[i].x + (r[i + 1].x - r[i].x) * j / m; out[k].y = r[i].y + (r[i + 1].y - r[i].y) * j / m;
            arc[k++] = acc + seg[i] * j / m;
        }
        acc += seg[i];
    }
    return k == N;
}
