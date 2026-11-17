/*
 * WAIL32.DLL substituto para Pitfall: The Mayan Adventure (Win95, 1995)
 * Copyright (c) 2026 Alessandro Ramos - MIT License
 *
 * O jogo usa so 13 funcoes da Miles AIL 3.03. Esta versao usa DirectSound:
 * cada som (imagem WAV que o jogo passa) vira um buffer secundario criado
 * uma unica vez e reaproveitado; tocar/parar e instantaneo e a mistura e
 * feita pelo Windows. Sem threads, timers ou callbacks proprios.
 */
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <math.h>
#include <string.h>

/* A normal import—not a runtime LoadLibrary call—makes PITMOD.DLL a fixed
   dependency of WAIL32.DLL.  PITFALL.EXE already imports WAIL32, so this gets
   the compatibility layer loaded without changing a byte of the game. */
__declspec(dllimport) void WINAPI PitmodBootstrap(void);

#define SMP_FREE     0x0001
#define SMP_DONE     0x0002
#define SMP_PLAYING  0x0004

#define MAX_SAMPLES  16
#define MAX_CACHE    512

typedef struct CACHED {
    const BYTE         *file;   /* imagem WAV do jogo (chave) */
    DWORD               len;
    int                 owner;  /* indice do sample handle dono */
    IDirectSoundBuffer *buf;
} CACHED;

typedef struct SAMPLE {
    int                 used;
    int                 index;
    const BYTE         *file;
    const BYTE         *data;
    DWORD               len;
    WAVEFORMATEX        fmt;
    IDirectSoundBuffer *cur;    /* buffer tocando agora */
    int                 volume; /* 0..127 */
    unsigned            status;
} SAMPLE;

static IDirectSound *g_ds;
static IDirectSoundBuffer *g_primary;
static SAMPLE  g_samples[MAX_SAMPLES];
static CACHED  g_cache[MAX_CACHE];
static int     g_ncache;
static int     g_driver_dummy = 0x4D494C45;
static char    g_error[128] = "";
typedef void (WINAPI *PITMOD_RUMBLE_EVENT)(DWORD event);
static PITMOD_RUMBLE_EVENT g_rumble_event;
static DWORD sample_hash(const SAMPLE *s);
static DWORD g_catapult_stretch_at;

#define PITMOD_LOG_SAMPLES 0

static void sample_log_path(char *path)
{
    char *slash;
    if (!GetModuleFileNameA(NULL, path, MAX_PATH)) {
        path[0] = 0;
        return;
    }
    slash = path + lstrlenA(path);
    while (slash > path && slash[-1] != '\\' && slash[-1] != '/') --slash;
    lstrcpyA(slash, "wail_samples.log");
}

static void reset_sample_log(void)
{
#if PITMOD_LOG_SAMPLES
    char path[MAX_PATH];
    const char header[] = "milliseconds,hash,length,event\r\n";
    DWORD written;
    HANDLE file;
    sample_log_path(path);
    if (!path[0]) return;
    file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    WriteFile(file, header, sizeof(header) - 1, &written, NULL);
    CloseHandle(file);
#endif
}

