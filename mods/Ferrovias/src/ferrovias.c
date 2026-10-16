// Ferrovias 1.9 - mod independente de ferrovias para ARA: History Untold 2.1.0.542.
// Carregado como proxy de avformat-60.dll (o original fica como avformat-60_orig.dll). Dados em Mods\Ferrovias
// (modelo rdt_Railroad), que precisa estar ativo no jogo novo. Log em bin\Ferrovias.log, opcoes em bin\Ferrovias.ini.
// Nao depende do AraQoL nem do mod Brasil; ganchos compartilhados (analise do XML) sao encadeados (ara_hook.h).
//
// Uma ferrovia e uma estrada do jogo com o modelo rdt_Railroad. O jogo guarda o ID do modelo no objeto da estrada
// (+0x10), cobra o ItemCost do modelo em cada regiao e salva tudo sozinho. A DLL so:
//  * poe o botao "FERROVIA" ao lado das abas da tela de estradas (UI\RoadManagementScreen.xml, so na memoria);
//    marcado (aba Construir, com tch_SteamEngine) = a tela usa rdt_Railroad no lugar do modelo da era (tela+0xcec);
//  * libera a construcao por cima de estrada pronta quando o modelo e rdt_Railroad. O jogo normalmente pula regiao
//    com estrada (contagem da tela 0x8d6832 e ordem por regiao 0xe4aea7); a troca em si (0xe48230) ja e do jogo.
//    Estrada de IA/jogador continua sem poder passar por cima de trilho.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define EXPECTED_TIMESTAMP   0x6a286b53u
#define EXPECTED_SIZEOFIMAGE 0x5e622e00u

#define RAIL_ID           0xbba00679u   // mm3("rdt_Railroad") = ID do modelo
#define RAIL_TECH         0xa392661eu   // mm3("tch_SteamEngine")
#define H_ROADTPL_TYPE    0xc1710a37u   // mm3("RoadTypes.RoadTemplate")
#define H_ROADTPL_GROUP   0xe76270bbu   // mm3("RoadTemplates")
#define RVA_GC_PTR        0x23e37650ull // ponteiro global do GameCore
#define RVA_HASH          0x00a5c1a0ull // hash de nome (str, len)
#define RVA_UI_PARSE      0x00c2b8c0ull // analisa o XML de um contexto de UI: (ctx, buffer)
#define RVA_ROAD_SCREEN   0x069d9cd0ull // objeto da tela de estradas: [0] contexto da UI, +8 modo (0 construir, 1 reparar)
#define SCR_MODE          0x8
#define SCR_FLAGS         0xce8         // u16 de "sujo"; bit 8 = tem ponto inicial
#define SCR_TPL           0xcec         // modelo usado pela tela
#define SCR_START         0xcf0         // inicio/fim (u32 x2)
#define RVA_ROAD_BINDER   0x0120b920ull // liga os controles do RoadManagementScreen.xml (contexto)
#define RVA_ROAD_OPEN     0x008d3e00ull // abre a tela (tela, modo): grava o modelo da era em +0xcec
#define RVA_TPL_FOR_ACT   0x00d95d00ull // (regras, ato) -> modelo de estrada da era
#define RVA_RULES         0x24c30100ull
#define RVA_DATA_GET      0x00cec5f0ull // (&saida, tipoHash, grupoHash, id) -> char (mesma chamada do jogo em 0xe4828c)
#define RVA_BIND_CHECKBOX 0x00ca6e00ull // (contexto, &nome, &saida) -> CheckBox (tipo 0xe)
#define RVA_SET_CHECKED   0x00c445e0ull // (checkbox, bool); marcado = bit 33 de +0x38
#define RVA_ROAD_INDEX    0x002c9250ull // (mapa de handles, handle) -> indice (-1 = procurar na parte linear)
#define SITE_UI_COUNT     0x008d6832ull // contagem de regioes a construir na tela
#define SITE_ORDER        0x00e4aea7ull // ordem por regiao (0xe4ae70)
#define RVA_BIND_STACK    0x007c5100ull // (contexto, &nome, &saida) -> Stack (tipo 3)
#define RVA_MUX_SELECT    0x00c37560ull // (mux, filho): mostra so o filho (vtbl+0x88 = esconder nos outros)
#define RVA_BUILD_REFRESH 0x008d5c40ull // atualiza a tela no modo Construir (titulo, subtitulo) quando ha mudanca
#define RVA_LOCALIZE      0x00ad6420ull // (textos, saida, cap, &{chave, tam}, ...) -> tamanho do texto traduzido
#define RVA_KEY_POPUP     0x02c2c5a8ull // "TXT_ROAD_UI_POPUP_QUESTION_BUILD" (o jogo passa este ponteiro)
#define RVA_KEY_DESC_A    0x02c2c348ull // "TXT_ROAD_UI_TITLE_DESC_BUILD_START"
#define RVA_KEY_DESC_B    0x02c2c3c0ull // "TXT_ROAD_UI_TITLE_DESC_BUILD_END"
#define CTX_MODE_MUX      0x2f990       // controles ligados pelo binder no contexto da tela
#define RVA_SET_ATLAS_ID  0x00c34ae0ull // (imagem, hash do AtlasID): corpo de 0xc34ad0 (imagem, &{nome,tam,hash})
#define RVA_ATLAS_SET     0x3f6bb4c0ull // conjunto de atlas da UI; 0x168eec0(conjunto, &regiao[0x88], atlas, id) -> achou
#define RVA_ATLAS_FIND    0x0168eec0ull // (0xc12e50, chamada por 0xc34ae0, repassa atlas/id para ela)
#define RVA_SET_ATLAS     0x00c34a10ull // SetAtlas(imagem, &{nome,tam,hash}, 0): troca atlas E textura (0x74bced faz isso com "Items_160")
#define IMG_ATLAS         0x198         // hash do atlas da imagem (+0x19c = hash do AtlasID)
#define CTRL_CONTEXT      0x30          // contexto de UI dono do controle
#define SITE_MATERIAL     0x005c48c9ull // 0x5c45e0: hash do material do trecho em ecx, trecho (spline do WorldView, 0x50 B) em rsi
#define RAIL_MATERIAL     0x8cdcf442u   // mm3("Road_Railroad_Thick"): material de trilho do PathMaterial.csv que o jogo nunca usa
#define GC_CONN_COUNT     0x76f30       // ligacoes entre regioes (0x18 B): +4 tipo visual, +0xc/+0x10 as duas regioes
#define GC_CONNS          0x76f38
#define SPL_TYPE          0x28          // &0x3fff: 3 trilha, 4..10 estrada, 8 rodovia
#define SPL_CONN          0x44          // indice da ligacao (0x51ad85)
#define SPL_NPTS          0x08          // quantidade de pontos (0x5c4615)
#define SPL_PTS           0x10          // pontos de 0x10 B: x, y, altura (0x5c4666 / 0x5c4dc1)
#define OFF_BTN_CTX       0x130
#define OFF_BTN_FN        0x138
#define GC_REGION_ROADS   0x76f48       // vetor por regiao (0xc B): [0] estado (1 pronta, 2 destruida), +8 handle
#define GC_ROAD_MAP       0x76f80
#define GC_ROAD_POOL      0x76fa8       // objetos de 0x30 B, +0x10 ID do modelo
#define GC_ACT            0x788fc
#define GC_LOCAL_NATION   0x78090
#define GC_NATION_COUNT   0x73278
#define GC_NATIONS        0x73280
#define NATION_SIZE       0xbec0
#define NATION_TECH_N     0xa830        // techs pesquisadas: u32 qtd, u32* IDs (confirmado no jogo em 12/10)
#define NATION_TECH_P     0xa838

