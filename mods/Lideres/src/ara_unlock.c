// AraLeaderBuilderUnlock - proxy de swscale-7.dll para ARA: History Untold 2.1.0.542
// Liga o DebugButton escondido do menu principal (sobre o logo do ARA) ao handler
// original do botao "LEADER BUILDER" da tela de debug, que nunca fica acessivel na versao final.
// Todas as exportacoes de swscale-7.dll sao repassadas para swscale-7_orig.dll (via .def).
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// --- Constantes do build alvo (AraHistoryUntold.exe 2.1.0.542, CL 222963) ---
#define EXPECTED_TIMESTAMP   0x6a286b53u
#define EXPECTED_SIZEOFIMAGE 0x5e622e00u
#define RVA_MAINMENU         0x3b486680ull   // objeto global da tela MainMenu
#define OFF_DEBUGBUTTON      0x2f790ull      // MainMenu -> ponteiro do elemento "DebugButton"
#define OFF_CB_CTX           0x130ull        // elemento -> contexto do callback de clique
#define OFF_CB_FN            0x138ull        // elemento -> funcao de callback de clique
#define RVA_LEADERBTN_HANDLER 0x788250ull    // handler original do botao LEADER BUILDER
#define RVA_LB_FLAGS         0x58a6bd78ull   // controlador do LeaderBuilder +0x3e8 (0x40=init, 0x80=pronto)
#define RVA_LB_SCREEN_PTR    0x58a6bd88ull   // controlador +0x3f8 -> tela LeaderBuilder
#define RVA_NB_SCREEN_PTR    0x067514c0ull   // wrapper NationBuilder -> tela NationBuilder
#define RVA_NB_READY_ELEM    0x067514d0ull   // preenchido quando a NationBuilder termina de carregar
#define RVA_S3_SCREEN_PTR    0x56833df8ull   // terceira tela do fluxo (FlagBuilder)

static const unsigned char SIG_HANDLER[16] = {
    0x48,0x83,0xec,0x48, 0x83,0x7a,0x38,0x00, 0x0f,0x85,0x0e,0x01,0x00,0x00, 0xb2,0x01 };
static const unsigned char SIG_MAINMENU_BIND[14] = {   // em 0x11e6175: lea r8,[rbx+0x2f790]
    0x4c,0x8d,0x83,0x90,0xf7,0x02,0x00, 0x89,0x45,0xfc, 0x48,0x8d,0x55,0xf0 };
#define RVA_MAINMENU_BIND 0x11e6175ull

typedef void (*click_handler_t)(void *ctx, void *evt);

static HMODULE        g_self;
static int            g_enable_builder = 0;   // 1 = logo do ARA abre o criador de lideres
static unsigned char *g_base;
static click_handler_t g_orig_handler;
static volatile ULONGLONG g_last_click;
static char g_logpath[MAX_PATH];