static void log_sample_event(const SAMPLE *s, DWORD hash, DWORD event)
{
#if PITMOD_LOG_SAMPLES
    char path[MAX_PATH], line[96];
    DWORD written;
    HANDLE file;
    sample_log_path(path);
    if (!path[0]) return;
    wsprintfA(line, "%lu,%08lX,%lu,%lu\r\n",
              GetTickCount(), hash, s ? s->len : 0, event);
    file = CreateFileA(path, FILE_APPEND_DATA,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    WriteFile(file, line, lstrlenA(line), &written, NULL);
    CloseHandle(file);
#endif
}

static DWORD sample_hash(const SAMPLE *s)
{
    DWORD hash = 2166136261u;
    DWORD i;
    if (!s || !s->data) return 0;
    for (i = 0; i < s->len; ++i) {
        hash ^= s->data[i];
        hash *= 16777619u;
    }
    return hash;
}

static void send_rumble_for_sample(const SAMPLE *s)
{
    HMODULE mod;
    DWORD event = 0;
    DWORD hash;
    if (!s) return;
    hash = sample_hash(s);
    /* Exact content hashes prevent same-length effects from being confused. */
    if (hash == 0x5E3648B0u) event = 1; /* whip */
    else if (s->len == 1728) event = 2; /* throw release */
    else if (s->len == 2816) event = 3; /* thrown-object impact */
    /* This vocal/grunt is reused verbatim for grenade throws and one damage
       reaction, so use the moderate grenade/action profile for both. */
    else if (hash == 0x7907A6B8u) event = 4;
    else if (hash == 0x7CBB905Eu) event = 5; /* grenade explosion */
    else if (hash == 0x2E639E5Cu) event = 21; /* boomerang flight */
    else if (hash == 0xBD36E166u || /* Harry damage screams */
             hash == 0x894C6DFCu ||
             hash == 0xB58B32BDu) event = 7;
    else if (hash == 0x7BEDE038u) event = 8; /* jump takeoff */
    else if (hash == 0x97C3B80Eu) event = 9; /* landing */
    else if (hash == 0x4FDC304Eu) event = 10; /* vine/rope climbing */
    else if (hash == 0x674228B1u) event = 10; /* rope movement/grab */
    /* CD48E4CF is an ambient jungle chirp. It must not create rumble or
       interrupt a sustained effect such as meditation/hover. */
    else if (hash == 0xC7C8C349u) event = 15; /* angled rope slide */
    else if (hash == 0xA4EDCF58u) event = 16; /* wall-stun stars */
    else if (hash == 0x79C7F544u) event = 11; /* wall-stun impact */
    /* This wall-impact variant is also reused by supply-bag collection. */
    else if (hash == 0x73641DDAu) event = 12;
    else if (hash == 0xD7195AD6u || /* supply bag pickup */
             hash == 0xE4E1A1EAu) event = 12;
    else if (hash == 0x73C8A5F4u || /* treasure pickup */
             hash == 0x86541A60u || /* heart pickup */
             hash == 0x0C788E2Eu) event = 14; /* ring / "oh yeah" */
    else if (hash == 0x6748C37Du) event = 13; /* coin pickup */
    else if (hash == 0x896BFC28u ||
             hash == 0x3C95ECB1u) event = 18; /* spiderweb trampoline variants */
    else if (hash == 0xF57DB45Du) event = 19; /* extra life */
    else if (hash == 0xB7994200u) event = 20; /* bubbling mud death */
    else if (hash == 0x21B870F1u) event = 22; /* grinding rock door */
    else if (hash == 0xF5C06B96u) {
        event = 23; /* slime catapult stretch */
        g_catapult_stretch_at = GetTickCount();
    }
    /* This PCM is reused as an ambient chirp. It is the catapult launch only
       when it immediately follows the distinctive stretch sample. */
    else if (hash == 0x4D56316Fu &&
             GetTickCount() - g_catapult_stretch_at < 2000) event = 24;
    else if (hash == 0x899F5C9Au) event = 25; /* hover / meditation power-up */
    else if (hash == 0xDD88BDA6u) event = 26; /* nunchuck idle flourish */
    else if (hash == 0xC256EBA5u) event = 27; /* enemy destruction */
    else if (hash == 0xAD635836u) event = 28; /* bricks sliding out of wall */
    else if (hash == 0x35B0669Bu) event = 29; /* pushing a minecart */
    else if (hash == 0x7164C304u) event = 28; /* moving stone platform/block */
    /* 9C241D41 is the nearby heart's roughly once-per-second pump. It stays
       audible but deliberately does not produce rumble. */
    log_sample_event(s, hash, event);
    if (!event) return;
    if (!g_rumble_event) {
        mod = GetModuleHandleA("PITMOD.DLL");
        if (mod) {
            g_rumble_event = (PITMOD_RUMBLE_EVENT)
                GetProcAddress(mod, "PitmodRumbleEvent");
            if (!g_rumble_event)
                g_rumble_event = (PITMOD_RUMBLE_EVENT)
                    GetProcAddress(mod, "PitmodRumbleEvent@4");
            if (!g_rumble_event)
                g_rumble_event = (PITMOD_RUMBLE_EVENT)
                    GetProcAddress(mod, "_PitmodRumbleEvent@4");
        }
    }
    if (g_rumble_event) g_rumble_event(event);
}

static HWND find_game_window(void)
{
    HWND h = GetActiveWindow();
    if (!h) h = GetForegroundWindow();
    if (!h) h = GetDesktopWindow();
    return h;
}

static int ds_init(void)
{
    DSBUFFERDESC d;
    WAVEFORMATEX wf;
    if (g_ds) return 1;
    if (FAILED(DirectSoundCreate(NULL, &g_ds, NULL))) {
        g_ds = NULL;
        lstrcpyA(g_error, "DirectSoundCreate failed");
        return 0;
    }
    if (FAILED(IDirectSound_SetCooperativeLevel(g_ds, find_game_window(), DSSCL_PRIORITY)))
        IDirectSound_SetCooperativeLevel(g_ds, GetDesktopWindow(), DSSCL_NORMAL);
    /* formato do buffer primario: 44.1 kHz 16-bit estereo */
    memset(&d, 0, sizeof(d));
    d.dwSize = sizeof(d);
    d.dwFlags = DSBCAPS_PRIMARYBUFFER;
    if (SUCCEEDED(IDirectSound_CreateSoundBuffer(g_ds, &d, &g_primary, NULL))) {
        memset(&wf, 0, sizeof(wf));
        wf.wFormatTag = WAVE_FORMAT_PCM; wf.nChannels = 2; wf.nSamplesPerSec = 44100;
        wf.wBitsPerSample = 16; wf.nBlockAlign = 4; wf.nAvgBytesPerSec = 44100 * 4;
        IDirectSoundBuffer_SetFormat(g_primary, &wf);
    }
    return 1;
}

static LONG vol_to_db(int v)
{
    double db;
    if (v <= 0) return DSBVOLUME_MIN;
    if (v >= 127) return 0;
    db = 2000.0 * log10((double)v / 127.0);   /* centesimos de dB */
    return db < DSBVOLUME_MIN ? DSBVOLUME_MIN : (LONG)db;
}

static IDirectSoundBuffer *get_buffer(SAMPLE *s)
{
    int i;
    DSBUFFERDESC d;
    IDirectSoundBuffer *b = NULL;
    void *p1, *p2; DWORD n1, n2;

    for (i = 0; i < g_ncache; i++)
        if (g_cache[i].file == s->file && g_cache[i].len == s->len && g_cache[i].owner == s->index)
            return g_cache[i].buf;

    memset(&d, 0, sizeof(d));
    d.dwSize = sizeof(d);
    d.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS | DSBCAPS_STATIC | DSBCAPS_GETCURRENTPOSITION2;
    d.dwBufferBytes = s->len;
    d.lpwfxFormat = &s->fmt;
    if (FAILED(IDirectSound_CreateSoundBuffer(g_ds, &d, &b, NULL))) return NULL;
    if (SUCCEEDED(IDirectSoundBuffer_Lock(b, 0, s->len, &p1, &n1, &p2, &n2, 0))) {
        memcpy(p1, s->data, n1);
        if (p2) memcpy(p2, s->data + n1, n2);
        IDirectSoundBuffer_Unlock(b, p1, n1, p2, n2);
    }
    if (g_ncache < MAX_CACHE) {
        g_cache[g_ncache].file = s->file;
        g_cache[g_ncache].owner = s->index;
        g_cache[g_ncache].len = s->len;
        g_cache[g_ncache].buf = b;
        g_ncache++;
    }
    return b;
}

static void stop_sample(SAMPLE *s)
{
    if (s->cur) IDirectSoundBuffer_Stop(s->cur);
    s->cur = NULL;
    s->status = SMP_DONE;
}

static void poll_sample(SAMPLE *s)
{
    DWORD st = 0;
    if (s->status != SMP_PLAYING || !s->cur) return;
    if (FAILED(IDirectSoundBuffer_GetStatus(s->cur, &st)) || !(st & DSBSTATUS_PLAYING))
        s->status = SMP_DONE;
}

/* ---- funcoes exportadas (stdcall, nomes sem decoracao via .def) ---- */

void WINAPI AIL_startup(void)
{
    memset(g_samples, 0, sizeof(g_samples));
    g_catapult_stretch_at = 0;
    reset_sample_log();
}

void WINAPI AIL_shutdown(void)
{
    int i;
    for (i = 0; i < MAX_SAMPLES; i++) if (g_samples[i].used) stop_sample(&g_samples[i]);
    for (i = 0; i < g_ncache; i++) if (g_cache[i].buf) IDirectSoundBuffer_Release(g_cache[i].buf);
    g_ncache = 0;
    if (g_primary) { IDirectSoundBuffer_Release(g_primary); g_primary = NULL; }
    if (g_ds) { IDirectSound_Release(g_ds); g_ds = NULL; }
    memset(g_samples, 0, sizeof(g_samples));
}

char *WINAPI AIL_last_error(void) { return g_error; }

void WINAPI AIL_serve(void)
{
    int i;
    for (i = 0; i < MAX_SAMPLES; i++) if (g_samples[i].used) poll_sample(&g_samples[i]);
}

int WINAPI AIL_waveOutOpen(void **drvr, void **lphWaveOut, int dev, void *fmt)
{
    (void)dev; (void)fmt;
    if (!ds_init()) { if (drvr) *drvr = NULL; return 1; }
    if (drvr) *drvr = &g_driver_dummy;
    if (lphWaveOut) *lphWaveOut = NULL;
    return 0;
}

void *WINAPI AIL_allocate_sample_handle(void *drvr)
{
    int i;
    (void)drvr;
    for (i = 0; i < MAX_SAMPLES; i++) {
        if (!g_samples[i].used) {
            SAMPLE *s = &g_samples[i];
            memset(s, 0, sizeof(*s));
            s->used = 1; s->index = i; s->volume = 127; s->status = SMP_DONE;
            return s;
        }
    }
    lstrcpyA(g_error, "Out of sample handles");
    return NULL;
}

void WINAPI AIL_init_sample(SAMPLE *s)
{
    if (!s) return;
    stop_sample(s);
    s->file = NULL; s->data = NULL; s->len = 0;
    s->volume = 127;
}

void WINAPI AIL_end_sample(SAMPLE *s)
{
    if (s) stop_sample(s);
}

unsigned WINAPI AIL_sample_status(SAMPLE *s)
{
    if (!s) return SMP_FREE;
    poll_sample(s);
    return s->status;
}

void WINAPI AIL_set_sample_volume(SAMPLE *s, int volume)
{
    if (!s) return;
    if (volume < 0) volume = 0;
    if (volume > 127) volume = 127;
    s->volume = volume;
    if (s->cur) IDirectSoundBuffer_SetVolume(s->cur, vol_to_db(volume));
}

int WINAPI AIL_set_sample_file(SAMPLE *s, const BYTE *file, int block)
{
    const BYTE *p, *end;
    DWORD riffsz;
    int havefmt = 0;
    (void)block;
    if (!s || !file) return 0;
    if (memcmp(file, "RIFF", 4) || memcmp(file + 8, "WAVE", 4)) {
        lstrcpyA(g_error, "Not a WAV file");
        return 0;
    }
    stop_sample(s);
    riffsz = *(const DWORD *)(file + 4);
    p = file + 12;
    end = file + 8 + riffsz;
    s->data = NULL; s->len = 0;
    while (p + 8 <= end) {
        DWORD csz = *(const DWORD *)(p + 4);
        if (!memcmp(p, "fmt ", 4)) {
            memset(&s->fmt, 0, sizeof(s->fmt));
            memcpy(&s->fmt, p + 8, csz < 16 ? csz : 16);
            s->fmt.cbSize = 0;
            havefmt = 1;
        } else if (!memcmp(p, "data", 4)) {
            s->data = p + 8;
            s->len = csz;
            if (s->data + s->len > end) s->len = (DWORD)(end - s->data);
        }
        p += 8 + ((csz + 1) & ~1u);
    }
    if (!havefmt || !s->data || !s->len || s->fmt.wFormatTag != WAVE_FORMAT_PCM) {
        lstrcpyA(g_error, "Unsupported WAV");
        s->data = NULL;
        return 0;
    }
    s->file = file;
    return 1;
}

void WINAPI AIL_start_sample(SAMPLE *s)
{
    IDirectSoundBuffer *b;
    if (!s || !s->data || !g_ds) return;
    stop_sample(s);
    b = get_buffer(s);
    if (!b) return;
    IDirectSoundBuffer_SetVolume(b, vol_to_db(s->volume));
    IDirectSoundBuffer_SetCurrentPosition(b, 0);
    if (IDirectSoundBuffer_Play(b, 0, 0, 0) == DSERR_BUFFERLOST) {
        IDirectSoundBuffer_Restore(b);
        IDirectSoundBuffer_Play(b, 0, 0, 0);
    }
    s->cur = b;
    s->status = SMP_PLAYING;
    send_rumble_for_sample(s);
}

void *WINAPI FILE_read(const char *filename, void *dest)
{
    HANDLE h; DWORD sz, rd;
    void *buf = dest;
    h = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(h, NULL);
    if (!buf) buf = HeapAlloc(GetProcessHeap(), 0, sz);
    if (buf && !ReadFile(h, buf, sz, &rd, NULL)) buf = NULL;
    CloseHandle(h);
    return buf;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)r;
    if (reason == DLL_PROCESS_ATTACH) PitmodBootstrap();
    return TRUE;
}