typedef struct { const char *p; unsigned n; unsigned h; } gstr_t;
typedef unsigned long long (*bind_fn)(void *, gstr_t *, void **);
typedef unsigned (*hash_fn)(const char *, unsigned);
typedef void (*binder_fn)(void *);
typedef void (*parse_fn)(unsigned char *, long long *);
typedef unsigned (*tpl_act_fn)(void *, unsigned char);
typedef char (*data_get_fn)(void **, unsigned, unsigned, unsigned);
typedef void (*set_checked_fn)(void *, char);
typedef unsigned (*road_index_fn)(void *, int);
typedef unsigned (*road_open_fn)(unsigned char *, int);
typedef void (*set_hidden_fn)(void *, char, int);
typedef void (*mux_fn)(void *, void *);
typedef void (*refresh_fn)(unsigned char *);
typedef struct { const char *p; unsigned n; } kstr_t;
typedef void (*atlas_id_fn)(unsigned char *, unsigned);
typedef void (*set_atlas_fn)(unsigned char *, gstr_t *, char);
typedef char (*atlas_find_fn)(void *, void *, unsigned, unsigned);
typedef unsigned (*loc_fn)(void *, char *, unsigned, kstr_t *, void *, void *, void *, void *);

static unsigned char *g_base;
static char g_logpath[MAX_PATH], g_inipath[MAX_PATH], g_locale[16] = "en";

static void qlog(const char *fmt, ...) {
    FILE *f = fopen(g_logpath, "a"); if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}
#define hook_log qlog
#include "../../../common/ara_hook.h"

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
    return 1;
}
static unsigned char *gc(void) { unsigned char **pp = (unsigned char **)(g_base + RVA_GC_PTR); return mem_ok(pp, 8, 0) ? *pp : NULL; }

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
static int memmem_ok(const char *b, size_t n, const char *pat) {
    size_t pl = strlen(pat);
    for (size_t i = 0; i + pl <= n; i++) if (!memcmp(b + i, pat, pl)) return 1;
    return 0;
}
static int repl_once(char **buf, size_t *len, const char *from, const char *to, const char *after) {
    char *a = after ? strstr(*buf, after) : *buf; if (!a) return 0;
    char *p = strstr(a, from); if (!p) return 0;
    size_t fl = strlen(from), tl = strlen(to), off = p - *buf;
    char *nb = (char *)realloc(*buf, *len + tl + 1); if (!nb) return 0;
    *buf = nb; p = nb + off;
    memmove(p + tl, p + fl, *len - off - fl + 1); memcpy(p, to, tl); *len += tl - fl; return 1;
}
static size_t line_end(const char *buf, size_t pos) { const char *e = strchr(buf + pos, '\n'); return e ? (size_t)(e - buf) + 1 : strlen(buf); }
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
// Idioma do jogo (Locale= em %LOCALAPPDATA%\Ara History Untold\Settings_v121.txt).
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

// --- Analise do XML: entrega o RoadManagementScreen.xml com o botao ------------------------------------------------
// 0xc2b8c0(ctx, buf): caminho em ctx+0x168/+0x170; buf = {u64 tamanho, u64 cap, char* dados, alocador}.
static char *g_xml_road; static size_t g_xml_road_len;
static parse_fn g_parse_prev;
static void hook_parse(unsigned char *ctx, long long *buf) {
    const char *p = *(const char **)(ctx + 0x168); unsigned n = *(unsigned *)(ctx + 0x170);
    if (g_xml_road && p && n > 4 && n < 512 && mem_ok(p, n, 0) && ends_with_ci(p, n, "ui/roadmanagementscreen.xml")) {
        char *copy = (char *)malloc(g_xml_road_len + 1);   // a analise guarda ponteiros para o texto: nunca liberada
        if (copy) {
            memcpy(copy, g_xml_road, g_xml_road_len); copy[g_xml_road_len] = 0;
            long long fake[4] = { (long long)g_xml_road_len, (long long)g_xml_road_len + 1, (long long)copy, buf ? buf[3] : 0 };
            qlog("Ferrovia: %.*s carregado com o botao (%u bytes; original %lld).", (int)n, p, (unsigned)g_xml_road_len, buf ? buf[0] : -1);
            g_parse_prev(ctx, fake);
            return;
        }
    }
    g_parse_prev(ctx, buf);
}

// --- Regras ----------------------------------------------------------------------------------------------------
static char g_txt_rail[64] = "RAILROAD";
static int g_rail_data;
static void *volatile g_rail_ctrl, *volatile g_rail_title;
static binder_fn g_road_binder_tramp;
static road_open_fn g_road_open_tramp;

static unsigned char *road_screen(void) { return g_base + RVA_ROAD_SCREEN; }
static int rail_checked(void) {
    void *c = g_rail_ctrl;
    return c && mem_ok((unsigned char *)c + 0x38, 8, 0) && ((*(unsigned long long *)((unsigned char *)c + 0x38) >> 33) & 1);
}
static int rail_has_tech(unsigned char *g, unsigned *count) {
    *count = 0;
    if (!g) return 0;
    unsigned nat = *(unsigned *)(g + GC_LOCAL_NATION);
    if (nat >= *(unsigned *)(g + GC_NATION_COUNT)) return 0;
    unsigned char *n = *(unsigned char **)(g + GC_NATIONS) + (size_t)nat * NATION_SIZE;
    if (!mem_ok(n + NATION_TECH_N, 16, 0)) return 0;
    unsigned cnt = *(unsigned *)(n + NATION_TECH_N); unsigned *ids = *(unsigned **)(n + NATION_TECH_P);
    *count = cnt;
    if (!cnt || cnt > 4096 || !mem_ok(ids, cnt * 4, 0)) return 0;
    for (unsigned i = 0; i < cnt; i++) if (ids[i] == RAIL_TECH) return 1;
    return 0;
}
static unsigned rail_normal_tpl(unsigned char *g) {
    return ((tpl_act_fn)(g_base + RVA_TPL_FOR_ACT))(g_base + RVA_RULES, *(unsigned char *)(g + GC_ACT));
}
// Modelo da estrada pelo handle (mesma busca de 0xe49860).
static unsigned road_tpl(unsigned char *g, int handle) {
    unsigned i = ((road_index_fn)(g_base + RVA_ROAD_INDEX))(g + GC_ROAD_MAP, handle);
    int *arr = *(int **)(g + GC_ROAD_MAP + 0x18);
    if (i == 0xffffffffu) {
        unsigned s = *(unsigned *)(g + GC_ROAD_MAP + 8), e = s + *(unsigned *)(g + GC_ROAD_MAP);
        for (unsigned k = s; k < e; k++) if (arr[k * 2] == handle) { i = k; break; }
        if (i == 0xffffffffu) return 0xffffffffu;
    }
    unsigned char *o = *(unsigned char **)(g + GC_ROAD_POOL) + (size_t)(unsigned)arr[i * 2 + 1] * 0x30;
    return mem_ok(o + 0x10, 4, 0) ? *(unsigned *)(o + 0x10) : 0xffffffffu;
}
// 1 = regiao entra na obra. Original: sem estrada ou estrada destruida. Ferrovia: tambem estrada pronta que nao e trilho.
static int need_build(unsigned char *g, unsigned region, unsigned tpl) {
    unsigned char *e = *(unsigned char **)(g + GC_REGION_ROADS) + (size_t)region * 0xc;
    int h = *(int *)(e + 8);
    if (h == -1 || e[0] == 2) return 1;
    if (tpl != RAIL_ID || !g_rail_data || e[0] != 1) return 0;
    return road_tpl(g, h) != RAIL_ID;
}
// Deixa a tela coerente com o botao: marcado + aba Construir + tech + dados = modelo de ferrovia.
static void rail_sync(unsigned char *g) {
    unsigned char *s = road_screen(); unsigned n;
    int want = g_rail_data && rail_checked() && *(int *)(s + SCR_MODE) == 0 && rail_has_tech(g, &n);
    unsigned cur = *(unsigned *)(s + SCR_TPL);
    if (want && cur != RAIL_ID) *(unsigned *)(s + SCR_TPL) = RAIL_ID;
    else if (!want && cur == RAIL_ID) *(unsigned *)(s + SCR_TPL) = rail_normal_tpl(g);
}
static int __cdecl need_ui(unsigned region, unsigned char *g) {
    static unsigned last; unsigned now = GetTickCount();
    if (now - last > 200) { rail_sync(g); last = now; }
    return need_build(g, region, *(unsigned *)(road_screen() + SCR_TPL));
}
static int __cdecl need_order(unsigned region, unsigned char *g, unsigned tpl) {
    int r = need_build(g, region, tpl);
    if (tpl == RAIL_ID) qlog("Ferrovia: ordem na regiao %u -> %s.", region, r ? "obra criada" : "ja tem trilho/obra, pulada");
    return r;
}

