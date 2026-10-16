// AraQoL 2.0 (Project ARA: Reborn) - correcoes de camera/interface para ARA: History Untold 2.1.0.542.
// Carregado como proxy de swresample-4.dll (o original fica como swresample-4_orig.dll).
//
// O que o jogo faz (engenharia reversa, build 2.1.0.542):
//  * Altura maxima da camera = min(limite do tamanho do mapa [CamSettings +0x30..+0x3c: 18000/20000/25000/28000],
//    max(400, 2.0 * maior lado da regiao em que a camera pode andar [CamSettings +0x90 = 2.0])).
//    Com pouca area explorada a camera nao sobe o bastante para a Visao Estrategica (OverlaySettings: entra entre
//    zoom 0.5 e 0.7 conforme o slider; o zoom vai de 0 a 1).
//  * Bandeiras: tipo i some quando zoom > OverlaySettings[+0x70 + 4*i] (0 = nunca), a menos que "Mostrar" esteja
//    ligado. Recursos = 1.1 (impossivel, o zoom vai ate 1), Rendimentos = 0 (nunca some).
//
// Correcoes:
//  1. Camera: multiplicador 2.0 -> 100.0, entao o teto passa a ser so o limite do tamanho do mapa.
//  2. Bandeiras de Recursos, Rendimentos, Melhorias e Forcas: desligado = escondidas em qualquer zoom;
//     ligado = sempre visiveis (comportamento de "Mostrar" do proprio jogo).
//  3. Clique nas unidades: o jogo testa o raio do mouse contra a esfera envolvente de cada boneco 3D
//     (FUN_140aec400: raio da esfera = raio do modelo + folga global em [picking+0x2b0], que o jogo deixa em 0).
//     O AraQoL poe essa folga no valor de AraQoL.ini (RaioExtraUnidades), relido a cada 2 s.
//  4. Upgrade All (1.6, pedido do Discord): dois botoes novos na interface.
//     - Painel de upgrade de um exercito: "Upgrade All" escolhe, em cada unidade, o melhor upgrade liberado
//       (mais forca; prefere a mesma funcao). O jogador ainda confere o custo e confirma como sempre.
//     - Painel Militar: "Upgrade All Forces" manda a ordem de upgrade para todos os exercitos que podem fazer
//       upgrade agora (o proprio jogo valida territorio, combate, embarque e custo de cada um).
//     Os botoes entram por versoes modificadas de UI\IndusPopups.xml e UI\MilitaryPanel.xml montadas pela DLL e
//     entregues no lugar do texto original na funcao que analisa o XML (0xc2b8c0); os arquivos do jogo nao sao
//     alterados (copias para conferencia em bin\AraQoL_UI). O clique chama as mesmas funcoes dos botoes do jogo.
//     Achados: mensagem de upgrade = tipo 0x39 {+0x10 exercito, +0x1c 12 modelos de destino}, enviada por
//     0x5fd150; painel em 0x690c3c0 (+0x2bac exercito, +0x2bb0 escolhas[12], +0x2be0 qtd, +0x2be4 modo exercito);
//     opcoes de upgrade de um modelo = 0xd70ef0; botao: +0x130 contexto, +0x138 funcao do clique.
//  1.8: igual a 1.6c (so QoL). Ganchos agora encadeaveis, para conviver com mods independentes (Ferrovias).
//  1.9: Prioridade. O jogo poe o proprio processo em ACIMA DO NORMAL quando a janela tem foco (0xbf5790: nivel
//       0 = SetPriorityClass 0x4000 abaixo do normal, 1 = 0x20 normal, 2 = 0x8000 acima do normal). Com todos os
//       nucleos a 100% na passagem de turno isso deixa o Windows sem folga. O AraQoL troca, so na memoria, o 0x8000
//       por 0x20 (normal). O log do jogo continua escrevendo "ABOVE_NORMAL" (o texto e fixo). AraQoL.ini:
//       [Prioridade] LimitarEmNormal=1 (0 = comportamento original; vale ao abrir o jogo).
//  2.0: CPU e Prioridade (1.9/1.9c) sairam para o mod separado ARA CPU Fix (proxy de avutil-58.dll).
//  1.9b: [Camera] ZoomMaximo=1 (0 = teto original do jogo, multiplicador 2.0). Relido a cada 2 s, para comparar
//       com o jogo aberto (suspeita: travadas ao abrir a tela da cidade com o teto alto).
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define EXPECTED_TIMESTAMP   0x6a286b53u
#define EXPECTED_SIZEOFIMAGE 0x5e622e00u
#define RVA_LOOKUP           0x0122e570ull   // (tipoHash, bibliotecaHash, nomeHash) -> ponteiro do dado
#define RVA_DATALIB_PTR      0x3b9f2c40ull   // ponteiro global das bibliotecas de dados
#define RVA_ZOOM             0x5686feb4ull   // zoom atual (0 = perto, 1 = longe)
#define RVA_CAMH             0x053726d0ull   // altura da camera
#define RVA_CAM_MULT         0x5686fad0ull   // copia viva de CamSettings+0x90 (multiplicador da regiao)
#define RVA_BANNER_STATE     0x065883e8ull   // 20 x {float alpha; u8 alwaysVis; ...}
#define H_WORLDVIEW   0x509aa0ddu
#define H_OVERLAYDEF  0xb9b0c759u            // OverlaySettingsSchema.OverlaySettingsDef
#define H_OVERLAYSET  0x5304db6eu            // "OverlaySettings"
#define H_CAMDEF      0x8d394d9du            // CameraSettings.CamSettings
#define H_CAMERA      0x06c42abfu            // "Camera"
#define OFF_HIDE_ABOVE 0x70                  // float[20]
#define CAM_MULT_ORIG  2.0f
#define CAM_MULT_NEW   100.0f
#define HIDE_ALWAYS    0.0001f               // > 0 e abaixo de qualquer zoom real: some sempre (se "Mostrar" desligado)