static void ulog(const char *fmt, ...) {
    FILE *f = fopen(g_logpath, "a");
    if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

static int readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!p || !VirtualQuery(p, &mbi, sizeof mbi)) return 0;
    if (mbi.State != MEM_COMMIT) return 0;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
    if (!(mbi.Protect & (PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return 0;
    return (const unsigned char*)p + n <= (const unsigned char*)mbi.BaseAddress + mbi.RegionSize;
}

static int ptr_ok(unsigned char *obj, unsigned long long off) {
    if (!obj || !readable(obj + off, 8)) return 0;
    void *p = *(void **)(obj + off);
    return p && readable(p, 0x100);
}

// As tres telas do criador ja carregaram e todos os elementos usados pelo caminho de abertura existem?
static int builder_ready(void) {
    unsigned char *lb = *(unsigned char **)(g_base + RVA_LB_SCREEN_PTR);
    unsigned char *nb = *(unsigned char **)(g_base + RVA_NB_SCREEN_PTR);
    unsigned char *s3 = *(unsigned char **)(g_base + RVA_S3_SCREEN_PTR);
    if (!*(void **)(g_base + RVA_NB_READY_ELEM)) return 0;
    static const unsigned long long lb_offs[] = {0x2fa10,0x2fa18,0x2fa58,0x2fa70,0x2fb18,0x2fb78,0x2fb80};
    for (unsigned i = 0; i < sizeof lb_offs / sizeof *lb_offs; i++) if (!ptr_ok(lb, lb_offs[i])) return 0;
    if (!ptr_ok(nb, 0x2f8e8)) return 0;
    if (!ptr_ok(s3, 0x2f960) || !ptr_ok(s3, 0x2f9f0)) return 0;
    return 1;
}

static void call_orig(void *ctx, void *evt) {
    unsigned char fake[0x100];
    memset(fake, 0, sizeof fake);
    if (evt && readable(evt, 0x38)) memcpy(fake, evt, 0x38);
    *(int *)(fake + 0x38) = 0;                   // condicao exigida pelo handler original
    g_orig_handler(ctx, fake);
}

// Chamado pela UI do jogo (thread da interface) quando o DebugButton e clicado.
static void OnDebugButtonClick(void *ctx, void *evt) {
    ULONGLONG now = GetTickCount64();
    if (now - g_last_click < 800) return;        // evita disparo duplo
    g_last_click = now;
    volatile unsigned int *flags = (volatile unsigned int *)(g_base + RVA_LB_FLAGS);
    ulog("Clique no logo (flags=%02x)", *flags & 0xff);
    if (!(*flags & 0x40)) {
        call_orig(ctx, evt);                     // 1a vez: o jogo inicializa e carrega as telas
        ulog("Inicializacao pedida (flags=%02x). Telas carregando.", *flags & 0xff);
    }
    if (!(*flags & 0x80)) {
        if (!builder_ready()) {
            ulog("Telas do criador ainda nao carregaram. Clique no logo de novo em 2-3 segundos.");
            return;
        }
        *flags |= 0x80;                          // o trecho que a versao final cortou
        ulog("Telas prontas. Bit 'pronto' ligado.");
    } else if (!builder_ready()) {
        ulog("Flag pronta mas telas incompletas; abortando por seguranca.");
        return;
    }
    call_orig(ctx, evt);                         // caminho de abertura original
    ulog("Criador aberto (flags=%02x)", *flags & 0xff);
}


// ---------------------------------------------------------------------------
// Entitlements: o jogo so mostra lideres/skins presentes na lista de itens de uma entitlement possuida
// (mapa global em RVA 0x1d58c6a0: entradas {u32 id; ...; u32 count @8; u32 cap @0xc; u32 *items @0x10}, stride 0x18).
// v1.2 trocava o ponteiro do delegate e disparava a protecao anti-adulteracao do jogo ("Mana values...").
// v1.3 so acrescenta os IDs do mod na lista da entitlement BaseGame (dados no heap, sem tocar em codigo/ponteiros).
#define RVA_ENT_MAP      0x1d58c6a0ull
#define RVA_ENT_RECORDS  0x1d58cfc8ull   // -> { record *recs; u32 count }, record stride 0xc0, possuida se dword[0x11]!=0
#define RVA_GAME_ALLOC   0xbea8c0ull
#define ENT_BASEGAME     0xa6062913u
typedef void *(*game_alloc_t)(unsigned long long);
static unsigned g_allow[32]; static int g_nallow;
static int g_ent_done_logged;

static unsigned mm3(const char *s) {
    const unsigned char *d=(const unsigned char*)s; size_t len=strlen(s); unsigned h=0, c1=0xcc9e2d51, c2=0x1b873593, k;
    size_t n=len/4;
    for (size_t i=0;i<n;i++){ memcpy(&k,d+4*i,4); k*=c1; k=(k<<15)|(k>>17); k*=c2; h^=k; h=(h<<13)|(h>>19); h=h*5+0xe6546b64; }
    const unsigned char *t=d+4*n; k=0;
    switch(len&3){ case 3: k^=t[2]<<16; /* fallthrough */ case 2: k^=t[1]<<8; /* fallthrough */ case 1: k^=t[0]; k*=c1; k=(k<<15)|(k>>17); k*=c2; h^=k; }
    h^=(unsigned)len; h^=h>>16; h*=0x85ebca6b; h^=h>>13; h*=0xc2b2ae35; h^=h>>16; return h;
}
static void allow(const char *s){ if (g_nallow<32){ g_allow[g_nallow++]=mm3(s); ulog("Liberar: %s = %08x", s, mm3(s)); } }

static void ent_poll(void) {
    unsigned char *pl = *(unsigned char **)(g_base + RVA_ENT_RECORDS);
    if (!pl || !readable(pl, 0x10)) return;
    unsigned char *recs = *(unsigned char **)pl; unsigned nrec = *(unsigned *)(pl + 8);
    if (!recs || !nrec || nrec > 4096 || !readable(recs, (size_t)nrec * 0xc0)) return;
    unsigned key = 0xffffffffu;
    for (unsigned i=0;i<nrec;i++){ unsigned id=*(unsigned*)(recs+i*0xc0); if (id==ENT_BASEGAME && *(unsigned*)(recs+i*0xc0+0x44)) key=id; }
    if (key==0xffffffffu) for (unsigned i=0;i<nrec;i++) if (*(unsigned*)(recs+i*0xc0+0x44)) { key=*(unsigned*)(recs+i*0xc0); break; }
    if (key==0xffffffffu) return;
    unsigned char *map = g_base + RVA_ENT_MAP;
    unsigned n = *(unsigned *)(map + 0x10); unsigned char *ents = *(unsigned char **)(map + 0x18);
    if (!ents || !n || n > 4096 || !readable(ents, (size_t)n * 0x18)) return;
    unsigned char *e = NULL;
    for (unsigned i=0;i<n;i++) if (*(unsigned*)(ents+i*0x18)==key) { e = ents+i*0x18; break; }
    if (!e) return;
    unsigned cnt = *(unsigned*)(e+8), cap = *(unsigned*)(e+0xc); unsigned *items = *(unsigned**)(e+0x10);
    if (cnt > 100000 || (cnt && (!items || !readable(items, (size_t)cnt*4)))) return;
    unsigned missing[32]; int nm=0;
    for (int a=0;a<g_nallow;a++){ int f=0; for (unsigned i=0;i<cnt;i++) if (items[i]==g_allow[a]){f=1;break;} if(!f) missing[nm++]=g_allow[a]; }
    if (!nm) { if (!g_ent_done_logged){ g_ent_done_logged=1; ulog("Entitlement %08x ja contem os IDs do mod (%u itens).", key, cnt);} return; }
    if (cnt + nm > cap) {
        unsigned ncap = (cnt + nm + 0x3f) & ~0x3fu;
        unsigned *nb = (unsigned *)((game_alloc_t)(g_base + RVA_GAME_ALLOC))((unsigned long long)ncap * 4);
        if (!nb) { ulog("Falha ao alocar lista de entitlement."); return; }
        if (cnt) memcpy(nb, items, (size_t)cnt*4);
        for (int a=0;a<nm;a++) nb[cnt+a]=missing[a];
        *(unsigned*)(e+0xc) = ncap; MemoryBarrier();
        *(unsigned**)(e+0x10) = nb; MemoryBarrier();
        *(unsigned*)(e+8) = cnt + nm;              // lista antiga fica sem liberar (alguns bytes) para nao correr com leitores
        ulog("Entitlement %08x: lista realocada %u->%u, +%d IDs do mod.", key, cnt, cnt+nm, nm);
    } else {
        for (int a=0;a<nm;a++) items[cnt+a]=missing[a];
        MemoryBarrier();
        *(unsigned*)(e+8) = cnt + nm;
        ulog("Entitlement %08x: +%d IDs do mod (%u->%u itens, cap %u).", key, nm, cnt, cnt+nm, cap);
    }
    g_ent_done_logged = 0;
}


// ---------------------------------------------------------------------------
// LeaderView: a tela de selecao procura o visual 3D (LeaderView.Leader) so na biblioteca IndusClientData
// (objeto em RVA 0x3b9f1700; tabela ordenada em lib+0x1540 -> T; chave = mm3({T+0x94, grupo, id}); entradas
// {u32 chave; ptr registro} em T+0x990, contagem T+0x998; registro+0x10 = hash do tipo, +0x18 = dados).
// Os visuais ldr_c_* ficam na biblioteca do criador (RVA 0x58a6dcc0), carregada quando o criador inicializa.
// v1.4 copia os registros ldr_c_* da biblioteca do criador para a tabela da IndusClientData.
#define RVA_CLIENT_LIB   0x3b9f1700ull
#define RVA_CUSTOM_LIB   0x58a6dcc0ull
#define H_LEADERVIEW_T   0xb3cf53aeu   // "LeaderView.Leader"
#define H_LEADERVIEW_G   0x989c847cu   // "LeaderViewData"
static unsigned mm3b(const unsigned char *d, size_t len) {
    unsigned h=0, c1=0xcc9e2d51, c2=0x1b873593, k; size_t n=len/4;
    for (size_t i=0;i<n;i++){ memcpy(&k,d+4*i,4); k*=c1; k=(k<<15)|(k>>17); k*=c2; h^=k; h=(h<<13)|(h>>19); h=h*5+0xe6546b64; }
    const unsigned char *t=d+4*n; k=0;
    switch(len&3){ case 3: k^=t[2]<<16; /* fallthrough */ case 2: k^=t[1]<<8; /* fallthrough */ case 1: k^=t[0]; k*=c1; k=(k<<15)|(k>>17); k*=c2; h^=k; }
    h^=(unsigned)len; h^=h>>16; h*=0x85ebca6b; h^=h>>13; h*=0xc2b2ae35; h^=h>>16; return h;
}
static unsigned char *lib_table(unsigned long long rva) {
    unsigned char *T = *(unsigned char **)(g_base + rva + 0x1540);
    if (!T || !readable(T, 0x9a0)) return NULL;
    unsigned n = *(unsigned *)(T + 0x998); unsigned char *arr = *(unsigned char **)(T + 0x990);
    if (!n || n > 2000000 || !arr || !readable(arr, (size_t)n * 0x10)) return NULL;
    return T;
}
static unsigned lib_key(unsigned char *T, unsigned group, unsigned id) {
    unsigned b[3] = { *(unsigned *)(T + 0x94), group, id }; return mm3b((unsigned char *)b, 12);
}
static unsigned char *lib_find(unsigned char *T, unsigned key) {   // devolve o ponteiro do registro
    unsigned n = *(unsigned *)(T + 0x998); unsigned char *arr = *(unsigned char **)(T + 0x990);
    unsigned lo=0, hi=n;
    while (lo<hi){ unsigned mid=(lo+hi)/2; unsigned k=*(unsigned*)(arr+mid*0x10); if (k==key) return *(unsigned char**)(arr+mid*0x10+8); if (k<key) lo=mid+1; else hi=mid; }
    return NULL;
}
static void dump_struct(const char *tag, unsigned char *p, size_t n) {
    char path[MAX_PATH]; strcpy(path, g_logpath); char *s=strrchr(path,'\\'); sprintf(s?s+1:path, "lvdump_%s.bin", tag);
    if (!readable(p, n)) { ulog("dump %s: memoria nao legivel", tag); return; }
    FILE *f=fopen(path,"wb"); if(!f) return;
    fwrite(p,1,n,f);
    // segue ponteiros (qwords alinhados) e grava 0x100 bytes de cada alvo: [u32 offset][u32 0][0x100 bytes]
    for (size_t o=0;o+8<=n;o+=8){ unsigned char *q=*(unsigned char**)(p+o);
        if ((unsigned long long)q>0x10000 && ((unsigned long long)q>>48)==0 && readable(q,0x100)) { unsigned hdr[2]={(unsigned)o,0}; fwrite(hdr,4,2,f); fwrite(q,1,0x100,f);} }
    fclose(f);
}

// Visual do Getulio: Florey (rosto/pele/sobrancelha/corpo) + cabelo do Bismarck + roupa do Benito Juarez.
// Layout LeaderView.Leader (0x180 bytes): 0x10 Head, 0x30 Body, 0x50 Headwear*, 0x60 Eyebrows*, 0x70 FacialHair*,
// 0x80 Outfit, 0xa0 Accessory1* (oculos do Florey), 0xe0 BaseSkin.  (* = vetor {ptr, ?, tamanho})
// Teste de cor do cabelo por skin: indices 1, 2 e a cor original do Bismarck (0 = sem troca).
#define H_GROUP_CONTEMP_GV 0xdd9d6f77u   // grupo das pecas Contemporary
static const char *g_lv_ids[3] = { "ldr_c_1937", "ldr_c_1951", "ldr_c_1954" };
static const unsigned g_hair_idx[3] = { 1, 2, 0 };
static int g_lv_done;
static void lv_insert(unsigned char *CT, unsigned char **recs, unsigned *keys, int nadd, const char *tag);

static unsigned char *lv_get(unsigned char *CT, const char *id) {
    unsigned char *r = lib_find(CT, lib_key(CT, H_LEADERVIEW_G, mm3(id)));
    if (!r || !readable(r,0x20) || *(unsigned*)(r+0x10)!=H_LEADERVIEW_T) return NULL;
    return r;
}
static void hexlog(const char *tag, unsigned char *p, unsigned n) {
    char b[400]; int k=0; if (!readable(p,n)) { ulog("%s: ilegivel", tag); return; }
    for (unsigned i=0;i<n && k<380;i+=4) k+=sprintf(b+k,"%08x ",*(unsigned*)(p+i));
    ulog("%s: %s", tag, b);
}

static void lv_poll(void) {
    if (g_lv_done) return;
    unsigned char *CT = lib_table(RVA_CLIENT_LIB);
    if (!CT) return;
    unsigned char *rF = lv_get(CT, "skin_HowardFlorey"), *rB = lv_get(CT, "skin_BenitoJuarez");
    unsigned char *rO = lv_get(CT, "skin_OttoVonBismarck");
    if (!rF || !rB || !rO) return;
    g_lv_done = 1;
    const unsigned dsz = 0x180;
    unsigned char *dF=*(unsigned char**)(rF+0x18), *dB=*(unsigned char**)(rB+0x18), *dO=*(unsigned char**)(rO+0x18);
    if (!readable(dF,dsz) || !readable(dB,dsz) || !readable(dO,dsz)) { ulog("Visuais base ilegiveis"); return; }
    unsigned char *hwO = *(unsigned char**)(dO+0x50), *ebF = *(unsigned char**)(dF+0x60);
    if (!readable(hwO,0x30) || !readable(ebF,0x20)) { ulog("Headwear/Eyebrows ilegiveis"); return; }
    unsigned char *hc1 = *(unsigned char**)(hwO+0x10), *hc2 = *(unsigned char**)(hwO+0x20);
    hexlog("Bismarck Headwear", hwO, 0x30); hexlog("Bismarck Headwear+10", hc1, 0x10); hexlog("Bismarck Headwear+20", hc2, 0x10);
    hexlog("Florey Eyebrows", ebF, 0x20); hexlog("Benito Outfit", dB+0x80, 0x20);
    if (!readable(hc1,0x10) || !readable(hc2,0x10)) { ulog("Cores do cabelo ilegiveis"); return; }
    game_alloc_t ga = (game_alloc_t)(g_base + RVA_GAME_ALLOC);
    unsigned char *recs[3]; unsigned keys[3]; int nadd=0;
    for (int i=0;i<3;i++) {
        unsigned key = lib_key(CT, H_LEADERVIEW_G, mm3(g_lv_ids[i]));
        if (lib_find(CT, key)) continue;
        unsigned char *d=(unsigned char*)ga(dsz), *r=(unsigned char*)ga(0x40), *hw=(unsigned char*)ga(0x30);
        unsigned char *c1=(unsigned char*)ga(0x10), *c2=(unsigned char*)ga(0x10), *eb=(unsigned char*)ga(0x20);
        if (!d||!r||!hw||!c1||!c2||!eb) { ulog("Falha ao alocar visual"); return; }
        memcpy(d, dF, dsz);                              // Florey: cabeca, corpo, pele, sobrancelhas
        memcpy(d+0x80, dB+0x80, 0x30);                   // roupa do Benito (e sem acessorio = sem oculos)
        *(unsigned*)(d+0x84) = 0xc3caa1adu;              // paleta Outfit_BenitoJuarez_VarB (faixa verde do mod)
        memcpy(hw, hwO, 0x30); memcpy(c1, hc1, 0x10); memcpy(c2, hc2, 0x10); memcpy(eb, ebF, 0x20);
        if (g_hair_idx[i]) {
            for (int k=0;k<3;k++){ ((unsigned*)c1)[k]=g_hair_idx[i]; ((unsigned*)c2)[k]=g_hair_idx[i]; ((unsigned*)(eb+0x10))[k]=g_hair_idx[i]; }
        }
        // v1.17: cabelo proprio do Getulio = asset sem uso Accessories_SapphoContemporary_Necklaces, que o instalador do mod URSS
        // transforma em malha do cabelo do Bismarck + textura escura so dele (o cabelo do Bismarck continua branco)
        *(unsigned*)(hw+0x00) = H_GROUP_CONTEMP_GV; *(unsigned*)(hw+0x04) = mm3("Accessories_SapphoContemporary_Necklaces");
        *(unsigned char**)(hw+0x10)=c1; *(unsigned char**)(hw+0x20)=c2;
        *(unsigned char**)(d+0x50)=hw; *(unsigned char**)(d+0x60)=eb;
        memcpy(r, rF, 0x40); *(unsigned*)(r+8) = mm3(g_lv_ids[i]); *(unsigned char**)(r+0x18) = d;
        recs[nadd]=r; keys[nadd]=key; nadd++;
    }
    if (!nadd) return;
    lv_insert(CT, recs, keys, nadd, "Getulio");
}

// Insere registros novos (chaves ainda ausentes) na tabela ordenada da IndusClientData.
static void lv_insert(unsigned char *CT, unsigned char **recs, unsigned *keys, int nadd, const char *tag) {
    game_alloc_t ga = (game_alloc_t)(g_base + RVA_GAME_ALLOC);
    unsigned n=*(unsigned*)(CT+0x998); unsigned char *arr=*(unsigned char**)(CT+0x990);
    unsigned nn = n + nadd;
    unsigned char *na = (unsigned char *)ga((unsigned long long)nn * 0x10);
    if (!na) { ulog("Falha ao alocar tabela"); return; }
    for (int a=0;a<nadd;a++) for (int b=a+1;b<nadd;b++) if (keys[b]<keys[a]){ unsigned t=keys[a]; keys[a]=keys[b]; keys[b]=t; unsigned char *tr=recs[a]; recs[a]=recs[b]; recs[b]=tr; }
    unsigned i=0, o=0; int a=0;
    while (i<n || a<nadd) {
        if (a<nadd && (i>=n || keys[a] < *(unsigned*)(arr+i*0x10))) {
            memset(na+o*0x10,0,0x10); *(unsigned*)(na+o*0x10)=keys[a]; *(unsigned char**)(na+o*0x10+8)=recs[a]; a++;
        } else { memcpy(na+o*0x10, arr+i*0x10, 0x10); i++; }
        o++;
    }
    *(unsigned char**)(CT+0x990) = na; MemoryBarrier();
    *(unsigned*)(CT+0x998) = nn; MemoryBarrier();
    ulog("LeaderView: %d visuais (%s) inseridos na IndusClientData (%u -> %u entradas).", nadd, tag, n, nn);
}


// Visual do Stalin (mod URSS), v1.16: pecas proprias. O instalador do mod (Mods\URSS\textures\texturas.ps1) reaproveita no
// Characters.OxAssetInfoPackage tres assets que nenhuma skin oficial usa, com texturas so do Stalin nos slots deles:
//   Outfit_Otto_Military        = malha Casual do Bismarck, casaco e camisa marrons, sem gravata
//   Headwear_Otto               = bigode liso do Bismarck, castanho-escuro
//   Eyebrows_SapphoContemporary = sobrancelhas do Alexandre, castanho-escuras (grupo de assets Contemporary)
// Resto: rosto/pele do Alexandre, corpo do Bismarck, cabelo do Florey.
#define H_OUTFIT_STALIN   0x384e41a3u   // mm3("Outfit_Otto_Military")
#define H_GROUP_CHAR      0x23ca6f01u   // /Characters/Characters.AssetGroup
#define H_GROUP_CONTEMP   0xdd9d6f77u   // grupo das pecas Contemporary
#define H_COLORREPLACE_HAIR 0xb1ff8ed3u
static const char *g_stalin_id = "ldr_c_1924";
static int g_stalin_done;
static void lv_poll_stalin(void) {
    if (g_stalin_done) return;
    unsigned char *CT = lib_table(RVA_CLIENT_LIB);
    if (!CT) return;
    unsigned char *rA = lv_get(CT, "skin_AlexanderTheGreat"), *rS = lv_get(CT, "skin_Ashoka"), *rF = lv_get(CT, "skin_HowardFlorey");
    unsigned char *rO = lv_get(CT, "skin_OttoVonBismarck");
    if (!rA || !rS || !rF || !rO) return;
    g_stalin_done = 1;
    const unsigned dsz = 0x180;
    unsigned char *dA=*(unsigned char**)(rA+0x18), *dS=*(unsigned char**)(rS+0x18), *dF=*(unsigned char**)(rF+0x18), *dO=*(unsigned char**)(rO+0x18);
    if (!readable(dA,dsz) || !readable(dS,dsz) || !readable(dF,dsz) || !readable(dO,dsz)) { ulog("Stalin: visuais base ilegiveis"); return; }
    unsigned char *fhS = *(unsigned char**)(dS+0x70), *ebA = *(unsigned char**)(dA+0x60);
    if (!readable(fhS,0x20) || !readable(ebA,0x20)) { ulog("Stalin: bigode/sobrancelha base ilegiveis"); return; }
    unsigned key = lib_key(CT, H_LEADERVIEW_G, mm3(g_stalin_id));
    if (lib_find(CT, key)) { ulog("Stalin: %s ja existe", g_stalin_id); return; }
    game_alloc_t ga = (game_alloc_t)(g_base + RVA_GAME_ALLOC);
    unsigned char *d=(unsigned char*)ga(dsz), *r=(unsigned char*)ga(0x40), *fh=(unsigned char*)ga(0x20), *eb=(unsigned char*)ga(0x20);
    if (!d || !r || !fh || !eb) { ulog("Stalin: falha ao alocar visual"); return; }
    memcpy(d, dA, dsz);                       // Alexandre: cabeca, pele (BaseSkin), resto
    memcpy(d+0x30, dO+0x30, 0x20);            // corpo do Bismarck (a roupa foi feita para ele)
    memcpy(d+0x50, dF+0x50, 0x10);            // cabelo do Florey
    memcpy(eb, ebA, 0x20);                    // sobrancelhas: elemento do Alexandre trocando grupo/nome pelo asset reaproveitado
    *(unsigned*)(eb+0x00) = H_GROUP_CONTEMP; *(unsigned*)(eb+0x04) = mm3("Eyebrows_SapphoContemporary");
    memcpy(d+0x60, dA+0x60, 0x10); *(unsigned char**)(d+0x60) = eb;
    memcpy(fh, fhS, 0x20);                    // bigode: elemento do Ashoka como molde
    *(unsigned*)(fh+0x00) = H_GROUP_CHAR; *(unsigned*)(fh+0x04) = mm3("Headwear_Otto");
    for (int k=0;k<3;k++) ((unsigned*)(fh+0x10))[k] = 0;
    *(unsigned*)(fh+0x1c) = H_COLORREPLACE_HAIR;
    memcpy(d+0x70, dS+0x70, 0x10); *(unsigned char**)(d+0x70) = fh;
    memcpy(d+0x80, dO+0x80, 0x60);            // roupa + acessorios (vazios) do Bismarck...
    *(unsigned*)(d+0x84) = H_OUTFIT_STALIN;   // ...trocando pelo casaco proprio do Stalin
    memcpy(r, rA, 0x40); *(unsigned*)(r+8) = mm3(g_stalin_id); *(unsigned char**)(r+0x18) = d;
    hexlog("Stalin sobrancelha", eb, 0x20); hexlog("Stalin bigode", fh, 0x20); hexlog("Stalin roupa", d+0x80, 0x20);
    unsigned char *recs[1] = { r }; unsigned keys[1] = { key };
    lv_insert(CT, recs, keys, 1, "Stalin");
}

// v1.19: 2a skin do Stalin, "1945" (marechal): pecas NOVAS acrescentadas aos pacotes por urss/scripts/novo_asset.py e tex_add.py
// (nenhum asset do jogo e reaproveitado):
//   Outfit_URSS_Stalin1945     = malha da farda do Washington Contemporary, textura branca com estrela de Heroi
//   Headwear_URSS_Stalin1945   = cabelo do Washington Contemporary, grisalho
//   FacialHair_URSS_Stalin1945 = bigode do Stalin, grisalho;  Eyebrows_URSS_Stalin1945 = sobrancelhas do Stalin, grisalhas
// Rosto/pele do Alexandre e corpo do Bismarck (como a skin 1924), cores do cabelo do Bismarck (branco).
#define QUEPE_ATIVO 1
static const char *g_stalin2_id = "ldr_c_1945";
static int g_stalin2_done;
static void lv_poll_stalin1945(void) {
    if (g_stalin2_done) return;
    unsigned char *CT = lib_table(RVA_CLIENT_LIB);
    if (!CT) return;
    unsigned char *rA = lv_get(CT, "skin_AlexanderTheGreat"), *rS = lv_get(CT, "skin_Ashoka");
    unsigned char *rG = lv_get(CT, "skin_Contemporary_GeorgeWashington"), *rO = lv_get(CT, "skin_OttoVonBismarck");
    if (!rA || !rS || !rG || !rO) return;
    g_stalin2_done = 1;
    const unsigned dsz = 0x180;
    unsigned char *dA=*(unsigned char**)(rA+0x18), *dS=*(unsigned char**)(rS+0x18), *dG=*(unsigned char**)(rG+0x18), *dO=*(unsigned char**)(rO+0x18);
    if (!readable(dA,dsz) || !readable(dS,dsz) || !readable(dG,dsz) || !readable(dO,dsz)) { ulog("Stalin1945: visuais base ilegiveis"); return; }
    unsigned char *fhS = *(unsigned char**)(dS+0x70), *ebA = *(unsigned char**)(dA+0x60), *hwG = *(unsigned char**)(dG+0x50), *hwO = *(unsigned char**)(dO+0x50);
    if (!readable(fhS,0x20) || !readable(ebA,0x20) || !readable(hwG,0x30) || !readable(hwO,0x30)) { ulog("Stalin1945: pecas base ilegiveis"); return; }
    hexlog("Washington Contemporary corpo", dG+0x30, 0x20); hexlog("Washington Contemporary cabelo", hwG, 0x30);
    hexlog("Washington Contemporary roupa", dG+0x80, 0x60);
    unsigned key = lib_key(CT, H_LEADERVIEW_G, mm3(g_stalin2_id));
    if (lib_find(CT, key)) { ulog("Stalin1945: %s ja existe", g_stalin2_id); return; }
    game_alloc_t ga = (game_alloc_t)(g_base + RVA_GAME_ALLOC);
    unsigned char *d=(unsigned char*)ga(dsz), *r=(unsigned char*)ga(0x40), *fh=(unsigned char*)ga(0x20), *eb=(unsigned char*)ga(0x20), *hw=(unsigned char*)ga(0x30);
    if (!d || !r || !fh || !eb || !hw) { ulog("Stalin1945: falha ao alocar visual"); return; }
    memcpy(d, dA, dsz);                       // Alexandre: cabeca, pele (BaseSkin), resto
    memcpy(d+0x30, dO+0x30, 0x20);            // corpo do Bismarck (mais gordo, como a skin 1924)
    memcpy(hw, hwG, 0x30);                    // cabelo: elemento do Washington Contemporary com o asset grisalho...
    *(unsigned*)(hw+0x04) = mm3("Headwear_URSS_Stalin1945");
    memcpy(hw+0x10, hwO+0x10, 0x8); memcpy(hw+0x20, hwO+0x20, 0x8);   // ...e as cores do cabelo branco do Bismarck
    *(unsigned char**)(d+0x50) = hw;
    memcpy(eb, ebA, 0x20);
    *(unsigned*)(eb+0x00) = H_GROUP_CONTEMP; *(unsigned*)(eb+0x04) = mm3("Eyebrows_URSS_Stalin1945");
    memcpy(d+0x60, dA+0x60, 0x10); *(unsigned char**)(d+0x60) = eb;
    memcpy(fh, fhS, 0x20);
    *(unsigned*)(fh+0x00) = H_GROUP_CHAR; *(unsigned*)(fh+0x04) = mm3("FacialHair_URSS_Stalin1945");
    for (int k=0;k<3;k++) ((unsigned*)(fh+0x10))[k] = 0;
    *(unsigned*)(fh+0x1c) = H_COLORREPLACE_HAIR;
    memcpy(d+0x70, dS+0x70, 0x10); *(unsigned char**)(d+0x70) = fh;
    memcpy(d+0x80, dG+0x80, 0x60);            // roupa + acessorios do Washington Contemporary...
    *(unsigned*)(d+0x84) = mm3("Outfit_URSS_Stalin1945");   // ...com a farda branca
    // quepe = acessorio (malha da boina do Copernico, Accessories_URSS_Quepe). Formato copiado do acessorio dos oculos do
    // Florey (skin_HowardFlorey, d+0xA0), trocando grupo/nome; o log mostra o formato para conferencia.
    // Quepe com MALHA NOVA (urss/scripts/quepe_build.py). Acessorio = ponteiro, no formato dos oculos
    // do Florey.
    unsigned char *rF = QUEPE_ATIVO ? lv_get(CT, "skin_HowardFlorey") : 0, *dF = rF ? *(unsigned char**)(rF+0x18) : 0;
    if (dF && readable(dF, dsz)) {
        hexlog("Florey acessorios d+A0", dF+0xA0, 0x40);
        unsigned char *ap = *(unsigned char**)(dF+0xA0);
        unsigned g0 = *(unsigned*)(dF+0xA0);
        if ((unsigned long long)ap > 0x10000 && readable(ap, 0x30)) {
            hexlog("Florey acessorio[0]", ap, 0x30);
            unsigned char *ac = (unsigned char*)ga(0x30);
            if (ac) {
                memcpy(ac, ap, 0x30);
                *(unsigned*)(ac+0x00) = H_GROUP_CHAR; *(unsigned*)(ac+0x04) = mm3("Accessories_URSS_Quepe");
                memcpy(d+0xA0, dF+0xA0, 0x10); *(unsigned char**)(d+0xA0) = ac;
                ulog("Stalin1945: quepe como acessorio (ponteiro)");
            }
        } else if (g0 == H_GROUP_CHAR || g0 == H_GROUP_CONTEMP || g0 == 0x6318f128u) {
            memcpy(d+0xA0, dF+0xA0, 0x20); *(unsigned*)(d+0xA0) = H_GROUP_CHAR; *(unsigned*)(d+0xA4) = mm3("Accessories_URSS_Quepe");
            ulog("Stalin1945: quepe como acessorio (embutido)");
        } else ulog("Stalin1945: formato de acessorio desconhecido, sem quepe");
    }
    memcpy(r, rA, 0x40); *(unsigned*)(r+8) = mm3(g_stalin2_id); *(unsigned char**)(r+0x18) = d;
    hexlog("Stalin1945 cabelo", hw, 0x30); hexlog("Stalin1945 roupa", d+0x80, 0x60);
    unsigned char *recs[1] = { r }; unsigned keys[1] = { key };
    lv_insert(CT, recs, keys, 1, "Stalin1945");
}


// ---------------------------------------------------------------------------
// Idioma do mod Brasil: o ARA nao tem localizacao para mods. Os textos ficam em
// Mods\Brasil\lang\<en|pt_BR>\*.zdata e a DLL copia a versao do idioma do jogo
// (Locale= em Settings_v121.txt) para Mods\Brasil\files\ antes do jogo ler os mods.
#include <shlobj.h>
static int same_file(const wchar_t *a, const wchar_t *b) {
    HANDLE ha=CreateFileW(a,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); if (ha==INVALID_HANDLE_VALUE) return 0;
    HANDLE hb=CreateFileW(b,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); if (hb==INVALID_HANDLE_VALUE){CloseHandle(ha);return 0;}
    int same=1; static char ba[65536], bb[65536];
    for(;;){ DWORD ra=0,rb=0; ReadFile(ha,ba,sizeof ba,&ra,NULL); ReadFile(hb,bb,sizeof bb,&rb,NULL);
        if (ra!=rb || memcmp(ba,bb,ra)) { same=0; break; } if (!ra) break; }
    CloseHandle(ha); CloseHandle(hb); return same;
}
static wchar_t g_voice_lang[8] = L"en";   // idioma da narracao do mod (v1.18)
static void mod_language_sync(void) {
    wchar_t settings[MAX_PATH], docs[MAX_PATH];
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", settings, MAX_PATH)) return;
    wcscat(settings, L"\\Ara History Untold\\Settings_v121.txt");
    if (SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, docs) != S_OK) return;
    const wchar_t *lang = L"en";
    FILE *f = _wfopen(settings, L"rb");
    if (f) { char line[512];
        while (fgets(line, sizeof line, f)) if (!strncmp(line, "Locale=", 7)) {
            if (!strncmp(line+7, "pt_BR", 5) || !strncmp(line+7, "pt-BR", 5)) lang = L"pt_BR";
            wcsncpy(g_voice_lang, lang, 7);
            break; }
        fclose(f); }
    static const wchar_t *mods[] = { L"Brasil", L"URSS" };   // mods de nacao com textos em lang\<idioma>
    for (unsigned m=0; m<sizeof mods/sizeof *mods; m++) {
        wchar_t base[MAX_PATH]; _snwprintf(base, MAX_PATH, L"%ls\\My Games\\Ara History Untold\\Mods\\%ls", docs, mods[m]);
        int copied = 0;
        wchar_t pat[MAX_PATH]; _snwprintf(pat, MAX_PATH, L"%ls\\lang\\%ls\\*.zdata", base, lang);
        WIN32_FIND_DATAW fd; HANDLE fh = FindFirstFileW(pat, &fd);
        if (fh == INVALID_HANDLE_VALUE) continue;           // mod nao instalado
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            wchar_t src[MAX_PATH], dst[MAX_PATH];
            _snwprintf(src, MAX_PATH, L"%ls\\lang\\%ls\\%ls", base, lang, fd.cFileName);
            _snwprintf(dst, MAX_PATH, L"%ls\\files\\%ls", base, fd.cFileName);
            if (same_file(src, dst)) continue;
            if (CopyFileW(src, dst, FALSE)) copied++; else ulog("Falha ao copiar texto do mod (%lu)", GetLastError());
        } while (FindNextFileW(fh, &fd));
        FindClose(fh);
        ulog("Mod %ls: idioma %ls (%d arquivo(s) de texto atualizados)", mods[m], lang, copied);
    }
}