static void cb_rail_check(unsigned char *scr, unsigned char *ev) {
    if (!mem_ok(ev + 0x38, 4, 0) || *(int *)(ev + 0x38) != 0) return;
    *(unsigned long long *)(scr + SCR_START) = ~0ull;            // como as abas: limpa inicio/fim
    *(unsigned short *)(scr + SCR_FLAGS) = (*(unsigned short *)(scr + SCR_FLAGS) & 0xfff7) | 4;   // 4 = atualizar como troca de aba
    unsigned char *g = gc(); if (!g) return;
    rail_sync(g);
    qlog("Ferrovia: botao %s (modelo da tela %08x).", rail_checked() ? "marcado" : "desmarcado", *(unsigned *)(scr + SCR_TPL));
}
// O binder roda muitas vezes por abertura: liga sempre (barato) e so registra quando o controle muda.
static void hook_road_binder(void *ctx) {
    g_road_binder_tramp(ctx);
    gstr_t s = { "AraRailCheck", 12, ((hash_fn)(g_base + RVA_HASH))("AraRailCheck", 12) };
    void *c = NULL;
    ((bind_fn)(g_base + RVA_BIND_CHECKBOX))(ctx, &s, &c);
    if (!c || !mem_ok((unsigned char *)c + OFF_BTN_CTX, 16, 1)) {
        static int miss; if (miss++ < 3) qlog("Ferrovia: botao nao encontrado na tela de estradas.");
        return;
    }
    *(void **)((unsigned char *)c + OFF_BTN_CTX) = road_screen();
    *(void **)((unsigned char *)c + OFF_BTN_FN) = (void *)cb_rail_check;
    gstr_t ts = { "AraRailMode", 11, ((hash_fn)(g_base + RVA_HASH))("AraRailMode", 11) };
    void *t = NULL;
    ((bind_fn)(g_base + RVA_BIND_STACK))(ctx, &ts, &t);
    g_rail_title = t;
    if (g_rail_ctrl != c) { g_rail_ctrl = c; qlog("Ferrovia: botao ligado (%p, contexto %p; titulo %p).", c, ctx, t); }
}
static unsigned hook_road_open(unsigned char *scr, int mode) {
    unsigned r = g_road_open_tramp(scr, mode);
    unsigned char *g = gc(); void *c = g_rail_ctrl;
    if (!g || !c) return r;
    unsigned ntech; int tech = rail_has_tech(g, &ntech);
    unsigned normal = *(unsigned *)(scr + SCR_TPL);
    void *dummy = NULL;
    if (!g_rail_data) g_rail_data = ((data_get_fn)(g_base + RVA_DATA_GET))(&dummy, H_ROADTPL_TYPE, H_ROADTPL_GROUP, RAIL_ID) && dummy;
    int show = tech && g_rail_data;
    if (!show && rail_checked()) ((set_checked_fn)(g_base + RVA_SET_CHECKED))(c, 0);
    void **vt = *(void ***)c;
    if (mem_ok(vt, 0x90, 0)) ((set_hidden_fn)vt[0x88 / 8])(c, show ? 0 : 1, 0);
    rail_sync(g);
    static int logs;
    if (logs++ < 20)
        qlog("Ferrovia: tela aberta (modo %d). Modelo da era %08x; rdt_Railroad %s; tch_SteamEngine %s (%u techs); botao %s.",
             mode, normal, g_rail_data ? "carregado" : "AUSENTE (ative o mod Ferrovias num jogo novo)", tech ? "sim" : "nao", ntech,
             show ? "visivel" : "escondido");
    return r;
}

// --- Textos da tela com a ferrovia marcada -----------------------------------------------------------------------
// Subtitulo e pergunta de confirmacao sao montados pelo codigo (0xad6420 com o ponteiro da chave): com a ferrovia
// ativa, a DLL troca o texto traduzido pela versao de ferrovia (texto oficial de cada idioma com o substantivo trocado).
// O titulo e fixo no XML: a DLL acrescenta um terceiro titulo (AraRailMode) no ModeMux e o seleciona.
// /c:Gold/.../r/ = marcacao de cor do proprio jogo (removida se o jogo ja entregar o texto sem ela).
static const struct { const char *loc, *title, *start, *end, *popup, *legend; } k_rail_ui[] = {
    { "en", "BUILD RAILROADS", "Select a /c:Gold/connection point/r/ to start building a new railroad",
      "Select another /c:Gold/connection point/r/ to end this railroad", "Build this railroad?", "Railroad" },
    { "pt", "CONSTRUIR FERROVIAS", "Selecione um /c:Gold/ponto de conexão/r/ para começar a construir uma nova ferrovia",
      "Selecione outro /c:Gold/ponto de conexão/r/ para terminar esta ferrovia", "Construir esta ferrovia?", "Ferrovia" },
    { "es", "CONSTRUIR FERROCARRILES", "Selecciona un /c:Gold/punto de conexión/r/ para empezar a construir un nuevo ferrocarril.",
      "Selecciona otro /c:Gold/punto de conexión/r/ para finalizar este ferrocarril.", "¿Construir este ferrocarril?", "Ferrocarril" },
    { "fr", "CONSTRUIRE DES CHEMINS DE FER", "Sélectionnez un /c:Gold/point de connexion/r/ pour commencer la construction d'un nouveau chemin de fer.",
      "Sélectionnez un autre /c:Gold/point de connexion/r/ pour terminer ce chemin de fer.", "Construire ce chemin de fer ?", "Chemin de fer" },
    { "de", "EISENBAHNEN BAUEN", "Wähle einen /c:Gold/connection point/r/, um von dort aus eine neue Eisenbahn zu bauen.",
      "Wähle einen weiteren /c:Gold/connection point/r/, um diese Eisenbahn dort zu beenden.", "Diese Eisenbahn bauen?", "Eisenbahn" },
    { "it", "COSTRUISCI FERROVIE", "Seleziona un /c:Gold/punto di collegamento/r/ per iniziare a costruire una nuova ferrovia",
      "Seleziona un altro /c:Gold/punto di collegamento/r/ per terminare questa ferrovia", "Costruire questa ferrovia?", "Ferrovia" },
    { "pl", "BUDOWA KOLEI", "Wybierz /c:Gold/punkt połączenia/r/, aby rozpocząć budowę nowej linii kolejowej",
      "Wybierz inny /c:Gold/punkt połączenia/r/, aby zakończyć tę linię kolejową", "Zbudować tę linię kolejową?", "Kolej" },
    { "ru", "ПОСТРОЙТЕ ЖЕЛЕЗНЫЕ ДОРОГИ", "Выберите /c:Gold/точку соединения/r/, чтобы начать строительство новой железной дороги",
      "Выберите другую /c:Gold/точку соединения/r/, чтобы завершить строительство железной дороги.", "Построить железную дорогу?", "Железная дорога" },
};
static int g_ui_lang = -1;           // indice em k_rail_ui; -1 = idioma sem traducao: textos do jogo ficam como estao
static loc_fn g_loc_tramp;
static refresh_fn g_refresh_tramp;