typedef void *(*lookup_t)(unsigned, unsigned, unsigned);
static unsigned char *g_base;
static char g_logpath[MAX_PATH];
static const int k_banner_types[] = { 1, 2, 3, 4, 5, 6 };   // 1 Recursos, 2 Rendimentos, 3/4/5 Melhorias, 6 Forcas

static void qlog(const char *fmt, ...) {
    FILE *f = fopen(g_logpath, "a"); if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

static int mem_ok(const void *p, size_t n, int write) {
    MEMORY_BASIC_INFORMATION m;
    if (!p || !VirtualQuery(p, &m, sizeof m) || m.State != MEM_COMMIT) return 0;
    if (m.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
    DWORD w = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    DWORD r = w | PAGE_READONLY | PAGE_EXECUTE_READ;
    if (!(m.Protect & (write ? w : r))) return 0;
    return (const unsigned char *)p + n <= (const unsigned char *)m.BaseAddress + m.RegionSize;
}

static int verify_target(void) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)g_base;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(g_base + dos->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != EXPECTED_TIMESTAMP || nt->OptionalHeader.SizeOfImage != EXPECTED_SIZEOFIMAGE) {
        qlog("Executavel diferente do esperado (timestamp %08x, size %08x). Nada foi alterado.",
             (unsigned)nt->FileHeader.TimeDateStamp, (unsigned)nt->OptionalHeader.SizeOfImage);
        return 0;
    }
    static const unsigned char sig[] = { 0x45,0x8b,0xc8,0x44,0x8b,0xc2,0x8b,0xd1,0x48,0x8d,0x0d };
    if (memcmp(g_base + RVA_LOOKUP, sig, sizeof sig)) { qlog("Assinatura da funcao de busca nao confere. Nada foi alterado."); return 0; }
    return 1;
}

static float F(unsigned long long rva) { return *(volatile float *)(g_base + rva); }

// So aceita a tabela "Camera" se ela tiver os valores conhecidos desta versao.
static int cam_ok(const float *c) {
    return c && mem_ok(c, 0xa0, 1) && c[0x40/4] == 400.0f && c[0x44/4] == 0.9f && c[0x30/4] == 18000.0f && c[0x3c/4] == 28000.0f
           && (c[0x90/4] == CAM_MULT_ORIG || c[0x90/4] == CAM_MULT_NEW);
}
// So aceita OverlaySettings com os limites conhecidos (Recursos 1.1, Melhorias 0.5, Forcas 0.7) ou ja ajustados.
static int ovl_ok(const float *d) {
    if (!d || !mem_ok(d, 0x140, 1)) return 0;
    const float *h = d + OFF_HIDE_ABOVE / 4;
    return (h[1] == 1.1f || h[1] == HIDE_ALWAYS) && (h[3] == 0.5f || h[3] == HIDE_ALWAYS) && (h[6] == 0.7f || h[6] == HIDE_ALWAYS);
}

#define RVA_PICK_PAD 0x3f9ad030ull // float: folga somada ao raio da esfera de cada objeto testado (picking A, base 0x3f9acd80)
static char g_inipath[MAX_PATH];
static float g_pad = 40.0f;
static volatile int g_cam_zoom = 1;
static void read_ini(void) {
    char v[64];
    GetPrivateProfileStringA("Camera", "ZoomMaximo", "", v, sizeof v, g_inipath);
    if (!v[0]) { WritePrivateProfileStringA("Camera", "ZoomMaximo", "1", g_inipath); strcpy(v, "1"); }
    int cz = atoi(v) == 1;
    if (cz != g_cam_zoom) qlog("Camera: ZoomMaximo = %d (%s).", cz, cz ? "teto = limite do tamanho do mapa" : "teto original do jogo");
    g_cam_zoom = cz;
    GetPrivateProfileStringA("Clique", "RaioExtraUnidades", "", v, sizeof v, g_inipath);
    if (!v[0]) {
        WritePrivateProfileStringA("Clique", "RaioExtraUnidades", "40", g_inipath);
        strcpy(v, "40");
    }
    float x = (float)atof(v);
    if (x < 0) x = 0; if (x > 2000) x = 2000;
    if (x != g_pad) qlog("Clique: RaioExtraUnidades = %.1f", x);
    g_pad = x;
}
static void apply_pick_pad(void) {
    static int logged = 0;
    float *pad = (float *)(g_base + RVA_PICK_PAD);
    if (!mem_ok(pad, 4, 1)) return;
    float cur = *pad;
    if (!(cur >= 0.0f && cur <= 2000.0f)) { if (!logged) { qlog("Clique: valor inesperado na folga (%g). Nada alterado.", cur); logged = 1; } return; }
    if (!logged) { qlog("Clique: folga original do jogo = %.2f -> %.1f", cur, g_pad); logged = 1; }
    if (cur != g_pad) *pad = g_pad;
}