// Diagnostico: registro de atlas da UI (RVA 0x1c81c240, mapa em +0x4d83e0: chave u64 = atlasHash | idHash<<32)
#define RVA_ATLAS_REG 0x1c81c240ull
static int g_atlas_diag_done;
static void atlas_diag(void) {
    if (g_atlas_diag_done) return;
    static ULONGLONG t0; if (!t0) t0 = GetTickCount64();
    if (GetTickCount64() - t0 < 25000) return;
    g_atlas_diag_done = 1;
    unsigned char *reg = g_base + RVA_ATLAS_REG + 0x4d83e0;
    if (!readable(reg, 0x20)) { ulog("Atlas: registro ilegivel"); return; }
    unsigned extra = *(unsigned*)(reg+0), sorted = *(unsigned*)(reg+8);
    unsigned char *ents = *(unsigned char**)(reg+0x18);
    unsigned total = sorted + extra;
    ulog("Atlas registro: extra %u ordenadas %u ptr %p", extra, sorted, ents);
    if (!ents || !total || total > 5000000 || !readable(ents,16) || !readable(ents+(size_t)(total-1)*0x10,16)) { ulog("Atlas: entradas ilegiveis"); return; }
    const char *atl[] = {"RGBEmblems","NationEmblems","LeaderSquare_256","LeaderEmblems_256","Leaders_Right_Facing"};
    const char *ids[] = {"Brazil","Argentina","GetulioVargas","HowardFlorey"};
    for (int x=0;x<5;x++){ unsigned ah=mm3(atl[x]); unsigned cnt=0;
        for (unsigned i=0;i<total;i++) if ((unsigned)*(unsigned long long*)(ents+(size_t)i*0x10)==ah) cnt++;
        ulog("  %s (%08x): %u entradas", atl[x], ah, cnt);
        for (int y=0;y<4;y++){ unsigned long long key=(unsigned long long)ah | ((unsigned long long)mm3(ids[y])<<32);
            int found=-1; for (unsigned i=0;i<total;i++) if (*(unsigned long long*)(ents+(size_t)i*0x10)==key){found=(int)i;break;}
            if (found>=0) ulog("    %s -> idx %d valor %08x %08x", ids[y], found, *(unsigned*)(ents+(size_t)found*0x10+8), *(unsigned*)(ents+(size_t)found*0x10+0xc));
            else ulog("    %s -> NAO ENCONTRADO", ids[y]); } }
    // amostra das primeiras chaves para conferir o formato
    for (unsigned i=0;i<4 && i<total;i++) ulog("  amostra %u: %016llx %08x", i, *(unsigned long long*)(ents+(size_t)i*0x10), *(unsigned*)(ents+(size_t)i*0x10+8));
}


