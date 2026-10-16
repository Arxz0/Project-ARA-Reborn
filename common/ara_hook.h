// ara_hook.h - gancho de inicio de funcao que convive com os outros mods (AraQoL, Ferrovias, ...).
// Cada mod e uma DLL independente; quando duas DLLs engancham a mesma funcao do jogo, a segunda entra NA FRENTE da
// primeira em vez de recusar: o "original" dela passa a ser o gancho que ja estava la. Assim cada mod funciona
// sozinho ou junto com os outros, em qualquer ordem de carga.
// Formato do gancho (o mesmo em todos os mods): FF 25 00000000 <endereco de 8 bytes> e NOPs ate n bytes.
// Uso: definir antes do include  static void hook_log(const char *fmt, ...);
#ifndef ARA_HOOK_H
#define ARA_HOOK_H

static int ara_is_absjmp(const unsigned char *t) { return t[0] == 0xFF && t[1] == 0x25 && *(const unsigned *)(t + 2) == 0; }

// t = inicio da funcao; pro/n = prologo esperado (sem enderecos relativos, n >= 14). Devolve o que chamar como
// "original" (trampolim ou gancho anterior) ou NULL sem alterar nada.
static void *ara_hook(unsigned char *t, const unsigned char *pro, int n, void *fn, const char *what) {
    if (n < 14 || n > 32) return NULL;
    unsigned char *tr = (unsigned char *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) return NULL;
    DWORD old;
    if (ara_is_absjmp(t)) {
        // outro mod ja enganchou: o trampolim pula para o gancho dele e o nosso toma o lugar no jmp
        unsigned long long prev = *(unsigned long long *)(t + 6);
        tr[0] = 0xFF; tr[1] = 0x25; *(unsigned *)(tr + 2) = 0; *(unsigned long long *)(tr + 6) = prev;
        FlushInstructionCache(GetCurrentProcess(), tr, 64);
        if (!VirtualProtect(t, 14, PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(tr, 0, MEM_RELEASE); return NULL; }
        *(volatile unsigned long long *)(t + 6) = (unsigned long long)fn;
        VirtualProtect(t, 14, old, &old); FlushInstructionCache(GetCurrentProcess(), t, 14);
        hook_log("Gancho de %s encadeado com outro mod (anterior %p).", what, (void *)prev);
        return tr;
    }
    if (memcmp(t, pro, n)) { hook_log("Prologo de %s nao confere; nada alterado.", what); VirtualFree(tr, 0, MEM_RELEASE); return NULL; }
    memcpy(tr, t, n);
    tr[n] = 0xFF; tr[n + 1] = 0x25; *(unsigned *)(tr + n + 2) = 0; *(unsigned long long *)(tr + n + 6) = (unsigned long long)(t + n);
    FlushInstructionCache(GetCurrentProcess(), tr, 64);
    if (!VirtualProtect(t, n, PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(tr, 0, MEM_RELEASE); return NULL; }
    unsigned char j[32]; memset(j, 0x90, sizeof j);
    j[0] = 0xFF; j[1] = 0x25; memset(j + 2, 0, 4); *(unsigned long long *)(j + 6) = (unsigned long long)fn;
    memcpy(t, j, n); VirtualProtect(t, n, old, &old); FlushInstructionCache(GetCurrentProcess(), t, n);
    return tr;
}

#endif