static int rail_active(void) {
    unsigned char *s = road_screen();
    return *(unsigned *)(s + SCR_TPL) == RAIL_ID && *(int *)(s + SCR_MODE) == 0;
}
static unsigned hook_localize(void *db, char *out, unsigned cap, kstr_t *key, void *a5, void *a6, void *a7, void *a8) {
    unsigned r = g_loc_tramp(db, out, cap, key, a5, a6, a7, a8);
    if (g_ui_lang < 0 || !key) return r;
    const char *k = key->p, *t;
    if (k == (const char *)(g_base + RVA_KEY_POPUP)) t = k_rail_ui[g_ui_lang].popup;
    else if (k == (const char *)(g_base + RVA_KEY_DESC_A)) t = k_rail_ui[g_ui_lang].start;
    else if (k == (const char *)(g_base + RVA_KEY_DESC_B)) t = k_rail_ui[g_ui_lang].end;
    else return r;
    if (!rail_active() || !out || cap < 2) return r;
    int markup = r < cap && memmem_ok(out, r, "/c:");
    unsigned n = 0;
    for (const char *q = t; *q && n + 1 < cap; q++) {
        if (!markup && *q == '/' && (q[1] == 'c' || q[1] == 'r')) {          // tira /c:Cor/ e /r/
            const char *e = strchr(q + 1, '/'); if (e) { q = e; continue; }
        }
        out[n++] = *q;
    }
    out[n] = 0;
    return n;
}
// Depois que o jogo atualiza o modo Construir, mostra o titulo de ferrovia se ela estiver ativa.
static void hook_build_refresh(unsigned char *scr) {
    g_refresh_tramp(scr);
    void *t = g_rail_title; unsigned char *ctx = *(unsigned char **)scr;
    if (!t || !ctx || !rail_active()) return;
    void *mux = *(void **)(ctx + CTX_MODE_MUX);
    if (mux) ((mux_fn)(g_base + RVA_MUX_SELECT))(mux, t);
}

// --- Icones da lista de custo ---------------------------------------------------------------------------------
// A linha da lista (PopupItemEntry) usa o AtlasID do item no atlas HUDResourceIcons_V2. Materials (Stone) e Timber (Wood)
// tem icone de recurso ali; Iron e Steel nao (NULL REF). Na tela de estradas, so o AtlasID que NAO existe no atlas do
// HUD passa para o atlas de itens (Items_160); quando a linha e reaproveitada para um recurso do HUD, volta.
static atlas_id_fn g_atlas_tramp;
static unsigned g_h_hud, g_h_items;
static int atlas_has(unsigned atlas, unsigned id) {
    unsigned long long reg[24] = { 0 };   // 0x88 bytes da regiao do icone (so interessa se achou)
    return ((atlas_find_fn)(g_base + RVA_ATLAS_FIND))(g_base + RVA_ATLAS_SET, reg, atlas, id) != 0;
}
// Imagens trocadas por nos: guardam o atlas original para voltar quando a linha e reaproveitada.
static void *volatile g_sw_img[32]; static unsigned g_sw_atlas[32];
static int switched_slot(void *img) { for (int i = 0; i < 32; i++) if (g_sw_img[i] == img) return i; return -1; }
// Uma vez: Steel/Iron/Wood/Stone existem no atlas do HUD e no de itens? (diagnostico)
static void atlas_diag(unsigned cur) {
    static int done; if (done) return; done = 1;
    static const struct { const char *nm; unsigned id; } w[] = { { "Steel", 0xd08fec78u }, { "Iron", 0x1fb9379du }, { "Wood", 0xeb213538u }, { "Stone", 0x10f7ee55u } };
    for (int k = 0; k < 4; k++)
        qlog("Ferrovia: icone %s: atlas da imagem %08x=%d, HUD %08x=%d, itens %08x=%d", w[k].nm, cur, atlas_has(cur, w[k].id),
             g_h_hud, atlas_has(g_h_hud, w[k].id), g_h_items, atlas_has(g_h_items, w[k].id));
}
// Vale para qualquer tela (a lista de custo aparece no popup E no painel "Cost (N Regions)"): so troca quando o icone
// NAO existe no atlas atual da imagem (daria NULL REF) e existe no atlas de itens. Nao depende do hash do atlas atual.
// So o hash do atlas e usado por SetAtlas (le +0xc da string).
static void set_atlas(unsigned char *img, unsigned atlas) {
    gstr_t g = { "", 0, atlas };
    ((set_atlas_fn)(g_base + RVA_SET_ATLAS))(img, &g, 0);
}
static void hook_atlas_id(unsigned char *img, unsigned id) {
    if (id) {
        unsigned orig = *(unsigned *)(img + IMG_ATLAS);
        int sl = switched_slot(img);
        if (sl >= 0) {                                         // linha reaproveitada: volta ao atlas do XML
            orig = g_sw_atlas[sl]; g_sw_img[sl] = NULL;
            if (atlas_has(orig, id)) set_atlas(img, orig);
        }
        if (orig != g_h_items && !atlas_has(orig, id)) {
            if (id == 0xd08fec78u || id == 0x1fb9379du) atlas_diag(orig);   // Steel/Iron: mostra uma vez
            if (atlas_has(g_h_items, id)) {
                static unsigned k; unsigned i = k++ & 31;
                g_sw_img[i] = img; g_sw_atlas[i] = orig;
                if (*(unsigned *)(img + IMG_ATLAS) != g_h_items) set_atlas(img, g_h_items);
                static int logs; if (logs++ < 6) qlog("Ferrovia: icone %08x ausente no atlas %08x -> atlas de itens.", id, orig);
            } else {
                static int miss; if (miss++ < 6) qlog("Ferrovia: icone %08x ausente no atlas %08x e no de itens.", id, orig);
            }
        }
    }
    g_atlas_tramp(img, id);
}
// O inicio de 0xc34ae0 tem um jne relativo no byte 12: trampolim proprio com o salto convertido para absoluto.
static const unsigned char k_pro_atlas[18] = { 0x40,0x55, 0x56, 0x48,0x83,0xec,0x68, 0x48,0x8b,0xf1, 0x85,0xd2, 0x0f,0x85,0x95,0x00,0x00,0x00 };
static void *hook_atlas_install(void) {
    unsigned char *t = g_base + RVA_SET_ATLAS_ID;
    if (memcmp(t, k_pro_atlas, 18)) { qlog("Ferrovia: inicio de SetAtlasID nao confere (ja enganchado por outro mod?); icones ficam como estao."); return NULL; }
    unsigned char *tr = (unsigned char *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) return NULL;
    unsigned char *jne_to = t + 18 + 0x95;                    // destino do jne original
    int i = 0;
    memcpy(tr, t, 12); i = 12;
    tr[i++] = 0x75; tr[i++] = 14;                              // jne +14 (pula o primeiro jmp absoluto)
    tr[i++] = 0xFF; tr[i++] = 0x25; *(unsigned *)(tr + i) = 0; i += 4; *(unsigned long long *)(tr + i) = (unsigned long long)(t + 18); i += 8;
    tr[i++] = 0xFF; tr[i++] = 0x25; *(unsigned *)(tr + i) = 0; i += 4; *(unsigned long long *)(tr + i) = (unsigned long long)jne_to; i += 8;
    FlushInstructionCache(GetCurrentProcess(), tr, 64);
    DWORD old; if (!VirtualProtect(t, 18, PAGE_EXECUTE_READWRITE, &old)) return NULL;
    unsigned char j[18]; memset(j, 0x90, sizeof j);
    j[0] = 0xFF; j[1] = 0x25; memset(j + 2, 0, 4); *(unsigned long long *)(j + 6) = (unsigned long long)hook_atlas_id;
    memcpy(t, j, 18); VirtualProtect(t, 18, old, &old); FlushInstructionCache(GetCurrentProcess(), t, 18);
    return tr;
}