// ---------------------------------------------------------------------------------------------------------------
// 4. Upgrade All.
#define RVA_GC_PTR        0x23e37650ull      // ponteiro global do GameCore
#define OFF_LOCAL_NATION  0x78090ull         // u32: nacao do jogador
#define OFF_NATIONS       0x73280ull         // ponteiro p/ nacoes (0xbec0 cada)
#define NATION_STRIDE     0xbec0ull
#define OFF_ARMY_TAB      0xac0ull           // mapa id -> exercito (entradas 0xd8, ponteiro = base + 8 + idx*0xd8)
#define OFF_UNIT_TAB      0x77200ull         // mapa id -> unidade (ponteiro = entrada + 8; +8 = modelo)
#define RVA_UPG_PANEL     0x0690c3c0ull      // objeto do painel de upgrade
#define RVA_PANEL_REFRESH 0x007dcbd0ull      // atualiza o painel (o dropdown chama depois de gravar a escolha)
#define RVA_UPG_OPTIONS   0x00d70ef0ull      // (gc, nacao, modelo, vetor*) -> modelos para os quais pode fazer upgrade
#define RVA_TEMPLATE      0x00dc8010ull      // modelo de unidade por id
#define RVA_MAP_UNIT      0x003d29f0ull      // (mapa, &saida, id, 0) -> char
#define RVA_ARMY_CHECK    0x00d62250ull      // (gc, 0, exercito, ordem, -1, 4, 0) -> 0 = pode
#define RVA_MSG_INIT      0x00e44fd0ull      // (msg, tipo, nacao)
#define RVA_SEND_UPGRADE  0x005fd150ull      // (msg 0x39) -> valida e poe na fila do GameCore
#define RVA_GAME_FREE     0x00bea8b0ull      // free do alocador do jogo (vetores de 0xd70ef0)
#define RVA_BIND_BUTTON   0x00ca6d00ull      // (contexto, &nome, &saida) -> controle Button pelo ID do XML (0 se nao achar)
#define RVA_HASH          0x00a5c1a0ull      // hash de nome (str, len)
#define RVA_UI_PARSE      0x00c2b8c0ull      // analisa o XML de um contexto de UI: (ctx, buffer)
#define RVA_BIND_POPUPS   0x0115dda0ull      // liga os controles do IndusPopups.xml (contexto)
#define RVA_BIND_MILITARY 0x010f6720ull      // liga os controles do MilitaryPanel.xml (contexto)
#define ORDER_UPGRADE     0x24
#define MSG_ARMY_UPGRADE  0x39
#define MSG_SIZE          0xc30
#define OFF_BTN_CTX       0x130
#define OFF_BTN_FN        0x138
#define PANEL_ARMY        0x2bac
#define PANEL_SEL         0x2bb0
#define PANEL_SELCOUNT    0x2be0
#define PANEL_ARMYMODE    0x2be4
#define TPL_STRENGTH      0x118
#define TPL_ERA           0x12c
#define TPL_ROLE          0x130
#define ARMY_OWNER        0x18
#define ARMY_ORDER        0x1c
#define ARMY_UNITS        0x34

typedef struct { const char *p; unsigned n; unsigned h; } gstr_t;
typedef struct { unsigned n, cap; unsigned *d; } uvec_t;
typedef unsigned char *(*tpl_fn)(unsigned);
typedef char (*map_unit_fn)(void *, unsigned char **, unsigned, int);
typedef void (*upg_opts_fn)(void *, void *, unsigned, uvec_t *);
typedef unsigned long long (*army_check_fn)(void *, unsigned, void *, int, unsigned, unsigned, void *);
typedef void *(*msg_init_fn)(void *, unsigned, unsigned);
typedef unsigned long long (*send_fn)(void *);
typedef void (*free_fn)(void *);
typedef unsigned long long (*bind_fn)(void *, gstr_t *, void **);
typedef unsigned (*hash_fn)(const char *, unsigned);
typedef void (*refresh_fn)(void *);
typedef void (*binder_fn)(void *);

static int g_upg_ok, g_upg_all_idle = 1;
static char g_txt_panel[96] = "Upgrade All", g_txt_military[96] = "Upgrade All Forces";
static binder_fn g_bind_popups_tramp, g_bind_military_tramp;

static unsigned char *gc(void) { unsigned char **pp = (unsigned char **)(g_base + RVA_GC_PTR); return mem_ok(pp, 8, 0) ? *pp : NULL; }
static unsigned char *tpl(unsigned id) { return id == 0xffffffffu ? NULL : ((tpl_fn)(g_base + RVA_TEMPLATE))(id); }

// Melhor upgrade liberado para o modelo cur: mais forca (so se for maior que a atual), preferindo a mesma funcao.
static unsigned best_upgrade(unsigned char *g, unsigned char *nat, unsigned cur) {
    unsigned char *ct = tpl(cur); if (!ct) return 0xffffffffu;
    uvec_t v = { 0, 0, NULL };
    ((upg_opts_fn)(g_base + RVA_UPG_OPTIONS))(g, nat, cur, &v);
    unsigned best = 0xffffffffu; int bsame = -1, bs = *(int *)(ct + TPL_STRENGTH); unsigned bera = 0;
    for (unsigned i = 0; i < v.n && v.d; i++) {
        unsigned c = v.d[i]; if (c == cur) continue;
        unsigned char *t = tpl(c); if (!t) continue;
        int same = t[TPL_ROLE] == ct[TPL_ROLE], s = *(int *)(t + TPL_STRENGTH); unsigned era = t[TPL_ERA];
        if (s <= *(int *)(ct + TPL_STRENGTH)) continue;
        if (best == 0xffffffffu || same > bsame || (same == bsame && (s > bs || (s == bs && era > bera)))) {
            best = c; bsame = same; bs = s; bera = era;
        }
    }
    if (v.d) ((free_fn)(g_base + RVA_GAME_FREE))(v.d);
    return best;
}

static unsigned char *unit_tpl_id(unsigned char *g, unsigned uid, unsigned *out) {
    unsigned char *u = NULL;
    if (uid == 0xffffffffu || !((map_unit_fn)(g_base + RVA_MAP_UNIT))(g + OFF_UNIT_TAB, &u, uid, 0) || !u) return NULL;
    if (u[0] & 1) return NULL;               // unidade marcada (o jogo pula esse bit em todo lugar)
    *out = *(unsigned *)(u + 8);
    return u;
}

