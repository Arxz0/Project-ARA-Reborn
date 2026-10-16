// ARA CPU Fix 1.0 (Project ARA: Reborn) - stops the whole-PC freeze during end-of-turn processing in ARA: History Untold 2.1.0.542.
// Loaded as a proxy of avutil-58.dll (the original is kept as avutil-58_orig.dll and every export is forwarded to it).
//
// The problem (measured on 15 Oct 2026 on an i7-11700KF, 16 logical processors): the game creates one worker thread per
// logical processor minus one, pins each worker to a processor (SetThreadAffinityMask/SetThreadIdealProcessor), the workers
// busy-spin when idle with a current priority of 16, and the process is raised to ABOVE_NORMAL while the window has focus.
// During turn processing every processor is busy above Windows itself and the whole system can lock up.
//
// The fix (in memory only, no game file is changed):
//  1. CPU: reserves the first N logical processors for Windows. The process affinity excludes them; the game's calls to
//     SetThreadAffinityMask/SetThreadIdealProcessor are limited to the allowed set; GetSystemInfo and
//     GetLogicalProcessorInformation only report the allowed processors (the game creates fewer workers);
//     SetProcessPriorityBoost always disables the dynamic boost. Only the executable's import table is redirected.
//     ARA_CpuFix.ini [CPU] ReservedProcessors = auto (16+ logical: 4; 12+: 2; 6+: 1; fewer: 0), a number, or 0 to disable.
//  2. Priority: while focused the game stays at NORMAL instead of ABOVE_NORMAL (0xbf5790 calls SetPriorityClass with 0x8000;
//     the immediate at 0xbf57c2 becomes 0x20). [Priority] CapAtNormal = 1 (0 = original behaviour).
// Applied at game start. Log: bin\ARA_CpuFix.log.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define EXPECTED_TIMESTAMP   0x6a286b53u
#define EXPECTED_SIZEOFIMAGE 0x5e622e00u
#define RVA_PRIO_ABOVE       0x00bf57b8ull   // call GetCurrentProcess / mov rcx,rax / mov edx,8000h / call SetPriorityClass

static unsigned char *g_base;
static char g_logpath[MAX_PATH], g_inipath[MAX_PATH];

static void qlog(const char *fmt, ...) {
    FILE *f = fopen(g_logpath, "a"); if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}