// --- Desenho da ferrovia no mapa ----------------------------------------------------------------------------------
// O jogo desenha cada ligacao entre duas regioes com um material do PathMaterial.csv escolhido pela cultura/era
// (0x5c45e0). Se as DUAS regioes da ligacao tem ferrovia pronta (modelo rdt_Railroad, estado 1), a DLL troca so esse
// trecho para Road_Railroad_Thick (trilhos com dormentes, arte do proprio jogo). Estradas comuns nao mudam.
// Roda no desenho de cada trecho: sem VirtualQuery aqui (indices validados pelas contagens do proprio jogo).
static unsigned road_tpl_fast(unsigned char *g, int handle) {
    unsigned i = ((road_index_fn)(g_base + RVA_ROAD_INDEX))(g + GC_ROAD_MAP, handle);
    int *arr = *(int **)(g + GC_ROAD_MAP + 0x18);
    if (i == 0xffffffffu) {
        unsigned s0 = *(unsigned *)(g + GC_ROAD_MAP + 8), e = s0 + *(unsigned *)(g + GC_ROAD_MAP);
        for (unsigned k = s0; k < e; k++) if (arr[k * 2] == handle) { i = k; break; }
        if (i == 0xffffffffu) return 0xffffffffu;
    }
    return *(unsigned *)(*(unsigned char **)(g + GC_ROAD_POOL) + (size_t)(unsigned)arr[i * 2 + 1] * 0x30 + 0x10);
}
static int region_has_rail(unsigned char *g, unsigned region) {
    unsigned char *e = *(unsigned char **)(g + GC_REGION_ROADS) + (size_t)region * 0xc;
    int h = *(int *)(e + 8);
    return h != -1 && e[0] == 1 && road_tpl_fast(g, h) == RAIL_ID;
}
// 1.2: ligacao com as duas pontas na MESMA regiao = rua interna (centro -> divisa, cidade, melhoria, estacao). Na 1.1 todas
// viravam trilho por cima do asfalto em toda regiao com ferrovia; na 1.2 nenhuma (sobravam so os pedacos curtos da divisa).
// 1.3: rua interna vira trilho se uma ponta dela coincide com uma ponta de um trecho com trilho ENTRE regioes.
// 1.4: o caminho dentro da regiao e uma CORRENTE de trechos de rodovia (tipo 8): divisa -> curva -> peca do centro -> divisa.
// Na 1.3 so o primeiro elo virava trilho e a peca do centro ficava asfalto. Agora todo trecho tipo 8 da regiao ligado
// (ponta com ponta) a um trecho com trilho entre regioes vira trilho; o elo que encosta numa divisa com vizinho SEM
// ferrovia continua estrada. Ruas tipo 4 (cidade, melhorias) so viram trilho se encostam direto no trilho entre regioes.
// As pontas dos trechos sao guardadas quando o jogo os desenha (o cache de 1 s reavalia depois).
// Ferrovias.ini [Desenho] TrilhoDentroDaRegiao=1 volta ao comportamento da 1.1 (todas as internas viram trilho).
#include "railbg.h"
static unsigned __cdecl rail_material(unsigned char *spl, unsigned hash) { return spline_rail(spl) ? RAIL_MATERIAL : hash; }
// 1.5: o jogo nao desenha trecho com o bit 0x100000 em +0x28 (0x5c462c), que aparece nos trechos dentro da area da cidade
// (com muralha): a saida do trilho da cidade sumia. Trecho de trilho com SO esse bit continua sendo desenhado.
static int __cdecl rail_force(unsigned char *spl) { return spline_rail(spl); }
// Desvio em 0x5c462c (test edx,110000h / jne 0x5c54c1 / cmp dword [rcx+1213e0h],0 -> segue em 0x5c463f)
static void *make_hidden_stub(void) {
    unsigned char *s = (unsigned char *)VirtualAlloc(NULL, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s) return NULL;
    int i = 0, j_vis, j_zero, j_test, j_skip;
    #define E(...) do { const unsigned char t_[] = { __VA_ARGS__ }; memcpy(s + i, t_, sizeof t_); i += sizeof t_; } while (0)
    E(0xf7,0xc2,0x00,0x00,0x11,0x00);                  // test edx,110000h
    E(0x74,0x00); j_vis = i - 1;                       // jz visivel
    E(0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53);   // push rax rcx rdx r8 r9 r10 r11
    E(0x89,0xd0, 0x25,0x00,0x00,0x11,0x00, 0x3d,0x00,0x00,0x10,0x00);   // mov eax,edx / and eax,110000h / cmp eax,100000h
    E(0x75,0x00); j_zero = i - 1;                      // jne zero (outro bit: o jogo decide)
    E(0x48,0x83,0xec,0x28, 0x48,0x89,0xf1, 0x48,0xb8); // sub rsp,28h / mov rcx,rsi / mov rax,imm64
    *(unsigned long long *)(s + i) = (unsigned long long)rail_force; i += 8;
    E(0xff,0xd0, 0x48,0x83,0xc4,0x28);                 // call rax / add rsp,28h
    E(0xeb,0x00); j_test = i - 1;                      // jmp teste
    s[j_zero] = (unsigned char)(i - j_zero - 1); E(0x31,0xc0);   // zero: xor eax,eax
    s[j_test] = (unsigned char)(i - j_test - 1); E(0x85,0xc0);   // teste: test eax,eax
    E(0x41,0x5b,0x41,0x5a,0x41,0x59,0x41,0x58,0x5a,0x59,0x58);   // pop r11 r10 r9 r8 rdx rcx rax (nao mexe nas flags)
    E(0x74,0x00); j_skip = i - 1;                      // jz pula (nao desenha)
    s[j_vis] = (unsigned char)(i - j_vis - 1);
    E(0x83,0xb9,0xe0,0x13,0x12,0x00,0x00);             // visivel: cmp dword [rcx+1213e0h],0
    E(0xff,0x25,0,0,0,0); *(unsigned long long *)(s + i) = (unsigned long long)(g_base + 0x5c463f); i += 8;
    s[j_skip] = (unsigned char)(i - j_skip - 1);
    E(0xff,0x25,0,0,0,0); *(unsigned long long *)(s + i) = (unsigned long long)(g_base + 0x5c54c1); i += 8;
    #undef E
    FlushInstructionCache(GetCurrentProcess(), s, 256);
    return s;
}
#include "desvio.h"
#include "alinha.h"
static const unsigned char k_site_hidden[19] = { 0xf7,0xc2,0x00,0x00,0x11,0x00, 0x0f,0x85,0x89,0x0e,0x00,0x00, 0x83,0xb9,0xe0,0x13,0x12,0x00,0x00 };
// Desvio em 0x5c48c9: guarda volateis e xmm0-5, ecx = rail_material(rsi, ecx), refaz as 4 instrucoes originais
// (mov edx,[rsi+28h] / mov eax,edx / and eax,3fffh / cmp eax,8) e o jne para 0x5c48df (senao 0x5c48d8).
static void *make_material_stub(void) {
    static const unsigned char pre[] = { 0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53, 0x48,0x81,0xec,0x88,0,0,0,
        0xf3,0x0f,0x7f,0x44,0x24,0x20, 0xf3,0x0f,0x7f,0x4c,0x24,0x30, 0xf3,0x0f,0x7f,0x54,0x24,0x40,
        0xf3,0x0f,0x7f,0x5c,0x24,0x50, 0xf3,0x0f,0x7f,0x64,0x24,0x60, 0xf3,0x0f,0x7f,0x6c,0x24,0x70,
        0x89,0xca, 0x48,0x89,0xf1 };                                   // edx = ecx (hash), rcx = rsi (trecho)
    static const unsigned char post[] = { 0x48,0x89,0x84,0x24,0xb0,0,0,0,  // rcx salvo = resultado
        0xf3,0x0f,0x6f,0x44,0x24,0x20, 0xf3,0x0f,0x6f,0x4c,0x24,0x30, 0xf3,0x0f,0x6f,0x54,0x24,0x40,
        0xf3,0x0f,0x6f,0x5c,0x24,0x50, 0xf3,0x0f,0x6f,0x64,0x24,0x60, 0xf3,0x0f,0x6f,0x6c,0x24,0x70,
        0x48,0x8d,0xa4,0x24,0x88,0,0,0, 0x41,0x5b,0x41,0x5a,0x41,0x59,0x41,0x58,0x5a,0x59,0x58,
        0x8b,0x56,0x28, 0x8b,0xc2, 0x25,0xff,0x3f,0x00,0x00, 0x83,0xf8,0x08, 0x75,0x0e };
    unsigned char *s = (unsigned char *)VirtualAlloc(NULL, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s) return NULL;
    int i = 0;
    memcpy(s + i, pre, sizeof pre); i += sizeof pre;
    s[i++] = 0xff; s[i++] = 0x15; *(unsigned *)(s + i) = 2; i += 4; s[i++] = 0xeb; s[i++] = 0x08;
    *(unsigned long long *)(s + i) = (unsigned long long)rail_material; i += 8;
    memcpy(s + i, post, sizeof post); i += sizeof post;
    s[i++] = 0xff; s[i++] = 0x25; *(unsigned *)(s + i) = 0; i += 4; *(unsigned long long *)(s + i) = (unsigned long long)(g_base + 0x5c48d8); i += 8;
    s[i++] = 0xff; s[i++] = 0x25; *(unsigned *)(s + i) = 0; i += 4; *(unsigned long long *)(s + i) = (unsigned long long)(g_base + 0x5c48df); i += 8;
    FlushInstructionCache(GetCurrentProcess(), s, 256);
    return s;
}
static const unsigned char k_site_material[15] = { 0x8b,0x56,0x28, 0x8b,0xc2, 0x25,0xff,0x3f,0x00,0x00, 0x83,0xf8,0x08, 0x75,0x07 };