static unsigned char *army_by_id(unsigned char *g, unsigned id) {
    unsigned char *t = g + OFF_ARMY_TAB;
    unsigned sorted = *(unsigned *)(t + 8), extra = *(unsigned *)t;
    unsigned *pairs = *(unsigned **)(t + 0x18); unsigned char *ents = *(unsigned char **)(t + 0x28);
    if (!pairs || !ents) return NULL;
    for (unsigned k = 0; k < sorted + extra; k++)
        if (pairs[k * 2] == id) return ents + 8 + (unsigned long long)pairs[k * 2 + 1] * 0xd8;
    return NULL;
}

// Botao do painel de upgrade: preenche as escolhas e deixa o jogo recalcular custo/estado.
static void __cdecl cb_panel_upgrade_all(void *ctx, unsigned char *ev) {
    (void)ctx;
    if (!mem_ok(ev + 0x38, 4, 0) || *(int *)(ev + 0x38) != 0) return;
    unsigned char *g = gc(), *panel = g_base + RVA_UPG_PANEL;
    if (!g || !panel[PANEL_ARMYMODE]) { qlog("UpgradeAll(painel): painel nao esta em modo exercito."); return; }
    unsigned nation = *(unsigned *)(g + OFF_LOCAL_NATION);
    unsigned char *nat = *(unsigned char **)(g + OFF_NATIONS) + nation * NATION_STRIDE;
    unsigned army_id = *(unsigned *)(panel + PANEL_ARMY), cnt = *(unsigned *)(panel + PANEL_SELCOUNT);
    unsigned char *army = army_by_id(g, army_id);
    if (!army) { qlog("UpgradeAll(painel): exercito %u nao encontrado.", army_id); return; }
    if (cnt > 12) cnt = 12;
    int changed = 0;
    for (unsigned i = 0; i < cnt; i++) {
        unsigned cur; if (!unit_tpl_id(g, *(unsigned *)(army + ARMY_UNITS + i * 4), &cur)) continue;
        unsigned b = best_upgrade(g, nat, cur);
        unsigned char *t0 = tpl(cur), *t1 = tpl(b);
        qlog("  slot %u: modelo %u (forca %d) -> %s %u (forca %d)", i, cur, t0 ? *(int *)(t0 + TPL_STRENGTH) : -1,
             b == 0xffffffffu ? "sem upgrade" : "upgrade", b, t1 ? *(int *)(t1 + TPL_STRENGTH) : -1);
        if (b != 0xffffffffu) { *(unsigned *)(panel + PANEL_SEL + i * 4) = b; changed++; }
    }
    qlog("UpgradeAll(painel): exercito %u, %d unidade(s) com upgrade escolhido.", army_id, changed);
    ((refresh_fn)(g_base + RVA_PANEL_REFRESH))(panel);
}

static struct { unsigned id; ULONGLONG t; } g_sent[64]; static int g_sent_i;
// Botao do painel Militar: ordem de upgrade para todos os exercitos do jogador que podem fazer upgrade agora.
static void __cdecl cb_military_upgrade_all(void *ctx, unsigned char *ev) {
    (void)ctx;
    if (!mem_ok(ev + 0x38, 4, 0) || *(int *)(ev + 0x38) != 0) return;
    static ULONGLONG last_click; ULONGLONG now = GetTickCount64();
    if (now - last_click < 3000) { qlog("UpgradeAll(militar): clique repetido ignorado."); return; }
    last_click = now;
    unsigned char *g = gc(); if (!g) return;
    unsigned nation = *(unsigned *)(g + OFF_LOCAL_NATION);
    unsigned char *nat = *(unsigned char **)(g + OFF_NATIONS) + nation * NATION_STRIDE;
    unsigned char *t = g + OFF_ARMY_TAB;
    unsigned total = *(unsigned *)(t + 8) + *(unsigned *)t;
    unsigned *pairs = *(unsigned **)(t + 0x18); unsigned char *ents = *(unsigned char **)(t + 0x28);
    if (!pairs || !ents || total > 100000) return;
    unsigned ids[4096]; unsigned n = 0;   // copia: o envio pode mexer no mapa
    for (unsigned k = 0; k < total && n < 4096; k++) {
        if (pairs[k * 2] == 0xffffffffu) continue;
        unsigned char *a = ents + 8 + (unsigned long long)pairs[k * 2 + 1] * 0xd8;
        if (*(unsigned *)(a + ARMY_OWNER) == nation) ids[n++] = pairs[k * 2];
    }
    int sent = 0, none = 0, busy = 0, invalid = 0;
    for (unsigned k = 0; k < n; k++) {
        unsigned char *a = army_by_id(g, ids[k]); if (!a) continue;
        unsigned order = *(unsigned *)(a + ARMY_ORDER);
        if (order == ORDER_UPGRADE) { busy++; continue; }
        int recent = 0;   // ordem enviada ha pouco que o GameCore ainda nao aplicou (ordem continua a anterior)
        for (int r = 0; r < 64; r++) if (g_sent[r].id == ids[k] && now - g_sent[r].t < 20000) recent = 1;
        if (recent) { qlog("UpgradeAll(militar): exercito %u ja recebeu a ordem ha pouco.", ids[k]); busy++; continue; }
        if (g_upg_all_idle && (order == 1 || order == 2 || order == 0x23)) {   // marchando / atacando
            qlog("UpgradeAll(militar): exercito %u pulado (ordem %u em andamento).", ids[k], order); busy++; continue;
        }
        unsigned long long r = ((army_check_fn)(g_base + RVA_ARMY_CHECK))(g, 0, a, ORDER_UPGRADE, 0xffffffffu, 4, NULL);
        if (r) { qlog("UpgradeAll(militar): exercito %u nao pode fazer upgrade agora (codigo %llu, ordem %u).", ids[k], r, order); invalid++; continue; }
        unsigned tg[12]; int any = 0;
        for (int i = 0; i < 12; i++) {
            unsigned cur; tg[i] = 0xffffffffu;
            if (unit_tpl_id(g, *(unsigned *)(a + ARMY_UNITS + i * 4), &cur)) { tg[i] = best_upgrade(g, nat, cur); if (tg[i] != 0xffffffffu) any++; }
        }
        if (!any) { none++; continue; }
        unsigned char msg[MSG_SIZE] __attribute__((aligned(16)));
        memset(msg, 0, sizeof msg);
        ((msg_init_fn)(g_base + RVA_MSG_INIT))(msg, MSG_ARMY_UPGRADE, nation);
        *(unsigned *)(msg + 0x10) = ids[k];
        memcpy(msg + 0x1c, tg, sizeof tg);
        ((send_fn)(g_base + RVA_SEND_UPGRADE))(msg);
        g_sent[g_sent_i].id = ids[k]; g_sent[g_sent_i].t = now; g_sent_i = (g_sent_i + 1) & 63;
        qlog("UpgradeAll(militar): exercito %u -> ordem de upgrade enviada (%d unidade(s), ordem anterior %u).", ids[k], any, order);
        sent++;
    }
    qlog("UpgradeAll(militar): %u exercito(s) do jogador; enviados %d, sem upgrade %d, ocupados %d, recusados %d.", n, sent, none, busy, invalid);
}