// Diagnostico 2: acha o elemento "Emblem" da tela de selecao (callback de desenho = RVA 0x664420 em +0x160,
// contexto RVA 0x3f6bc02c em +0x158, hash da nacao em +0x168) e grava o conteudo quando a nacao muda.
static unsigned char *g_emb; static unsigned g_emb_last; static int g_emb_dumps; static ULONGLONG g_emb_scan_t;
static void emblem_diag(void) {
    if (g_emb_dumps >= 8) return;
    unsigned long long cb = (unsigned long long)(g_base + 0x664420), ctx = (unsigned long long)(g_base + 0x3f6bc02cull);
    if (!g_emb) {
        if (GetTickCount64() - g_emb_scan_t < 8000) return;
        g_emb_scan_t = GetTickCount64();
        unsigned char *p = (unsigned char*)0x10000; MEMORY_BASIC_INFORMATION mbi;
        while (VirtualQuery(p, &mbi, sizeof mbi)) {
            unsigned char *b=(unsigned char*)mbi.BaseAddress; size_t n=mbi.RegionSize;
            if (mbi.State==MEM_COMMIT && mbi.Type==MEM_PRIVATE && (mbi.Protect==PAGE_READWRITE) && n < (512u<<20)) {
                static unsigned long long buf[(1<<20)/8 + 64];
                for (size_t off=0; off<n && !g_emb; off+=(1<<20)) {
                    size_t len = n-off < (1u<<20) ? n-off : (1u<<20); SIZE_T got=0;
                    if (!ReadProcessMemory(GetCurrentProcess(), b+off, buf, len, &got) || got<len) continue;
                    for (size_t q=1; q<len/8; q++) if (buf[q]==cb && buf[q-1]==ctx && off+q*8>=0x160) { g_emb=b+off+q*8-0x160; break; }
                }
                if (g_emb) break;
            }
            p = b + n; if ((unsigned long long)p > 0x7fffffff0000ull) break;
        }
        if (g_emb) ulog("Emblem: elemento em %p", g_emb); else return;
    }
    static unsigned char snap[0x200]; SIZE_T got=0;
    if (!ReadProcessMemory(GetCurrentProcess(), g_emb, snap, 0x200, &got) || got<0x200) { g_emb=NULL; return; }
    if (*(unsigned long long*)(snap+0x160)!=cb) { g_emb=NULL; return; }
    unsigned nat = *(unsigned*)(snap+0x168);
    if (nat == g_emb_last) return;
    g_emb_last = nat; g_emb_dumps++;
    ulog("Emblem: nacao %08x", nat);
    for (int r=0;r<0x200;r+=0x40) { char b[300]; int k=0; for (int i=0;i<0x40;i+=4) k+=sprintf(b+k,"%08x ",*(unsigned*)(snap+r+i)); ulog("  +%03x: %s", r, b); }
}