// Desvio de uma checagem do jogo para need_*: guarda registradores volateis e xmm0-5, chama fn(args) e volta para
// 'yes' (regiao entra) ou 'no' (pula).
static void *make_stub(const unsigned char *args, int na, void *fn, unsigned char *yes, unsigned char *no) {
    static const unsigned char pre[] = { 0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53, 0x48,0x81,0xec,0x88,0,0,0,
        0xf3,0x0f,0x7f,0x44,0x24,0x20, 0xf3,0x0f,0x7f,0x4c,0x24,0x30, 0xf3,0x0f,0x7f,0x54,0x24,0x40,
        0xf3,0x0f,0x7f,0x5c,0x24,0x50, 0xf3,0x0f,0x7f,0x64,0x24,0x60, 0xf3,0x0f,0x7f,0x6c,0x24,0x70 };
    static const unsigned char post[] = { 0x85,0xc0,
        0xf3,0x0f,0x6f,0x44,0x24,0x20, 0xf3,0x0f,0x6f,0x4c,0x24,0x30, 0xf3,0x0f,0x6f,0x54,0x24,0x40,
        0xf3,0x0f,0x6f,0x5c,0x24,0x50, 0xf3,0x0f,0x6f,0x64,0x24,0x60, 0xf3,0x0f,0x6f,0x6c,0x24,0x70,
        0x48,0x8d,0xa4,0x24,0x88,0,0,0, 0x41,0x5b,0x41,0x5a,0x41,0x59,0x41,0x58,0x5a,0x59,0x58, 0x75,0x0e };
    unsigned char *s = (unsigned char *)VirtualAlloc(NULL, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s) return NULL;
    int i = 0;
    memcpy(s + i, pre, sizeof pre); i += sizeof pre;
    memcpy(s + i, args, na); i += na;
    s[i++] = 0xff; s[i++] = 0x15; *(unsigned *)(s + i) = 2; i += 4; s[i++] = 0xeb; s[i++] = 0x08;
    *(unsigned long long *)(s + i) = (unsigned long long)fn; i += 8;
    memcpy(s + i, post, sizeof post); i += sizeof post;
    s[i++] = 0xff; s[i++] = 0x25; *(unsigned *)(s + i) = 0; i += 4; *(unsigned long long *)(s + i) = (unsigned long long)no; i += 8;
    s[i++] = 0xff; s[i++] = 0x25; *(unsigned *)(s + i) = 0; i += 4; *(unsigned long long *)(s + i) = (unsigned long long)yes; i += 8;
    FlushInstructionCache(GetCurrentProcess(), s, 256);
    return s;
}
static int patch_site(unsigned long long rva, const unsigned char *orig, int n, void *stub, const char *what) {
    unsigned char *t = g_base + rva;
    if (memcmp(t, orig, n)) { qlog("Ferrovia: codigo de %s nao confere.", what); return 0; }
    DWORD old; if (!VirtualProtect(t, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    unsigned char j[32]; memset(j, 0x90, sizeof j);
    j[0] = 0xff; j[1] = 0x25; memset(j + 2, 0, 4); *(unsigned long long *)(j + 6) = (unsigned long long)stub;
    memcpy(t, j, n); VirtualProtect(t, n, old, &old); FlushInstructionCache(GetCurrentProcess(), t, n);
    return 1;
}

static const struct { const char *loc, *txt; } k_rail_txt[] = {
    { "pt", "FERROVIA" }, { "es", "FERROCARRIL" }, { "fr", "CHEMIN DE FER" }, { "de", "EISENBAHN" }, { "it", "FERROVIA" },
    { "pl", "KOLEJ" }, { "ru", "\xd0\x96\xd0\x95\xd0\x9b\xd0\x95\xd0\x97\xd0\x9d\xd0\x90\xd0\xaf \xd0\x94\xd0\x9e\xd0\xa0\xd0\x9e\xd0\x93\xd0\x90" },
};
static int build_road(const wchar_t *src, const wchar_t *dst) {
    size_t len; char *b = read_all(src, &len); if (!b) { qlog("Ferrovia: nao li %ls.", src); return 0; }
    char *nb = (char *)malloc(len + 4096); memcpy(nb, b, len + 1); free(b); b = nb;
    int ok = 0;
    char *r = strstr(b, "ID=\"RepairTab\"");
    if (r && !strstr(b, "AraRailCheck")) {
        size_t e = line_end(b, r - b);
        char ins[512];
        int il = snprintf(ins, sizeof ins, "                <!-- Mod Ferrovias -->\r\n"
            "                <CheckBox ID=\"AraRailCheck\" Style=\"Button-Transparent\" TextStyle=\"HeaderTabText\" Size=\"200,60\" "
            "String=\"%s\" AudioID=\"ManageRoadScreen_Build\"/>\r\n", g_txt_rail);
        memmove(b + e + il, b + e, len - e + 1); memcpy(b + e, ins, il); len += il;
        ok = 1;
    }
    if (!ok) { qlog("Ferrovia: RoadManagementScreen.xml com formato inesperado; ferrovia desligada."); free(b); return 0; }
    // Titulo de ferrovia: terceiro filho do ModeMux (o jogo seleciona ModeBuild/ModeRepair; a DLL seleciona este).
    int title = 0;
    if (g_ui_lang >= 0) {
        char st[640];
        snprintf(st, sizeof st, "</Stack>\r\n                <!-- Mod Ferrovias -->\r\n"
            "                <Stack ID=\"AraRailMode\" Dir=\"r\" Anchor=\"c,c\" AutoSize=\"1\" Padding=\"10\" Hidden=\"1\">\r\n"
            "                    <Image Texture=\"UI/Art/Parts/RoadUI/RoadUI_TitleIcon_Build.dds\" UnitSize=\"62,62\" Size=\"62,62\" Anchor=\"c,c\"/>\r\n"
            "                    <Text String=\"%s\" Font=\"MainMenu\" FontSize=\"32\" Anchor=\"c,c\"/>\r\n"
            "                </Stack>", k_rail_ui[g_ui_lang].title);
        title = repl_once(&b, &len, "</Stack>", st, "ID=\"ModeRepair\"");
    }
    // Legenda do mapa: linha "Ferrovia" logo abaixo de "Road" (mesma amostra de linha, cor provisoria ate o visual do trilho).
    int legend = 0;
    if (g_ui_lang >= 0) {
        char lg[512];
        snprintf(lg, sizeof lg, "</Stack>\r\n                    <!-- Mod Ferrovias -->\r\n"
            "                    <Stack Dir=\"r\" Padding=\"10\" Anchor=\"l,t\">\r\n"
            "                        <Image Texture=\"UI/Art/Parts/RoadUI/RoadUI_MapLegend_RoadSlices.dds\" AtlasIndex=\"0\" Size=\"25,10\" UnitSize=\"25,10\" Offset=\"0,10\" Color=\"190,150,90,255\"/>\r\n"
            "                        <Text String=\"%s\" Style=\"LegendText\" Anchor=\"c,c\"/>\r\n"
            "                    </Stack>", k_rail_ui[g_ui_lang].legend);
        legend = repl_once(&b, &len, "</Stack>", lg, "String=\"TXT_ROAD\" Style=\"LegendText\"");
    }
    ok = write_all(dst, b, len);
    qlog("Ferrovia: XML da tela montado (titulo de ferrovia: %s; legenda: %s).", title ? "sim" : "nao", legend ? "sim" : "nao");
    free(b); return ok;
}

static const unsigned char k_pro_parse[24] = { 0x48,0x89,0x5c,0x24,0x18, 0x48,0x89,0x74,0x24,0x20, 0x57, 0x41,0x54, 0x41,0x55, 0x41,0x56, 0x41,0x57,
                                               0xb8,0x30,0x02,0x01,0x00 };
static const unsigned char k_pro_road_binder[16] = { 0x48,0x89,0x5c,0x24,0x08, 0x55, 0x48,0x8b,0xec, 0x48,0x83,0xec,0x30, 0x48,0x8b,0xd9 };
static const unsigned char k_pro_road_open[20] = { 0x48,0x89,0x5c,0x24,0x08, 0x48,0x89,0x6c,0x24,0x10, 0x48,0x89,0x74,0x24,0x18, 0x48,0x89,0x7c,0x24,0x20 };
static const unsigned char k_pro_refresh[20] = { 0x48,0x89,0x5c,0x24,0x10, 0x48,0x89,0x74,0x24,0x18, 0x48,0x89,0x7c,0x24,0x20, 0x48,0x89,0x4c,0x24,0x08 };
static const unsigned char k_pro_localize[17] = { 0x40,0x55, 0x53, 0x41,0x54, 0x41,0x55, 0x41,0x56, 0x48,0x8d,0xac,0x24,0xe0,0xf3,0xff,0xff };
static const unsigned char k_site_ui[24] = { 0x49,0x8b,0x82,0x48,0x6f,0x07,0x00, 0x48,0x8d,0x0c,0x5b, 0x83,0x7c,0x88,0x08,0xff, 0x74,0x06,
                                             0x80,0x3c,0x88,0x02, 0x75,0x77 };
static const unsigned char k_site_order[18] = { 0x42,0x39,0x5c,0x80,0x08, 0x74,0x0b, 0x42,0x80,0x3c,0x80,0x02, 0x0f,0x85,0x91,0x03,0x00,0x00 };
static const unsigned char k_args_ui[] = { 0x89,0xd9, 0x4c,0x89,0xd2 };                        // ecx = regiao (ebx), rdx = gc (r10)
static const unsigned char k_args_order[] = { 0x44,0x89,0xf1, 0x4c,0x89,0xea, 0x44,0x8b,0x45,0x6f }; // ecx = r14d, rdx = r13, r8d = [rbp+0x6f]

static void rail_install(HINSTANCE h) {
    char v[16];
    GetPrivateProfileStringA("Ferrovias", "Ativar", "", v, sizeof v, g_inipath);
    if (!v[0]) { WritePrivateProfileStringA("Ferrovias", "Ativar", "1", g_inipath); strcpy(v, "1"); }
    if (!atoi(v)) { qlog("Ferrovia: desligada no Ferrovias.ini."); return; }
    read_locale();
    for (size_t i = 0; i < sizeof k_rail_txt / sizeof *k_rail_txt; i++)
        if (!_strnicmp(g_locale, k_rail_txt[i].loc, 2)) { snprintf(g_txt_rail, sizeof g_txt_rail, "%s", k_rail_txt[i].txt); break; }
    for (size_t i = 0; i < sizeof k_rail_ui / sizeof *k_rail_ui; i++)
        if (!_strnicmp(g_locale, k_rail_ui[i].loc, 2)) { g_ui_lang = (int)i; break; }
    wchar_t dir[MAX_PATH], src[MAX_PATH], dst[MAX_PATH];
    GetModuleFileNameW(h, dir, MAX_PATH); { wchar_t *q = wcsrchr(dir, L'\\'); if (q) q[1] = 0; }
    _snwprintf(dst, MAX_PATH, L"%lsFerrovias_UI", dir); CreateDirectoryW(dst, NULL);   // copia gerada (so para conferencia)
    _snwprintf(src, MAX_PATH, L"%ls..\\assets\\UI\\RoadManagementScreen.xml", dir);
    _snwprintf(dst, MAX_PATH, L"%lsFerrovias_UI\\RoadManagementScreen.xml", dir);
    if (!build_road(src, dst) || !(g_xml_road = read_all(dst, &g_xml_road_len))) return;
    // confere o codigo do meio das funcoes antes de mexer em qualquer coisa (os inicios sao conferidos por ara_hook)
    if (memcmp(g_base + SITE_UI_COUNT, k_site_ui, 24) || memcmp(g_base + SITE_ORDER, k_site_order, 18)) {
        qlog("Ferrovia: codigo do jogo diferente do esperado; nada alterado."); g_xml_road = NULL; return;
    }
    void *st_ui = make_stub(k_args_ui, sizeof k_args_ui, (void *)need_ui, g_base + 0x8d684a, g_base + 0x8d68c1);
    void *st_or = make_stub(k_args_order, sizeof k_args_order, (void *)need_order, g_base + 0xe4aeb9, g_base + 0xe4b24a);
    if (!st_ui || !st_or) { g_xml_road = NULL; return; }
    g_road_binder_tramp = (binder_fn)ara_hook(g_base + RVA_ROAD_BINDER, k_pro_road_binder, 16, (void *)hook_road_binder, "tela de estradas");
    if (!g_road_binder_tramp) { qlog("Ferrovia: gancho da tela falhou; nada mais alterado."); g_xml_road = NULL; return; }
    g_road_open_tramp = (road_open_fn)ara_hook(g_base + RVA_ROAD_OPEN, k_pro_road_open, 20, (void *)hook_road_open, "abertura da tela de estradas");
    g_parse_prev = (parse_fn)ara_hook(g_base + RVA_UI_PARSE, k_pro_parse, 24, (void *)hook_parse, "analise do XML");
    if (!g_road_open_tramp || !g_parse_prev) {
        // o binder ja esta enganchado: sem o XML ele so nao acha o botao e nao faz nada
        qlog("Ferrovia: ganchos da tela falharam (abertura %s, XML %s); ferrovia desligada.", g_road_open_tramp ? "ok" : "FALHOU", g_parse_prev ? "ok" : "FALHOU");
        g_xml_road = NULL; return;
    }
    // textos: opcionais (sem eles a ferrovia funciona, so a tela continua dizendo "estrada")
    if (g_ui_lang >= 0) {
        g_refresh_tramp = (refresh_fn)ara_hook(g_base + RVA_BUILD_REFRESH, k_pro_refresh, 20, (void *)hook_build_refresh, "atualizacao da tela");
        g_loc_tramp = (loc_fn)ara_hook(g_base + RVA_LOCALIZE, k_pro_localize, 17, (void *)hook_localize, "traducao de textos");
    }
    g_h_hud = ((hash_fn)(g_base + RVA_HASH))("HUDResourceIcons_V2", 19);
    g_h_items = ((hash_fn)(g_base + RVA_HASH))("Items_160", 9);
    if (g_h_items == 0xe27783fbu) g_atlas_tramp = (atlas_id_fn)hook_atlas_install();   // o jogo usa 0xe27783fb para "Items_160" (0x742355)
    else qlog("Ferrovia: hash de atlas inesperado (%08x); icones ficam como estao.", g_h_items);
    // desenho da ferrovia: so se os bytes do ponto conferirem (patch_site confere)
    int mat = 0;
    { void *st_mat = make_material_stub(); if (st_mat) mat = patch_site(SITE_MATERIAL, k_site_material, 15, st_mat, "material do trecho"); }
    int hid = 0;
    if (mat) { void *st_h = make_hidden_stub(); if (st_h) hid = patch_site(0x5c462cull, k_site_hidden, 19, st_h, "trecho escondido na cidade"); }
    int dv = 0;
    int al = mat && alinha_install();
    if (mat) { HANDLE th = CreateThread(NULL, 0, rail_thread, NULL, 0, NULL); if (th) { dv = 1; CloseHandle(th); } }
    int a = patch_site(SITE_UI_COUNT, k_site_ui, 24, st_ui, "contagem da tela");
    int b = patch_site(SITE_ORDER, k_site_order, 18, st_or, "ordem por regiao");
    qlog("Ferrovia: instalada (botao \"%s\", idioma %s; contagem da tela %s; ordem por regiao %s; titulo %s; textos %s; icones %s; desenho do trilho %s; trilho dentro da cidade %s; estacao alinhada a linha %s).", g_txt_rail, g_locale,
         a ? "ok" : "FALHOU", b ? "ok" : "FALHOU", g_refresh_tramp ? "ok" : "nao", g_loc_tramp ? "ok" : "nao", g_atlas_tramp ? "ok" : "nao", mat ? "ok" : "FALHOU", hid ? "ok" : "FALHOU", dv && al ? "ok" : "FALHOU");
}

// 1.8: rede de seguranca para a depuracao (pontos de parada de hardware usados nos rastreios autorizados): se um ponto
// ficar armado numa thread depois que o depurador desconecta, a excecao de passo unico ficaria sem dono e o jogo fecharia
// (aconteceu em 16/10 05:01). So trata passo unico vindo de ponto de hardware (DR6 bits 0-3) sem depurador conectado:
// desarma os registradores de depuracao da thread e segue.
static LONG CALLBACK stray_hwbp(EXCEPTION_POINTERS *e) {
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !(e->ContextRecord->Dr6 & 0xf) || IsDebuggerPresent()) return EXCEPTION_CONTINUE_SEARCH;
    e->ContextRecord->Dr0 = e->ContextRecord->Dr1 = e->ContextRecord->Dr2 = e->ContextRecord->Dr3 = 0;
    e->ContextRecord->Dr6 = 0; e->ContextRecord->Dr7 = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(h);
    InitializeCriticalSection(&g_dvcs);
    AddVectoredExceptionHandler(1, stray_hwbp);
    GetModuleFileNameA(h, g_logpath, MAX_PATH);
    char *s = strrchr(g_logpath, '\\'); strcpy(s ? s + 1 : g_logpath, "Ferrovias.log");
    strcpy(g_inipath, g_logpath); { char *q = strrchr(g_inipath, '.'); if (q) strcpy(q, ".ini"); }
    char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
    const char *en = strrchr(exe, '\\'); en = en ? en + 1 : exe;
    if (_stricmp(en, "AraHistoryUntold.exe") != 0) return TRUE;
    g_base = (unsigned char *)GetModuleHandleA(NULL);
    qlog("=== Ferrovias 1.9 carregado (mod independente; proxy de avformat-60.dll)");
    if (!verify_target()) return TRUE;
    rail_install(h);
    return TRUE;
}