// --- Botoes no XML -------------------------------------------------------------------------------------------
static char *read_all(const wchar_t *path, size_t *len) {
    FILE *f = _wfopen(path, L"rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc(n + 1); if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, n, f); fclose(f); b[r] = 0; *len = r; return b;
}
static int write_all(const wchar_t *path, const char *s, size_t n) {
    FILE *f = _wfopen(path, L"wb"); if (!f) return 0;
    size_t w = fwrite(s, 1, n, f); fclose(f); return w == n;
}
static void xml_safe(char *s) { for (; *s; s++) if (*s == '<' || *s == '>' || *s == '&' || *s == '"' || *s == '\'' || (unsigned char)*s < 32) *s = ' '; }

// Substitui a 1a ocorrencia de 'from' por 'to' dentro de [a, b).
static int repl_in(char *buf, size_t *len, size_t a, size_t b, const char *from, const char *to) {
    char save = buf[b]; buf[b] = 0; char *p = strstr(buf + a, from); buf[b] = save;
    if (!p) return 0;
    size_t fl = strlen(from), tl = strlen(to), off = p - buf;
    memmove(p + tl, p + fl, *len - off - fl + 1); memcpy(p, to, tl); *len += tl - fl; return 1;
}
static size_t line_end(const char *buf, size_t pos) { const char *e = strchr(buf + pos, '\n'); return e ? (size_t)(e - buf) + 1 : strlen(buf); }
static size_t line_start(const char *buf, size_t pos) { while (pos && buf[pos - 1] != '\n') pos--; return pos; }

static int build_popups(const wchar_t *src, const wchar_t *dst) {
    size_t len; char *b = read_all(src, &len); if (!b) { qlog("UpgradeAll: nao li %ls.", src); return 0; }
    char *nb = (char *)malloc(len + 4096); memcpy(nb, b, len + 1); free(b); b = nb;
    int ok = 0;
    char *c = strstr(b, "ID=\"UnitUpgradeCancelButton\""), *k = strstr(b, "ID=\"UnitUpgradeConfirmButton\"");
    if (c && k && !strstr(b, "AraUpgradeAllButton")) {
        size_t cs = line_start(b, c - b), ce = line_end(b, c - b);
        if (repl_in(b, &len, cs, ce, "Size=\"330,50\"", "Size=\"215,50\"")) {
            k = strstr(b, "ID=\"UnitUpgradeConfirmButton\"");
            size_t ks = line_start(b, k - b), ke = line_end(b, k - b);
            if (repl_in(b, &len, ks, ke, "Size=\"330,50\"", "Size=\"215,50\"")) {
                ke = line_end(b, strstr(b, "ID=\"UnitUpgradeConfirmButton\"") - b);
                char ins[512];
                int il = snprintf(ins, sizeof ins, "                <!-- AraQoL: Upgrade All -->\r\n"
                    "                <Button ID=\"AraUpgradeAllButton\"           Anchor=\"c,c\"    Offset=\"0,0\"    Style=\"Button-Tier1\" "
                    "TextStyle=\"ButtonBase-Text-Truncate\" Case=\"Upper\" Size=\"215,50\" AudioID=\"WorldViewMenu_Confirm\" String=\"%s\"/>\r\n", g_txt_panel);
                memmove(b + ke + il, b + ke, len - ke + 1); memcpy(b + ke, ins, il); len += il; ok = 1;
            }
        }
    }
    if (ok) ok = write_all(dst, b, len);
    if (!ok) qlog("UpgradeAll: IndusPopups.xml com formato inesperado; botao do painel de upgrade desligado.");
    free(b); return ok;
}

static int build_military(const wchar_t *src, const wchar_t *dst) {
    size_t len; char *b = read_all(src, &len); if (!b) { qlog("UpgradeAll: nao li %ls.", src); return 0; }
    char *nb = (char *)malloc(len + 4096); memcpy(nb, b, len + 1); free(b); b = nb;
    int ok = 0;
    char *f = strstr(b, "<Container Style=\"LeftPanelFooterMed\"");
    char *gr = f ? strstr(f, "SidePanelBottomGradient\"/>") : NULL;
    if (gr && !strstr(b, "AraUpgradeAllArmiesButton")) {
        size_t e = line_end(b, gr - b);
        char ins[512];
        int il = snprintf(ins, sizeof ins, "                    <!-- AraQoL: Upgrade All Forces -->\r\n"
            "                    <Button ID=\"AraUpgradeAllArmiesButton\" Anchor=\"c,t\" Offset=\"0,22\" Style=\"Button-Tier1\" "
            "TextStyle=\"ButtonBase-Text-Truncate\" Case=\"Upper\" Size=\"340,48\" AudioID=\"WorldViewMenu_Confirm\" String=\"%s\"/>\r\n", g_txt_military);
        memmove(b + e + il, b + e, len - e + 1); memcpy(b + e, ins, il); len += il;
        ok = write_all(dst, b, len);
    }
    if (!ok) qlog("UpgradeAll: MilitaryPanel.xml com formato inesperado; botao do painel Militar desligado.");
    free(b); return ok;
}

static int ends_with_ci(const char *p, unsigned n, const char *suf) {
    size_t sl = strlen(suf); if (n < sl) return 0;
    for (size_t i = 0; i < sl; i++) {
        char a = p[n - sl + i], c = suf[i];
        if (a == '\\') a = '/';
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != c) return 0;
    }
    return n == sl || p[n - sl - 1] == '/' || p[n - sl - 1] == '\\';
}