// ---------------------------------------------------------------------------
// v1.18: narracao de apresentacao dos lideres moddados. O jogo posta o evento Wwise "VO_Narrator_LeaderIntro_Play"
// com o switch "LeaderType" = valor do lider pela funcao RVA 0x4f54e0 (rcx, &evento{ptr,len}, &grupo, &valor).
// Hook por trampolim (prologo de 15 bytes): se o valor for de um lider moddado, toca Mods\<mod>\voz\<idioma>.wav
// (voz sintetica gerada offline). Qualquer outra apresentacao interrompe a nossa.
#include <mmsystem.h>
#define RVA_POST_SWITCH_EVENT 0x4f54e0ull
static const unsigned char SIG_POST[15] = {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18};
typedef struct { const char *p; int n; } akstr_t;
typedef unsigned long long (*post_fn_t)(void*, akstr_t*, akstr_t*, akstr_t*, void*, void*);
static post_fn_t g_post_tramp;
static volatile LONG g_voice_req;      // 0 nada, 1 URSS, 2 Brasil, -1 parar
static HANDLE g_voice_evt;
static int g_vo_logs;
static int akeq(akstr_t *s, const char *z) { size_t n=strlen(z); return s && readable((void*)s,16) && s->n==(int)n && s->p && readable((void*)s->p,n) && !memcmp(s->p,z,n); }
static int akhas(akstr_t *s, const char *z) {
    if (!s || !readable((void*)s,16) || s->n<=0 || s->n>200 || !s->p || !readable((void*)s->p,s->n)) return 0;
    char b[201]; memcpy(b,s->p,s->n); b[s->n]=0; for (char *q=b;*q;q++) if(*q>='A'&&*q<='Z') *q+=32;
    return strstr(b,z)!=NULL;
}
static unsigned long long hook_post(void *a, akstr_t *ev, akstr_t *grp, akstr_t *val, void *e, void *f) {
    if (ev && readable((void*)ev,16) && ev->n>12 && ev->n<64 && ev->p && readable((void*)ev->p,ev->n) && !memcmp(ev->p,"VO_Narrator_",12)) {
        int intro = akeq(ev, "VO_Narrator_LeaderIntro_Play");
        if (g_vo_logs < 60) { g_vo_logs++;
            char e1[65]={0}, v1[129]={0}, g1[65]={0};
            memcpy(e1, ev->p, ev->n);
            if (grp && readable((void*)grp,16) && grp->n>0 && grp->n<64 && readable((void*)grp->p,grp->n)) memcpy(g1,grp->p,grp->n);
            if (val && readable((void*)val,16) && val->n>0 && val->n<128 && readable((void*)val->p,val->n)) memcpy(v1,val->p,val->n);
            ulog("VO: %s [%s=%s]", e1, g1, v1); }
        if (intro) {
            LONG r = akhas(val,"stalin") ? 1 : (akhas(val,"vargas") || akhas(val,"getulio")) ? 2 : -1;
            InterlockedExchange(&g_voice_req, r); if (g_voice_evt) SetEvent(g_voice_evt);
        }
    }
    return g_post_tramp(a, ev, grp, val, e, f);
}
static DWORD WINAPI voice_thread(LPVOID unused) {
    (void)unused; wchar_t docs[MAX_PATH]; if (SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, docs) != S_OK) return 0;
    for (;;) {
        WaitForSingleObject(g_voice_evt, INFINITE);
        LONG r = InterlockedExchange(&g_voice_req, 0);
        if (r == -1) { PlaySoundW(NULL, NULL, 0); continue; }
        if (r != 1 && r != 2) continue;
        const wchar_t *mod = r == 1 ? L"URSS" : L"Brasil"; wchar_t path[MAX_PATH];
        _snwprintf(path, MAX_PATH, L"%ls\\My Games\\Ara History Untold\\Mods\\%ls\\voz\\%ls.wav", docs, mod, g_voice_lang);
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
            _snwprintf(path, MAX_PATH, L"%ls\\My Games\\Ara History Untold\\Mods\\%ls\\voz\\en.wav", docs, mod);
        if (!PlaySoundW(path, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) ulog("VO: falha ao tocar %ls", path);
        else ulog("VO: narracao do mod %ls (%ls)", mod, g_voice_lang);
    }
}
static void install_voice_hook(void) {
    unsigned char *t = g_base + RVA_POST_SWITCH_EVENT;
    if (memcmp(t, SIG_POST, sizeof SIG_POST)) { ulog("VO: assinatura da funcao de evento nao confere. Narracao desligada."); return; }
    unsigned char *tr = (unsigned char*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) return;
    memcpy(tr, t, 15);                                                  // prologo original (sem enderecos relativos)
    tr[15]=0xFF; tr[16]=0x25; *(unsigned*)(tr+17)=0; *(unsigned long long*)(tr+21)=(unsigned long long)(t+15);
    g_post_tramp = (post_fn_t)tr;
    g_voice_evt = CreateEventW(NULL, FALSE, FALSE, NULL);
    HANDLE th = CreateThread(NULL, 0, voice_thread, NULL, 0, NULL); if (th) CloseHandle(th);
    DWORD old; if (!VirtualProtect(t, 16, PAGE_EXECUTE_READWRITE, &old)) { ulog("VO: VirtualProtect falhou"); return; }
    unsigned char j[15] = {0xFF,0x25,0,0,0,0, 0,0,0,0,0,0,0,0, 0x90};
    *(unsigned long long*)(j+6) = (unsigned long long)hook_post;
    memcpy(t, j, 15); VirtualProtect(t, 16, old, &old); FlushInstructionCache(GetCurrentProcess(), t, 16);
    ulog("VO: hook da narracao instalado (idioma %ls).", g_voice_lang);
}