static int mem_ok(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m;
    if (!p || !VirtualQuery(p, &m, sizeof m) || m.State != MEM_COMMIT || (m.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    return (const unsigned char *)p + n <= (const unsigned char *)m.BaseAddress + m.RegionSize;
}
static int same_build(void) {
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(g_base + ((IMAGE_DOS_HEADER *)g_base)->e_lfanew);
    return nt->FileHeader.TimeDateStamp == EXPECTED_TIMESTAMP && nt->OptionalHeader.SizeOfImage == EXPECTED_SIZEOFIMAGE;
}
static void ini_get(const char *sec, const char *key, const char *def, char *v, DWORD n) {
    GetPrivateProfileStringA(sec, key, "", v, n, g_inipath);
    if (!v[0]) { WritePrivateProfileStringA(sec, key, def, g_inipath); strcpy(v, def); }
}

// ---- CPU ----
static ULONG_PTR g_cpu_allowed;
static int g_cpu_list[64], g_cpu_n;
static DWORD_PTR (WINAPI *o_SetThreadAffinityMask)(HANDLE, DWORD_PTR);
static DWORD (WINAPI *o_SetThreadIdealProcessor)(HANDLE, DWORD);
static void (WINAPI *o_GetSystemInfo)(LPSYSTEM_INFO);
static BOOL (WINAPI *o_GetLogicalProcessorInformation)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
static BOOL (WINAPI *o_SetProcessPriorityBoost)(HANDLE, BOOL);
static volatile LONG g_cpu_logs;

static DWORD_PTR WINAPI h_SetThreadAffinityMask(HANDLE t, DWORD_PTR m) {
    DWORD_PTR m2 = m & g_cpu_allowed;
    if (!m2) m2 = g_cpu_allowed;
    if (m2 != m && InterlockedIncrement(&g_cpu_logs) <= 40) qlog("CPU: game asked for affinity %llx -> %llx.", (unsigned long long)m, (unsigned long long)m2);
    return o_SetThreadAffinityMask(t, m2);
}
static DWORD WINAPI h_SetThreadIdealProcessor(HANDLE t, DWORD n) {
    if (n < 64 && !(g_cpu_allowed >> n & 1) && g_cpu_n) n = (DWORD)g_cpu_list[n % g_cpu_n];
    return o_SetThreadIdealProcessor(t, n);
}
static void WINAPI h_GetSystemInfo(LPSYSTEM_INFO si) {
    o_GetSystemInfo(si);
    if (si) { si->dwActiveProcessorMask &= g_cpu_allowed; si->dwNumberOfProcessors = (DWORD)g_cpu_n; }
}
static BOOL WINAPI h_GetLogicalProcessorInformation(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION b, PDWORD len) {
    BOOL r = o_GetLogicalProcessorInformation(b, len);
    if (!r || !b || !len) return r;
    DWORD n = *len / sizeof *b, k = 0;
    for (DWORD i = 0; i < n; i++) { b[i].ProcessorMask &= g_cpu_allowed; if (b[i].ProcessorMask) b[k++] = b[i]; }
    *len = k * sizeof *b;
    return r;
}
static BOOL WINAPI h_SetProcessPriorityBoost(HANDLE p, BOOL disable) { (void)disable; return o_SetProcessPriorityBoost(p, TRUE); }

// swaps one entry of the executable's import table (by name); returns the original address or NULL
static void *iat_swap(const char *dll, const char *fn, void *nw) {
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(g_base + ((IMAGE_DOS_HEADER *)g_base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dd->VirtualAddress) return NULL;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(g_base + dd->VirtualAddress); d->Name; d++) {
        if (_stricmp((char *)(g_base + d->Name), dll)) continue;
        IMAGE_THUNK_DATA64 *o = (IMAGE_THUNK_DATA64 *)(g_base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        IMAGE_THUNK_DATA64 *f = (IMAGE_THUNK_DATA64 *)(g_base + d->FirstThunk);
        for (; o->u1.AddressOfData; o++, f++) {
            if (IMAGE_SNAP_BY_ORDINAL64(o->u1.Ordinal)) continue;
            if (strcmp((char *)((IMAGE_IMPORT_BY_NAME *)(g_base + o->u1.AddressOfData))->Name, fn)) continue;
            void *old = (void *)f->u1.Function; DWORD pr; HMODULE hm;   // only swap an import that is already resolved
            if (!old || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)old, &hm)
                || (unsigned char *)hm == g_base) return NULL;
            if (!VirtualProtect(&f->u1.Function, 8, PAGE_READWRITE, &pr)) return NULL;
            f->u1.Function = (ULONGLONG)nw;
            VirtualProtect(&f->u1.Function, 8, pr, &pr);
            return old;
        }
    }
    return NULL;
}
static void cpu_install(void) {
    char v[16]; ini_get("CPU", "ReservedProcessors", "auto", v, sizeof v);
    DWORD_PTR pm = 0, sm = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm)) { qlog("CPU: could not read the process affinity; nothing changed."); return; }
    int total = 0; for (int i = 0; i < 64; i++) if (pm >> i & 1) total++;
    int reserve = _stricmp(v, "auto") ? atoi(v) : total >= 16 ? 4 : total >= 12 ? 2 : total >= 6 ? 1 : 0;
    if (reserve <= 0) { qlog("CPU: nothing reserved (ReservedProcessors=%s, %d logical processors).", v, total); return; }
    if (total - reserve < 2) { qlog("CPU: only %d logical processors; nothing changed.", total); return; }
    ULONG_PTR allowed = 0; int skip = reserve;
    for (int i = 0; i < 64; i++) if (pm >> i & 1) { if (skip > 0) skip--; else allowed |= (ULONG_PTR)1 << i; }
    g_cpu_allowed = allowed; g_cpu_n = 0;
    for (int i = 0; i < 64; i++) if (allowed >> i & 1) g_cpu_list[g_cpu_n++] = i;
    if (!SetProcessAffinityMask(GetCurrentProcess(), allowed)) { qlog("CPU: SetProcessAffinityMask failed (%lu); nothing changed.", GetLastError()); return; }
    SetProcessPriorityBoost(GetCurrentProcess(), TRUE);
    int ok = 0;
    if ((o_SetThreadAffinityMask = iat_swap("KERNEL32.dll", "SetThreadAffinityMask", (void *)h_SetThreadAffinityMask))) ok++;
    if ((o_SetThreadIdealProcessor = iat_swap("KERNEL32.dll", "SetThreadIdealProcessor", (void *)h_SetThreadIdealProcessor))) ok++;
    if ((o_GetSystemInfo = iat_swap("KERNEL32.dll", "GetSystemInfo", (void *)h_GetSystemInfo))) ok++;
    if ((o_GetLogicalProcessorInformation = iat_swap("KERNEL32.dll", "GetLogicalProcessorInformation", (void *)h_GetLogicalProcessorInformation))) ok++;
    if ((o_SetProcessPriorityBoost = iat_swap("KERNEL32.dll", "SetProcessPriorityBoost", (void *)h_SetProcessPriorityBoost))) ok++;
    qlog("CPU: %d of %d logical processors for the game (affinity %llx); %d reserved for Windows (ini: %s). Dynamic boost off. %d/5 imports redirected.",
         g_cpu_n, total, (unsigned long long)allowed, reserve, v, ok);
}

