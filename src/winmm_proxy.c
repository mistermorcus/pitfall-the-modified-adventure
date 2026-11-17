#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* The retail executable imports WINMM at process startup but loads WAIL32
   later.  This small proxy therefore provides the early, byte-identical-EXE
   bootstrap.  Every multimedia call is forwarded to the real system DLL. */

#define FORWARDED_FUNCTIONS(X) \
    X(auxGetDevCapsA) X(auxGetNumDevs) X(auxGetVolume) X(auxSetVolume) \
    X(joyGetDevCapsA) X(joyGetNumDevs) X(joyGetPos) X(joyReleaseCapture) \
    X(joySetCapture) X(mciSendCommandA) X(mciSendStringA) X(sndPlaySoundA) \
    X(timeBeginPeriod) X(timeEndPeriod) X(timeGetTime) X(timeKillEvent) \
    X(timeSetEvent) X(waveOutClose) X(waveOutGetNumDevs) \
    X(waveOutGetPosition) X(waveOutOpen) X(waveOutPause) \
    X(waveOutPrepareHeader) X(waveOutReset) X(waveOutRestart) \
    X(waveOutSetVolume) X(waveOutUnprepareHeader) X(waveOutWrite)

#define DECLARE_SLOT(name) void *real_##name;
FORWARDED_FUNCTIONS(DECLARE_SLOT)

#define DECLARE_STUB(name) \
    __asm__(".globl _proxy_" #name "\n" \
            "_proxy_" #name ": jmp *_real_" #name "\n");
FORWARDED_FUNCTIONS(DECLARE_STUB)

static BOOL bind_real_winmm(void) {
    char path[MAX_PATH];
    HMODULE module;
    if (!GetSystemDirectoryA(path, MAX_PATH)) return FALSE;
    if (lstrlenA(path) + 10 >= MAX_PATH) return FALSE;
    lstrcatA(path, "\\winmm.dll");
    module = LoadLibraryA(path);
    if (!module) return FALSE;
#define BIND_SLOT(name) \
    real_##name = (void *)GetProcAddress(module, #name); \
    if (!real_##name) return FALSE;
    FORWARDED_FUNCTIONS(BIND_SLOT)
#undef BIND_SLOT
    return TRUE;
}

static BOOL load_pitmod(HINSTANCE instance) {
    char path[MAX_PATH], *cursor;
    if (!GetModuleFileNameA(instance, path, MAX_PATH)) return FALSE;
    cursor = path + lstrlenA(path);
    while (cursor > path && cursor[-1] != '\\' && cursor[-1] != '/') --cursor;
    lstrcpyA(cursor, "PITMOD.DLL");
    return LoadLibraryA(path) != NULL;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        if (!bind_real_winmm()) return FALSE;
        if (!load_pitmod(instance)) return FALSE;
    }
    return TRUE;
}