static DWORD WINAPI watcher(LPVOID unused) {
    mod_language_sync();
    (void)unused;
    unsigned char **slot = (unsigned char **)(g_base + RVA_MAINMENU + OFF_DEBUGBUTTON);
    void *last_elem = NULL;
    for (;;) {
        ent_poll();
        lv_poll();
        lv_poll_stalin();
        lv_poll_stalin1945();
        if (!g_enable_builder) { Sleep(300); continue; }
        unsigned char *elem = readable(slot, 8) ? *slot : NULL;
        if (elem && readable(elem + OFF_CB_CTX, 0x10)) {
            void **pctx = (void **)(elem + OFF_CB_CTX);
            void **pfn  = (void **)(elem + OFF_CB_FN);
            if (*pfn != (void *)OnDebugButtonClick) {
                if (*pfn) ulog("Aviso: DebugButton ja tinha callback %p (substituido)", *pfn);
                *pctx = elem;                                // contexto nao-nulo
                MemoryBarrier();
                *pfn = (void *)OnDebugButtonClick;
                ulog("DebugButton %p ligado ao Leader Builder (clique no logo do ARA no menu principal)", elem);
            }
            last_elem = elem;
        } else if (last_elem && !elem) {
            last_elem = NULL;
        }
        Sleep(300);
    }
    return 0;
}