// ---- Priority ----
static void prio_install(void) {
    char v[16]; ini_get("Priority", "CapAtNormal", "1", v, sizeof v);
    if (atoi(v) != 1) { qlog("Priority: disabled in the ini (the game raises itself to above normal while focused)."); return; }
    if (!same_build()) { qlog("Priority: different game build; nothing changed."); return; }
    unsigned char *p = g_base + RVA_PRIO_ABOVE;
    static const unsigned char want[] = { 0xff,0x15 }, mid[] = { 0x48,0x8b,0xc8, 0xba,0x00,0x80,0x00,0x00, 0xff,0x15 };
    if (!mem_ok(p, 18) || memcmp(p, want, 2) || memcmp(p + 6, mid, sizeof mid)) { qlog("Priority: game code is not as expected; nothing changed."); return; }
    DWORD old;
    if (!VirtualProtect(p + 10, 4, PAGE_EXECUTE_READWRITE, &old)) { qlog("Priority: VirtualProtect failed."); return; }
    *(unsigned *)(p + 10) = 0x20;                       // NORMAL_PRIORITY_CLASS instead of ABOVE_NORMAL (0x8000)
    VirtualProtect(p + 10, 4, old, &old); FlushInstructionCache(GetCurrentProcess(), p + 10, 4);
    qlog("Priority: the game stays at NORMAL while focused (was ABOVE_NORMAL). In the background it is still below normal.");
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(h);
    GetModuleFileNameA(h, g_logpath, MAX_PATH);
    char *s = strrchr(g_logpath, '\\'); strcpy(s ? s + 1 : g_logpath, "ARA_CpuFix.log");
    strcpy(g_inipath, g_logpath); { char *q = strrchr(g_inipath, '.'); if (q) strcpy(q, ".ini"); }
    char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
    const char *en = strrchr(exe, '\\'); en = en ? en + 1 : exe;
    if (_stricmp(en, "AraHistoryUntold.exe") != 0) return TRUE;
    g_base = (unsigned char *)GetModuleHandleA(NULL);
    qlog("=== ARA CPU Fix 1.0 loaded (Project ARA: Reborn; proxy of avutil-58.dll)");
    cpu_install();                                      // independent of the game build (Windows APIs only)
    prio_install();                                     // build-specific 4-byte patch, checked first
    return TRUE;
}