// O XML de cada contexto de UI (caminho em ctx+0x168/+0x170, ex. "UI/IndusPopups.xml") e lido por um carregador
// assincrono e entregue a 0xc2b8c0(ctx, buf) - buf = {u64 tamanho, u64 cap, char* dados, alocador} - que analisa o texto
// ali mesmo (escreve dados[tamanho] = 0). O AraQoL entrega uma copia do XML modificado nesse ponto.
static char *g_xml_popups, *g_xml_military;
static size_t g_xml_popups_len, g_xml_military_len;
typedef void (*parse_fn)(unsigned char *, long long *);
static parse_fn g_parse_tramp;
static volatile LONG g_bound_popups, g_bound_military, g_missing_logs;

static void hook_parse(unsigned char *ctx, long long *buf) {
    const char *xml = NULL; size_t len = 0;
    const char *p = *(const char **)(ctx + 0x168); unsigned n = *(unsigned *)(ctx + 0x170);
    if (p && n > 4 && n < 512 && mem_ok(p, n, 0)) {
        if (g_xml_popups && ends_with_ci(p, n, "ui/induspopups.xml")) { xml = g_xml_popups; len = g_xml_popups_len; }
        else if (g_xml_military && ends_with_ci(p, n, "ui/militarypanel.xml")) { xml = g_xml_military; len = g_xml_military_len; }
    }
    if (xml) {
        char *copy = (char *)malloc(len + 1);   // a analise guarda ponteiros para o texto: a copia nunca e liberada
        if (copy) {
            memcpy(copy, xml, len); copy[len] = 0;
            long long fake[4] = { (long long)len, (long long)len + 1, (long long)copy, buf ? buf[3] : 0 };
            qlog("UpgradeAll: %.*s carregado com os botoes do AraQoL (%u bytes; original %lld).", (int)n, p, (unsigned)len, buf ? buf[0] : -1);
            g_parse_tramp(ctx, fake);
            return;
        }
    }
    g_parse_tramp(ctx, buf);
}

// Os "binders" rodam muitas vezes; liga o botao sempre (barato) e so registra a primeira vez.
static void bind_button(void *ctx, const char *id, void *fn, const char *what, volatile LONG *once) {
    gstr_t s = { id, (unsigned)strlen(id), ((hash_fn)(g_base + RVA_HASH))(id, (unsigned)strlen(id)) };
    void *btn = NULL;
    ((bind_fn)(g_base + RVA_BIND_BUTTON))(ctx, &s, &btn);
    if (!btn || !mem_ok((unsigned char *)btn + OFF_BTN_CTX, 16, 1)) {
        if (g_missing_logs < 3) { InterlockedIncrement(&g_missing_logs); qlog("UpgradeAll: botao %s ainda nao existe na interface.", what); }
        return;
    }
    if (*(void **)((unsigned char *)btn + OFF_BTN_FN) != fn) {
        *(void **)((unsigned char *)btn + OFF_BTN_CTX) = g_base + RVA_UPG_PANEL;
        *(void **)((unsigned char *)btn + OFF_BTN_FN) = fn;
        if (InterlockedExchange(once, 1) == 0) qlog("UpgradeAll: botao %s ligado (%p).", what, btn);
    }
}
static void hook_bind_popups(void *ctx) {
    g_bind_popups_tramp(ctx);
    if (g_xml_popups) bind_button(ctx, "AraUpgradeAllButton", (void *)cb_panel_upgrade_all, "do painel de upgrade", &g_bound_popups);
}
static void hook_bind_military(void *ctx) {
    g_bind_military_tramp(ctx);
    if (g_xml_military) bind_button(ctx, "AraUpgradeAllArmiesButton", (void *)cb_military_upgrade_all, "do painel Militar", &g_bound_military);
}

// Trampolim: os primeiros n bytes (sem enderecos relativos, conferidos byte a byte) vao para a copia.
static const unsigned char k_pro_bind[20] = { 0x48,0x89,0x5c,0x24,0x08, 0x48,0x89,0x74,0x24,0x10, 0x48,0x89,0x7c,0x24,0x18, 0x4c,0x89,0x74,0x24,0x20 };
static const unsigned char k_pro_parse[24] = { 0x48,0x89,0x5c,0x24,0x18, 0x48,0x89,0x74,0x24,0x20, 0x57, 0x41,0x54, 0x41,0x55, 0x41,0x56, 0x41,0x57,
                                               0xb8,0x30,0x02,0x01,0x00 };