// ---------------------------------------------------------------------------
// Correcao das montanhas pretas (RTX 50xx): a tabela global de samplers do jogo declara 3 samplers
// ANISOTROPIC (filtro 0x55) com MaxAnisotropy = 0, valor fora do intervalo valido do D3D12 (1..16).
// Aqui o valor e trocado para 16 antes de o jogo criar os samplers. Tabela: 20 entradas de 40 bytes
// {Filter, AddrU, AddrV, AddrW, MipLODBias, MaxAnisotropy, ComparisonFunc, BorderColor, MinLOD, MaxLOD}.
#define RVA_SAMPLER_TABLE 0x24c7f2cull
static __attribute__((unused)) void fix_samplers(void) {
    unsigned char *t = g_base + RVA_SAMPLER_TABLE;
    if (!readable(t - 12, 12 + 20 * 40)) { ulog("Samplers: tabela ilegivel"); return; }
    if (*(unsigned*)(t - 12) != 8 || *(unsigned*)(t - 8) != 0x20 || *(unsigned*)(t - 4) != 20) { ulog("Samplers: cabecalho diferente do esperado. Nada alterado."); return; }
    int idx[3] = { 2, 3, 13 }, n = 0;
    for (int i = 0; i < 3; i++) {
        unsigned *e = (unsigned*)(t + idx[i] * 40);
        if (e[0] != 0x55 || e[5] != 0) { ulog("Samplers: entrada %d inesperada (filtro %x aniso %u). Nada alterado.", idx[i], e[0], e[5]); return; }
    }
    DWORD old;
    if (!VirtualProtect(t, 20 * 40, PAGE_READWRITE, &old)) { ulog("Samplers: VirtualProtect falhou (%lu)", GetLastError()); return; }
    for (int i = 0; i < 3; i++) { ((unsigned*)(t + idx[i] * 40))[5] = 16; n++; }
    VirtualProtect(t, 20 * 40, old, &old);
    ulog("Samplers: MaxAnisotropy 0 -> 16 em %d samplers anisotropicos (correcao RTX 50xx).", n);
}

static int verify_target(void) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)g_base;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(g_base + dos->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != EXPECTED_TIMESTAMP || nt->OptionalHeader.SizeOfImage != EXPECTED_SIZEOFIMAGE) {
        ulog("Executavel diferente do esperado (timestamp %08x, size %08x). Nada foi alterado.",
             (unsigned)nt->FileHeader.TimeDateStamp, (unsigned)nt->OptionalHeader.SizeOfImage);
        return 0;
    }
    if (memcmp(g_base + RVA_LEADERBTN_HANDLER, SIG_HANDLER, sizeof SIG_HANDLER) != 0) {
        ulog("Assinatura do handler LEADER BUILDER nao confere. Nada foi alterado."); return 0;
    }
    if (memcmp(g_base + RVA_MAINMENU_BIND, SIG_MAINMENU_BIND, sizeof SIG_MAINMENU_BIND) != 0) {
        ulog("Assinatura do MainMenu/DebugButton nao confere. Nada foi alterado."); return 0;
    }
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
        GetModuleFileNameA(h, g_logpath, MAX_PATH);
        char *s = strrchr(g_logpath, '\\');
        strcpy(s ? s + 1 : g_logpath, "AraLeaderBuilderUnlock.log");
        g_base = (unsigned char *)GetModuleHandleA(NULL);
        char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
        const char *en = strrchr(exe, '\\'); en = en ? en + 1 : exe;
        if (_stricmp(en, "AraHistoryUntold.exe") != 0) return TRUE;   // outro processo: so repassa
        ulog("=== AraLeaderBuilderUnlock 1.18 carregado em %s", exe);
        if (!verify_target()) return TRUE;
        g_orig_handler = (click_handler_t)(g_base + RVA_LEADERBTN_HANDLER);
        allow("ldr_GetulioVargas"); allow("ntn_Brazil"); allow("ldr_c_1930"); allow("ldr_c_1937"); allow("ldr_c_1951"); allow("ldr_c_1954");
        allow("ldr_JosefStalin"); allow("ntn_SovietUnion"); allow("ldr_c_1924"); allow("ldr_c_1945");
        // v1.12: fix_samplers() removido (nao corrigia o RTX 50xx e quebrava o hash da ZNOShaders.Samplers)
        ent_poll();
        install_voice_hook();
        HANDLE t = CreateThread(NULL, 0, watcher, NULL, 0, NULL);
        if (t) CloseHandle(t);
        ulog("Build 2.1.0.542 confirmado. Aguardando o menu principal...");
    }
    return TRUE;
}