// Gancho encadeavel (ara_hook.h): convive com outros mods que enganchem a mesma funcao (ex.: Ferrovias no XML).
#define hook_log qlog
#include "../../../common/ara_hook.h"
static void *hookn(unsigned long long rva, const unsigned char *pro, int n, void *fn, const char *what) {
    void *t = ara_hook(g_base + rva, pro, n, fn, what);
    if (!t) qlog("UpgradeAll: gancho de %s falhou.", what);
    return t;
}

// Textos dos botoes no idioma do jogo (Locale= em %LOCALAPPDATA%\Ara History Untold\Settings_v121.txt).
// Vocabulario da traducao oficial: TXT_UPGRADE_ORDER e TXT_MILITARY de cada idioma. UTF-8.
static const struct { const char *loc, *panel, *military; } k_txt[] = {
    { "pt", "Aprimorar Tudo",  "Aprimorar Todas as For\xc3\xa7" "as" },
    { "es", "Mejorar todo",    "Mejorar todas las tropas" },
    { "fr", "Tout am\xc3\xa9liorer", "Am\xc3\xa9liorer toutes les forces" },
    { "de", "Alle aufwerten",  "Alle Streitkr\xc3\xa4" "fte aufwerten" },
    { "it", "Potenzia tutto",  "Potenzia tutti gli eserciti" },
    { "pl", "Ulepsz wszystko", "Ulepsz wszystkie si\xc5\x82y" },
    { "ru", "\xd0\x9e\xd0\xb1\xd0\xbd\xd0\xbe\xd0\xb2\xd0\xb8\xd1\x82\xd1\x8c \xd0\xb2\xd1\x81\xd0\xb5",
            "\xd0\x9e\xd0\xb1\xd0\xbd\xd0\xbe\xd0\xb2\xd0\xb8\xd1\x82\xd1\x8c \xd0\xb2\xd1\x81\xd0\xb5 \xd0\xb2\xd0\xbe\xd0\xb9\xd1\x81\xd0\xba\xd0\xb0" },
};
static char g_locale[16] = "en";
static void read_locale(void) {
    wchar_t path[MAX_PATH];
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH)) return;
    wcscat(path, L"\\Ara History Untold\\Settings_v121.txt");
    FILE *f = _wfopen(path, L"rb"); if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) if (!strncmp(line, "Locale=", 7)) {
        int i = 0; for (const char *q = line + 7; *q && *q != ';' && *q > ' ' && i < 15; q++) g_locale[i++] = *q;
        g_locale[i] = 0; break;
    }
    fclose(f);
}
// Valor do ini: vazio, "auto" ou o texto padrao em ingles da 1.6 = usar o idioma do jogo.
static void pick_text(const char *key, const char *en, int which, char *out, size_t outsz) {
    char v[96];
    GetPrivateProfileStringA("UpgradeAll", key, "", v, sizeof v, g_inipath);
    if (!v[0] || !strcmp(v, en)) { WritePrivateProfileStringA("UpgradeAll", key, "auto", g_inipath); strcpy(v, "auto"); }
    if (_stricmp(v, "auto")) { v[outsz - 1] = 0; xml_safe(v); strcpy(out, v); return; }
    const char *t = en;
    for (size_t i = 0; i < sizeof k_txt / sizeof *k_txt; i++)
        if (!_strnicmp(g_locale, k_txt[i].loc, 2)) { t = which ? k_txt[i].military : k_txt[i].panel; break; }
    snprintf(out, outsz, "%s", t);
}
static void upg_read_ini(void) {
    char v[32];
    read_locale();
    pick_text("TextoPainel", "Upgrade All", 0, g_txt_panel, sizeof g_txt_panel);
    pick_text("TextoMilitar", "Upgrade All Forces", 1, g_txt_military, sizeof g_txt_military);
    GetPrivateProfileStringA("UpgradeAll", "PularExercitosEmMarcha", "", v, sizeof v, g_inipath);
    if (!v[0]) { WritePrivateProfileStringA("UpgradeAll", "PularExercitosEmMarcha", "1", g_inipath); strcpy(v, "1"); }
    g_upg_all_idle = atoi(v) != 0;
}

static void upg_install(HINSTANCE h) {
    upg_read_ini();
    wchar_t dir[MAX_PATH], src[MAX_PATH], dst[MAX_PATH], ui[MAX_PATH];
    GetModuleFileNameW(h, dir, MAX_PATH); { wchar_t *q = wcsrchr(dir, L'\\'); if (q) q[1] = 0; }
    _snwprintf(ui, MAX_PATH, L"%lsAraQoL_UI", dir); CreateDirectoryW(ui, NULL);   // copias geradas (so para conferencia)
    _snwprintf(src, MAX_PATH, L"%ls..\\assets\\UI\\IndusPopups.xml", dir); _snwprintf(dst, MAX_PATH, L"%ls\\IndusPopups.xml", ui);
    if (build_popups(src, dst)) g_xml_popups = read_all(dst, &g_xml_popups_len);
    _snwprintf(src, MAX_PATH, L"%ls..\\assets\\UI\\MilitaryPanel.xml", dir); _snwprintf(dst, MAX_PATH, L"%ls\\MilitaryPanel.xml", ui);
    if (build_military(src, dst)) g_xml_military = read_all(dst, &g_xml_military_len);
    if (!g_xml_popups && !g_xml_military) return;
    g_bind_popups_tramp = (binder_fn)hookn(RVA_BIND_POPUPS, k_pro_bind, 20, (void *)hook_bind_popups, "IndusPopups");
    g_bind_military_tramp = (binder_fn)hookn(RVA_BIND_MILITARY, k_pro_bind, 20, (void *)hook_bind_military, "MilitaryPanel");
    if (!g_bind_popups_tramp) g_xml_popups = NULL;
    if (!g_bind_military_tramp) g_xml_military = NULL;
    if (!g_xml_popups && !g_xml_military) return;
    g_parse_tramp = (parse_fn)hookn(RVA_UI_PARSE, k_pro_parse, 24, (void *)hook_parse, "analise do XML");
    if (!g_parse_tramp) { g_xml_popups = g_xml_military = NULL; return; }
    g_upg_ok = 1;
    qlog("UpgradeAll: instalado (painel de upgrade: %s; painel Militar: %s; pular exercitos em marcha: %d; idioma %s: \"%s\" / \"%s\").",
         g_xml_popups ? "sim" : "nao", g_xml_military ? "sim" : "nao", g_upg_all_idle, g_locale, g_txt_panel, g_txt_military);
}

static DWORD WINAPI worker(LPVOID p) {
    (void)p;
    lookup_t lookup = (lookup_t)(g_base + RVA_LOOKUP);
    float zmax = -1, hmax = -1; ULONGLONG lastlog = 0, lastcheck = 0;
    int warned_cam = 0, warned_ovl = 0;
    for (;;) {
        Sleep(30);
        { static ULONGLONG li = 0; ULONGLONG t = GetTickCount64(); if (t - li > 2000) { read_ini(); li = t; } }
        apply_pick_pad();
        if (!mem_ok(g_base + RVA_DATALIB_PTR, 8, 0) || !*(void **)(g_base + RVA_DATALIB_PTR)) continue;

        // 1. Camera: copia viva (verificada) - aplicada a cada 250 ms, o jogo pode recopiar os dados.
        float *live = (float *)(g_base + RVA_CAM_MULT);
        float want = g_cam_zoom ? CAM_MULT_NEW : CAM_MULT_ORIG;
        if ((*live == CAM_MULT_ORIG || *live == CAM_MULT_NEW) && *live != want
            && F(RVA_CAM_MULT - 0x50) == 400.0f && F(RVA_CAM_MULT - 0x4c) == 0.9f) {
            *live = want;
            qlog("Camera: limite da regiao -> %.0f (%s).", want, g_cam_zoom ? "teto agora = limite do tamanho do mapa" : "teto original do jogo");
            lastcheck = 0;
        }

        ULONGLONG now = GetTickCount64();
        if (now - lastcheck > 5000) {
            lastcheck = now;
            float *cam = (float *)lookup(H_CAMDEF, H_WORLDVIEW, H_CAMERA);
            if (cam_ok(cam)) {
                if (cam[0x90/4] != want) { cam[0x90/4] = want; qlog("Camera: dado original (CamSettings) = %.0f.", want); }
            } else if (!warned_cam && cam && mem_ok(cam, 0xa0, 0) && cam[0x40/4] != 0) {
                qlog("Camera: tabela com valores inesperados (%.1f %.3f %.1f %.1f %.2f). Dado original nao alterado.",
                     cam[0x40/4], cam[0x44/4], cam[0x30/4], cam[0x3c/4], cam[0x90/4]); warned_cam = 1;
            }
            float *def = (float *)lookup(H_OVERLAYDEF, H_WORLDVIEW, H_OVERLAYSET);
            if (ovl_ok(def)) {
                float *h = def + OFF_HIDE_ABOVE / 4; int n = 0;
                for (size_t k = 0; k < sizeof k_banner_types / sizeof *k_banner_types; k++)
                    if (h[k_banner_types[k]] != HIDE_ALWAYS) { h[k_banner_types[k]] = HIDE_ALWAYS; n++; }
                if (n) qlog("Bandeiras: %d tipos ajustados. Opcao desligada = escondida em qualquer zoom; ligada = sempre visivel.", n);
            } else if (!warned_ovl && def && mem_ok(def, 0x140, 0) && def[0x50/4] != 0) {
                const float *h = def + OFF_HIDE_ABOVE / 4;
                qlog("Bandeiras: tabela com valores inesperados (%.2f %.2f %.2f). Nada alterado.", h[1], h[3], h[6]); warned_ovl = 1;
            }
        }

        // Diagnostico leve.
        float z = F(RVA_ZOOM), hgt = F(RVA_CAMH);
        if (z > zmax) zmax = z;
        if (hgt > hmax) hmax = hgt;
        if (now - lastlog > 60000) {
            unsigned char *st = g_base + RVA_BANNER_STATE;
            qlog("zoom %.3f (max %.3f) | altura %.0f (max %.0f) | estrategica a=%.2f | recursos a=%.2f | forcas a=%.2f | melhorias a=%.2f",
                 z, zmax, hgt, hmax, *(float *)(st + 16 * 8), *(float *)(st + 8), *(float *)(st + 48), *(float *)(st + 24));
            lastlog = now;
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(h);
    GetModuleFileNameA(h, g_logpath, MAX_PATH);
    char *s = strrchr(g_logpath, '\\'); strcpy(s ? s + 1 : g_logpath, "AraQoL.log");
    strcpy(g_inipath, g_logpath); { char *q = strrchr(g_inipath, '.'); if (q) strcpy(q, ".ini"); }
    char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
    const char *en = strrchr(exe, '\\'); en = en ? en + 1 : exe;
    if (_stricmp(en, "AraHistoryUntold.exe") != 0) return TRUE;
    g_base = (unsigned char *)GetModuleHandleA(NULL);
    qlog("=== AraQoL 2.0 carregado (camera + bandeiras + clique nas unidades + upgrade all; CPU/prioridade agora no ARA CPU Fix)");
    if (!verify_target()) return TRUE;
    upg_install(h);
    HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (t) CloseHandle(t);
    return TRUE;
}
