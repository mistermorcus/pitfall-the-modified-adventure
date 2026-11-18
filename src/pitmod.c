#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <dwmapi.h>
#include <d3d9.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <xinput.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static BYTE *framebuffer;
#define FB_BITS (framebuffer + 0xF8C0)
#define GAME_PALETTE ((BYTE *)0x452C5A)
#define GAME_PACKED_MODE_FLAG ((volatile LONG *)0x453058)
#define GAME_ACTIVE_FLAG ((volatile LONG *)0x46E038)
#define GAME_JOYSTICK_ENABLED ((volatile LONG *)0x46E060)
#define GAME_JOYSTICK_AVAILABLE ((volatile LONG *)0x46D410)
#define GAME_OPTION_SOUND ((volatile LONG *)0x46D060)
#define GAME_OPTION_SMOOTH ((volatile LONG *)0x46E048)
#define GAME_OPTION_FRAMERATE ((volatile LONG *)0x46E054)
#define GAME_OPTION_JOYSTICK ((volatile LONG *)0x46E060)
#define GAME_DEBUG_ALLOW_CHEAT ((volatile LONG *)0x46E064)
#define GAME_OPTION_SLOW ((volatile LONG *)0x46ED5C)
#define GAME_OPTION_PRIORITY ((volatile LONG *)0x46EFD8)
#define GAME_OPTION_DIFFICULTY ((volatile LONG *)0x46FB78)
#define GAME_OPTION_WAV_VOLUME ((volatile LONG *)0x46E104)
#define GAME_OPTION_CD_VOLUME ((volatile LONG *)0x46F098)
#define GAME_WINDOW_TOP_X ((volatile LONG *)0x453060)
#define GAME_WINDOW_TOP_Y ((volatile LONG *)0x453064)
#define GAME_WINDOW_SIZE_X ((volatile LONG *)0x453068)
#define GAME_WINDOW_SIZE_Y ((volatile LONG *)0x45306C)
#define GAME_MUSIC_TRACK ((volatile LONG *)0x46E110)
#define GAME_AMBIENT_TRACK ((volatile LONG *)0x46E114)
#define GAME_AMBIENT_ENABLED ((volatile LONG *)0x46E118)
#define GAME_MUSIC_ENABLED ((volatile LONG *)0x46E11C)
#define GAME_PLAY_CD_TRACK ((BOOL (__cdecl *)(HWND, UINT))0x441B80)
#define GAME_ENTER_FULLSCREEN ((void (__cdecl *)(void))0x43DB40)
#define GAME_LEAVE_FULLSCREEN ((void (__cdecl *)(void))0x43DD80)
#define GAME_CAMERA_X ((volatile LONG *)0x46FBA0)
#define GAME_CAMERA_Y ((volatile LONG *)0x46FBA4)
#define GAME_TIMESET_IAT ((void **)0x474640)
#define GAME_TIMEKILL_IAT ((void **)0x474614)
#define GAME_TIMER_CALLBACK ((LPTIMECALLBACK)0x4448C0)
#define GAME_GETFOCUS_IAT ((void **)0x47455C)
#define GAME_GETASYNCKEYSTATE_IAT ((void **)0x474554)
#define GAME_AUXGETNUMDEVS_IAT ((void **)0x474618)
#define GAME_AUXGETDEVCAPS_IAT ((void **)0x47461C)
#define GAME_AUXGETVOLUME_IAT ((void **)0x474620)
#define GAME_AUXSETVOLUME_IAT ((void **)0x474624)
#define GAME_MCISEND_IAT ((void **)0x474628)
#define GAME_JOYRELEASE_IAT ((void **)0x474644)
#define GAME_JOYSETCAPTURE_IAT ((void **)0x474648)
#define GAME_JOYGETNUMDEVS_IAT ((void **)0x47464C)
#define GAME_JOYGETDEVCAPS_IAT ((void **)0x474650)
#define GAME_JOYGETPOS_IAT ((void **)0x474654)
#define GAME_REGCLOSE_IAT ((void **)0x474374)
#define GAME_REGFLUSH_IAT ((void **)0x474378)
#define GAME_REGOPEN_IAT ((void **)0x47437C)
#define GAME_REGQUERY_IAT ((void **)0x474380)
#define GAME_REGSET_IAT ((void **)0x474384)
#define GAME_REGCREATE_IAT ((void **)0x474388)
#define MUSIC_DEVICE_ID ((MCIDEVICEID)0x7F01)
#define PIT_TIMER_ID ((MMRESULT)0x50495431)
#define WM_PITMOD_TOGGLE_FULLSCREEN (WM_APP + 0x511)
#define WM_PITMOD_SELECT_MUSIC (WM_APP + 0x512)
#define WM_PITMOD_SELECT_AMBIENT (WM_APP + 0x513)
#define WM_PITMOD_VIDEO_PREVIOUS (WM_APP + 0x514)
#define WM_PITMOD_VIDEO_NEXT (WM_APP + 0x515)
#define WM_PITMOD_FILTER_PREVIOUS (WM_APP + 0x516)
#define WM_PITMOD_FILTER_NEXT (WM_APP + 0x517)
#define IDC_PITMOD_SCALING 5101
#define IDC_PITMOD_FILTER 5102
#define IDC_PITMOD_RUMBLE 5103
#define IDC_PITMOD_PAUSE_BUTTON 5110
#define IDC_PITMOD_FULLSCREEN_BUTTON 5111
#define IDC_PITMOD_MUSIC_BUTTON 5112
#define IDC_PITMOD_AMBIENT_BUTTON 5113
#define IDC_PITMOD_VIDEO_PREVIOUS_BUTTON 5114
#define IDC_PITMOD_VIDEO_NEXT_BUTTON 5115
#define IDC_PITMOD_FILTER_PREVIOUS_BUTTON 5116
#define IDC_PITMOD_FILTER_NEXT_BUTTON 5117
#define IDC_PITMOD_BONUS1_BUTTON 5118
#define IDC_PITMOD_BONUS2_BUTTON 5119
#define IDC_PITMOD_BONUS3_BUTTON 5120
#define IDC_PITMOD_CLEAR_BUTTONS 5121

static HINSTANCE dll_instance;

/* WAIL32 imports this symbol so the Windows loader brings PITMOD into the
   process before the game's entry point.  Initialization still happens from
   this DLL's process-attach path; the explicit import replaces the old
   on-disk PITFALL.EXE loader patch. */
__declspec(dllexport) void WINAPI PitmodBootstrap(void) {
}

static const HKEY pitmod_settings_key = (HKEY)(ULONG_PTR)0x5049544Du;
static LSTATUS (WINAPI *original_reg_close)(HKEY);
static LSTATUS (WINAPI *original_reg_flush)(HKEY);
static LSTATUS (WINAPI *original_reg_open)(HKEY, LPCSTR, PHKEY);
static LSTATUS (WINAPI *original_reg_query)(HKEY, LPCSTR, LPDWORD, LPDWORD,
                                            LPBYTE, LPDWORD);
static LSTATUS (WINAPI *original_reg_set)(HKEY, LPCSTR, DWORD, DWORD,
                                          const BYTE *, DWORD);
static LSTATUS (WINAPI *original_reg_create)(HKEY, LPCSTR, PHKEY);
static HWND display_window, game_window, render_window;
static HANDLE presenter_thread, control_thread;
static DWORD control_thread_id;
static volatile LONG presenting, fullscreen_active, control_running;
static volatile LONG sync_direct_present;
static volatile LONG game_timer_running;
static HMODULE game_module;
static DWORD (WINAPI *original_get_version)(void);
static DWORD (WINAPI *original_load_module)(LPCSTR, LPVOID);
static int (WINAPI *original_message_box)(HWND, LPCSTR, LPCSTR, UINT);
static HANDLE game_timer_thread;
static LPTIMECALLBACK game_timer_callback;
static DWORD_PTR game_timer_user;
static MMRESULT (WINAPI *original_time_set_event)(UINT, UINT, LPTIMECALLBACK,
                                                  DWORD_PTR, UINT);
static MMRESULT (WINAPI *original_time_kill_event)(UINT);
static LONG saved_style, saved_exstyle;
static RECT saved_rect;
static HMENU saved_menu;
static IDirect3D9Ex *d3d;
static IDirect3DDevice9Ex *d3d_device;
static IDirect3DSurface9 *d3d_frame;
#define VIDEO_FILTER_NONE 0
#define VIDEO_FILTER_SCANLINES 1
#define VIDEO_FILTER_CRT 2
#define VIDEO_FILTER_XBR 3
#define VIDEO_FILTER_FSR 4
static int video_filter;
static BOOL video_integer_scaling;
static BOOL video_stretch_fullscreen;
static int video_scaling_mode;
static const char *video_scaling_names[] = {
    "Integer4:3", "IntegerNative", "Integer16:9",
    "Fit4:3", "FitNative", "StretchToDisplay"
};
static const char *video_filter_names[] = {
    "None", "Scanlines", "CRT", "XBR", "FSR"
};
#define VIDEO_ASPECT_RAW 0
#define VIDEO_ASPECT_4_3 1
#define VIDEO_ASPECT_16_9 2
static int video_aspect = VIDEO_ASPECT_4_3;
static int video_frame_width = 320;
static int video_frame_height = 200;
static CRITICAL_SECTION d3d_lock;
static CRITICAL_SECTION music_lock;
static HWND (WINAPI *original_get_focus)(void);
static SHORT (WINAPI *original_get_async_key_state)(int);
static volatile BYTE fullscreen_key_down[256];
static HHOOK keyboard_hook;
static MCIERROR (WINAPI *original_mci_send_command)(MCIDEVICEID, UINT,
                                                    DWORD_PTR, DWORD_PTR);
static UINT (WINAPI *original_aux_get_num_devs)(void);
static MMRESULT (WINAPI *original_aux_get_dev_caps)(UINT_PTR, LPAUXCAPSA, UINT);
static MMRESULT (WINAPI *original_aux_get_volume)(UINT, LPDWORD);
static MMRESULT (WINAPI *original_aux_set_volume)(UINT, DWORD);
static volatile LONG music_generation;
static BOOL virtual_music_enabled;
static DWORD virtual_track_lengths[100];
static UINT virtual_track_count;
#define MUSIC_BACKEND_AUTO 0
#define MUSIC_BACKEND_FILES 1
#define MUSIC_BACKEND_CD 2
#define MUSIC_BACKEND_OFF 3
static int music_backend = MUSIC_BACKEND_AUTO;
static char music_root[MAX_PATH];
static UINT current_music_track;
static MCIDEVICEID actual_music_device;
static DWORD stopped_music_position;
static DWORD music_stopped_at;
static DWORD music_volume = 0xFFFFFFFF;
static HWAVEOUT music_wave;
static WAVEHDR music_wave_header;
static BYTE *music_wave_file;
static BOOL music_wave_paused;
static HMODULE xinput_module;
static DWORD (WINAPI *xinput_get_state)(DWORD, XINPUT_STATE *);
static DWORD (WINAPI *xinput_set_state)(DWORD, XINPUT_VIBRATION *);
static MMRESULT (WINAPI *original_joy_release_capture)(UINT);
static MMRESULT (WINAPI *original_joy_set_capture)(HWND, UINT, UINT, BOOL);
static UINT (WINAPI *original_joy_get_num_devs)(void);
static MMRESULT (WINAPI *original_joy_get_dev_caps)(UINT_PTR, LPJOYCAPSA, UINT);
static MMRESULT (WINAPI *original_joy_get_pos)(UINT, LPJOYINFO);
static HANDLE joystick_thread;
static volatile LONG joystick_running;
static HWND joystick_capture_window;
static UINT joystick_capture_period = 17;
static BOOL joystick_capture_changed;
static BOOL xinput_auto_enable = TRUE;
static BOOL xinput_hotplug = TRUE;
static BOOL xinput_auto_enable_applied;
static BOOL xinput_connected;
static BOOL xinput_rumble_enabled = TRUE;
static WORD xinput_rumble_action = 12000;
static WORD xinput_rumble_impact = 26000;
static WORD xinput_rumble_shared = 25000;
static WORD xinput_rumble_explosion = 36000;
static WORD xinput_rumble_damage = 42500;
static WORD xinput_rumble_movement = 8000;
static WORD xinput_rumble_rope = 4000;
static WORD xinput_rumble_vertical_slide = 2600;
static WORD xinput_rumble_zipline = 8000;
static WORD xinput_rumble_stun = 29500;
static WORD xinput_rumble_pickup = 12000;
static WORD xinput_rumble_bag = 6500;
static WORD xinput_rumble_coin = 5500;
static WORD xinput_rumble_trampoline = 12000;
static WORD xinput_rumble_extra_life = 10000;
static WORD xinput_rumble_mud = 12000;
static WORD xinput_rumble_boomerang = 10500;
static WORD xinput_rumble_rock_door = 6500;
static WORD xinput_rumble_catapult_stretch = 9000;
static WORD xinput_rumble_catapult_launch = 15700;
static WORD xinput_rumble_hover = 5900;
static WORD xinput_rumble_nunchuck = 8500;
static WORD xinput_rumble_enemy_explosion = 23000;
static WORD xinput_rumble_wall_bricks = 8000;
static WORD xinput_rumble_minecart = 9000;
static volatile DWORD xinput_rumble_until;
static volatile DWORD xinput_rumble_pattern_start;
static volatile LONG xinput_rumble_pattern;
static volatile LONG xinput_rumble_active;
static int xinput_bonus_buttons[3] = { 1, 2, 3 };
static int xinput_pause_button = 8;
static int xinput_fullscreen_button = 7;
static int xinput_music_button = 5;
static int xinput_ambient_button = 6;
static int xinput_video_previous_button = 11;
static int xinput_video_next_button = 12;
static int xinput_filter_previous_button;
static int xinput_filter_next_button;
static volatile BYTE xinput_number_down[3];
static volatile BYTE xinput_pause_down;
static volatile BYTE xinput_fullscreen_down;
static volatile BYTE xinput_music_down;
static volatile BYTE xinput_ambient_down;
static volatile BYTE xinput_video_previous_down;
static volatile BYTE xinput_video_next_down;
static volatile BYTE xinput_filter_previous_down;
static volatile BYTE xinput_filter_next_down;
static volatile LONG xinput_binding_active;
static uint32_t converted_frame[320 * 200];
static uint32_t filtered_frame[640 * 400];
static uint32_t fsr_easu_frame[640 * 400];
static uint32_t previous_changed_frame[320 * 200];
#define MAX_CHANGE_SAMPLES 8192
typedef struct {
    DWORD time_us;
    DWORD interval_us;
    DWORD changed;
    uint32_t hash;
    DWORD present_us;
    LONG present_result;
    LONG camera_x;
    LONG camera_y;
    LONG motion_shift;
    DWORD motion_matches;
} CHANGE_SAMPLE;
static CHANGE_SAMPLE change_samples[MAX_CHANGE_SAMPLES];
static unsigned long change_sample_count;
static LONGLONG capture_frequency;
static LARGE_INTEGER direct_capture_start, direct_last_sample;
static uint32_t direct_previous_hash;
static BOOL direct_sample_valid;
static volatile LONG preferred_music_enabled;
static volatile LONG preferred_ambient_enabled;
static volatile LONG preferred_music_loaded;
static volatile LONG preferred_ambient_loaded;
/* The original Sound page may consume and reset its checkboxes before the
   later PitMod page receives PSN_APPLY. Snapshot the user's live choices
   while the sheet is open, then commit that snapshot on OK/Apply. */
static HWND sound_pending_properties;
static volatile LONG sound_pending_music;
static volatile LONG sound_pending_ambient;
static volatile LONG sound_pending_valid;
static void write_log(const char *event);
static void draw_frame_once(void);
static void fit_4_3(RECT *out, int width, int height);
static void fit_video_target(RECT *out, int width, int height);

static void get_pitmod_ini_path(char *path) {
    char *slash;
    GetModuleFileNameA(NULL, path, MAX_PATH);
    slash = strrchr(path, '\\');
    if (slash) lstrcpyA(slash + 1, "PITMOD.INI");
    else lstrcpyA(path, "PITMOD.INI");
}

static BOOL contains_pitfall(const char *text) {
    static const char needle[] = "pitfall";
    size_t i, j;
    if (!text) return FALSE;
    for (i = 0; text[i]; ++i) {
        for (j = 0; needle[j]; ++j) {
            char c = text[i + j];
            if (!c) return FALSE;
            if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
            if (c != needle[j]) break;
        }
        if (!needle[j]) return TRUE;
    }
    return FALSE;
}

static LSTATUS WINAPI local_reg_open(HKEY root, LPCSTR subkey, PHKEY result) {
    if (contains_pitfall(subkey)) {
        if (result) *result = pitmod_settings_key;
        return ERROR_SUCCESS;
    }
    return original_reg_open(root, subkey, result);
}

static LSTATUS WINAPI local_reg_create(HKEY root, LPCSTR subkey, PHKEY result) {
    if (contains_pitfall(subkey)) {
        if (result) *result = pitmod_settings_key;
        return ERROR_SUCCESS;
    }
    return original_reg_create(root, subkey, result);
}

static int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

static DWORD normalize_key_binding(const char *name, DWORD number) {
    WORD virtual_key;
    if (!name || lstrlenA(name) < 4 ||
        (name[0] != 'K' && name[0] != 'k') ||
        (name[1] != 'E' && name[1] != 'e') ||
        (name[2] != 'Y' && name[2] != 'y') || name[3] != '_')
        return number;
    /* Pitfall tags ordinary keyboard bindings with 0x08 and extended keys
       with 0x09. A failed legacy joystick assignment can strip this tag,
       after which its duplicate-key validator rejects otherwise unique keys. */
    if (!number || (number & 0x0F000000)) return number;
    virtual_key = (WORD)number;
    if (virtual_key >= VK_LEFT && virtual_key <= VK_DOWN)
        return number | 0x09000000;
    return number | 0x08000000;
}

static LSTATUS WINAPI local_reg_query(HKEY key, LPCSTR name, LPDWORD reserved,
                                      LPDWORD type, LPBYTE data, LPDWORD size) {
    char ini[MAX_PATH], value[1024];
    DWORD required;
    (void)reserved;
    if (key != pitmod_settings_key)
        return original_reg_query(key, name, reserved, type, data, size);
    if (!name || !size) return ERROR_INVALID_PARAMETER;
    get_pitmod_ini_path(ini);
    GetPrivateProfileStringA("Game", name, "__PITMOD_MISSING__",
                             value, sizeof(value), ini);
    if (!lstrcmpA(value, "__PITMOD_MISSING__")) return ERROR_FILE_NOT_FOUND;
    if (!lstrcmpiA(name, "Option_Level")) {
        DWORD index;
        required = (DWORD)lstrlenA(value) / 2;
        if (type) *type = REG_BINARY;
        if (!data) { *size = required; return ERROR_SUCCESS; }
        if (*size < required) { *size = required; return ERROR_MORE_DATA; }
        for (index = 0; index < required; ++index) {
            int high = hex_digit(value[index * 2]);
            int low = hex_digit(value[index * 2 + 1]);
            if (high < 0 || low < 0) return ERROR_INVALID_DATA;
            data[index] = (BYTE)((high << 4) | low);
        }
        *size = required;
        return ERROR_SUCCESS;
    } else {
        DWORD number = normalize_key_binding(name, strtoul(value, NULL, 0));
        if (!lstrcmpiA(name, "Option_CDMusic")) {
            InterlockedExchange(&preferred_music_enabled, number != 0);
            InterlockedExchange(&preferred_music_loaded, 1);
        } else if (!lstrcmpiA(name, "Option_CDAmbient")) {
            InterlockedExchange(&preferred_ambient_enabled, number != 0);
            InterlockedExchange(&preferred_ambient_loaded, 1);
        }
        required = sizeof(number);
        if (type) *type = REG_DWORD;
        if (!data) { *size = required; return ERROR_SUCCESS; }
        if (*size < required) { *size = required; return ERROR_MORE_DATA; }
        CopyMemory(data, &number, required);
        *size = required;
        return ERROR_SUCCESS;
    }
}

static LSTATUS WINAPI local_reg_set(HKEY key, LPCSTR name, DWORD reserved,
                                    DWORD type, const BYTE *data, DWORD size) {
    char ini[MAX_PATH], value[1024];
    DWORD index;
    static const char hex[] = "0123456789ABCDEF";
    (void)reserved;
    if (key != pitmod_settings_key)
        return original_reg_set(key, name, reserved, type, data, size);
    if (!name || !data) return ERROR_INVALID_PARAMETER;
    /* These globals describe current playback as well as the Properties
       choice. Playback stops must not erase the durable user preference;
       the injected page commits those checkboxes directly on Apply/OK. */
    if (!lstrcmpiA(name, "Option_CDMusic") ||
        !lstrcmpiA(name, "Option_CDAmbient"))
        return ERROR_SUCCESS;
    get_pitmod_ini_path(ini);
    if (type == REG_DWORD && size >= sizeof(DWORD)) {
        DWORD number = normalize_key_binding(name, *(const DWORD *)data);
        wsprintfA(value, "%lu", number);
    } else if (type == REG_BINARY && size * 2 + 1 <= sizeof(value)) {
        for (index = 0; index < size; ++index) {
            value[index * 2] = hex[data[index] >> 4];
            value[index * 2 + 1] = hex[data[index] & 15];
        }
        value[size * 2] = 0;
    } else {
        return ERROR_INVALID_DATATYPE;
    }
    return WritePrivateProfileStringA("Game", name, value, ini)
        ? ERROR_SUCCESS : ERROR_WRITE_FAULT;
}

static void write_game_dword(const char *name, LONG value) {
    char ini[MAX_PATH], text[32];
    get_pitmod_ini_path(ini);
    wsprintfA(text, "%ld", value);
    WritePrivateProfileStringA("Game", name, text, ini);
}

static void save_live_game_options(void) {
    /* Pitfall conditionally omits several zero-valued options from its
       registry save routine. Persist the live values explicitly so turning
       an option off is just as durable as turning it on. */
    write_game_dword("Option_SoundEnabled", *GAME_OPTION_SOUND);
    write_game_dword("Option_Smooth1", *GAME_OPTION_SMOOTH);
    write_game_dword("Option_FrameRate", *GAME_OPTION_FRAMERATE);
    write_game_dword("Option_Slow", *GAME_OPTION_SLOW);
    write_game_dword("Option_JoyStick", *GAME_OPTION_JOYSTICK);
    write_game_dword("Debug_AllowCheat", *GAME_DEBUG_ALLOW_CHEAT);
    write_game_dword("Option_Priority", *GAME_OPTION_PRIORITY);
    write_game_dword("Option_Difficulty", *GAME_OPTION_DIFFICULTY);
    write_game_dword("Option_WavVolume", *GAME_OPTION_WAV_VOLUME);
    write_game_dword("Option_CDVolume", *GAME_OPTION_CD_VOLUME);
    /* Do not synthesize Option_CDMusic/Option_CDAmbient from these globals.
       They describe the currently playing stream and are cleared by the
       replacement audio backend when playback stops.  The Properties dialog
       writes the durable checkbox choices through local_reg_set(), which is
       the value that must survive shutdown and the next launch. */
    write_game_dword("Window_TopX", *GAME_WINDOW_TOP_X);
    write_game_dword("Window_TopY", *GAME_WINDOW_TOP_Y);
    write_game_dword("Window_SizeX", *GAME_WINDOW_SIZE_X);
    write_game_dword("Window_SizeY", *GAME_WINDOW_SIZE_Y);
}

static LSTATUS WINAPI local_reg_close(HKEY key) {
    if (key == pitmod_settings_key) {
        save_live_game_options();
        return ERROR_SUCCESS;
    }
    return original_reg_close(key);
}

static LSTATUS WINAPI local_reg_flush(HKEY key) {
    if (key == pitmod_settings_key) {
        char ini[MAX_PATH];
        get_pitmod_ini_path(ini);
        WritePrivateProfileStringA(NULL, NULL, NULL, ini);
        return ERROR_SUCCESS;
    }
    return original_reg_flush(key);
}

static BOOL install_local_settings_hook(void) {
    DWORD old_protect, ignored;
    SIZE_T bytes = (BYTE *)GAME_REGCREATE_IAT + sizeof(void *) -
                   (BYTE *)GAME_REGCLOSE_IAT;
    if (!VirtualProtect(GAME_REGCLOSE_IAT, bytes, PAGE_READWRITE,
                        &old_protect)) return FALSE;
    original_reg_close = (void *)*GAME_REGCLOSE_IAT;
    original_reg_flush = (void *)*GAME_REGFLUSH_IAT;
    original_reg_open = (void *)*GAME_REGOPEN_IAT;
    original_reg_query = (void *)*GAME_REGQUERY_IAT;
    original_reg_set = (void *)*GAME_REGSET_IAT;
    original_reg_create = (void *)*GAME_REGCREATE_IAT;
    *GAME_REGCLOSE_IAT = local_reg_close;
    *GAME_REGFLUSH_IAT = local_reg_flush;
    *GAME_REGOPEN_IAT = local_reg_open;
    *GAME_REGQUERY_IAT = local_reg_query;
    *GAME_REGSET_IAT = local_reg_set;
    *GAME_REGCREATE_IAT = local_reg_create;
    VirtualProtect(GAME_REGCLOSE_IAT, bytes, old_protect, &ignored);
    return TRUE;
}


__declspec(dllexport) void WINAPI PitmodRumbleEvent(DWORD event) {
    XINPUT_VIBRATION vibration;
    DWORD duration;
    WORD strength;
    if (!xinput_rumble_enabled || !xinput_set_state) return;
    InterlockedExchange(&xinput_rumble_pattern, 0);
    switch (event) {
    case 1: /* whip */
        strength = xinput_rumble_action;
        duration = 45;
        break;
    case 2: /* throw release */
        strength = xinput_rumble_action;
        duration = 70;
        break;
    case 3: /* thrown-object impact */
        strength = xinput_rumble_impact;
        duration = 110;
        break;
    case 4: /* grenade throw */
        /* Pitfall reuses this exact vocal sample for one damage reaction. */
        strength = xinput_rumble_shared;
        duration = 150;
        break;
    case 5: /* grenade explosion */
        strength = xinput_rumble_explosion;
        duration = 220;
        break;
    case 6: /* boomerang outbound/return */
        strength = xinput_rumble_action;
        duration = 60;
        break;
    case 7: /* player damage */
        strength = xinput_rumble_damage;
        duration = 280;
        break;
    case 8: /* jump takeoff */
        strength = xinput_rumble_movement;
        duration = 45;
        break;
    case 9: /* landing */
        strength = xinput_rumble_movement;
        duration = 70;
        break;
    case 10: /* vine/rope climbing */
        strength = xinput_rumble_rope;
        duration = 70;
        break;
    case 11: /* wall-stun impact */
        strength = xinput_rumble_stun;
        duration = 180;
        break;
    case 12: /* supply bag pickup */
        strength = xinput_rumble_bag;
        duration = 70;
        break;
    case 14: /* treasure/health pickup */
        strength = xinput_rumble_pickup;
        duration = 100;
        break;
    case 13: /* coin pickup */
        strength = xinput_rumble_coin;
        duration = 40;
        break;
    case 15: /* angled rope/zipline slide */
        strength = xinput_rumble_zipline;
        duration = 780;
        break;
    case 16: /* brief sparkle/star tick */
        strength = xinput_rumble_coin;
        duration = 30;
        break;
    case 17: /* vertical rope descent */
        strength = xinput_rumble_vertical_slide;
        duration = 215;
        break;
    case 18: /* spiderweb trampoline */
        strength = xinput_rumble_trampoline;
        duration = 140;
        break;
    case 19: /* extra life */
        strength = xinput_rumble_extra_life;
        duration = 240;
        break;
    case 20: /* bubbling mud death */
        strength = xinput_rumble_mud;
        duration = 1240;
        xinput_rumble_pattern_start = GetTickCount();
        InterlockedExchange(&xinput_rumble_pattern, 20);
        break;
    case 21: /* boomerang outward and return swipes */
        strength = xinput_rumble_boomerang;
        duration = 1050;
        xinput_rumble_pattern_start = GetTickCount();
        InterlockedExchange(&xinput_rumble_pattern, 21);
        break;
    case 22: /* grinding rock door */
        strength = xinput_rumble_rock_door;
        duration = 4300;
        break;
    case 23: /* slime catapult stretching */
        strength = xinput_rumble_catapult_stretch;
        duration = 640;
        xinput_rumble_pattern_start = GetTickCount();
        InterlockedExchange(&xinput_rumble_pattern, 23);
        break;
    case 24: /* slime catapult release */
        strength = xinput_rumble_catapult_launch;
        duration = 280;
        xinput_rumble_pattern_start = GetTickCount();
        InterlockedExchange(&xinput_rumble_pattern, 24);
        break;
    case 25: /* hovering / meditation power-up */
        strength = xinput_rumble_hover;
        duration = 9930;
        xinput_rumble_pattern_start = GetTickCount();
        InterlockedExchange(&xinput_rumble_pattern, 25);
        break;
    case 26: /* nunchuck idle flourish */
        strength = xinput_rumble_nunchuck;
        duration = 1240;
        xinput_rumble_pattern_start = GetTickCount();
        InterlockedExchange(&xinput_rumble_pattern, 26);
        break;
    case 27: /* enemy destruction / explosion */
        strength = xinput_rumble_enemy_explosion;
        duration = 260;
        break;
    case 28: /* bricks sliding out of a wall */
        strength = xinput_rumble_wall_bricks;
        duration = 300;
        break;
    case 29: /* pushing a minecart */
        strength = xinput_rumble_minecart;
        duration = 870;
        break;
    default:
        return;
    }
    ZeroMemory(&vibration, sizeof(vibration));
    if (event == 23) {
        vibration.wLeftMotorSpeed = 0;
        vibration.wRightMotorSpeed = 0;
    } else if (event == 24) {
        vibration.wLeftMotorSpeed = strength;
        vibration.wRightMotorSpeed = (WORD)(strength / 2);
    } else if (event == 21) {
        vibration.wLeftMotorSpeed = strength;
        vibration.wRightMotorSpeed = 0;
    } else if (event == 22 || event == 29) {
        vibration.wLeftMotorSpeed = strength;
        vibration.wRightMotorSpeed = (WORD)(strength / 4);
    } else if (event == 28) {
        /* A short, sharper scrape rather than another low mechanical drone. */
        vibration.wLeftMotorSpeed = (WORD)(strength / 3);
        vibration.wRightMotorSpeed = strength;
    } else if (event == 25 || event == 26) {
        /* Their envelopes are applied by update_xinput_rumble(). */
        vibration.wLeftMotorSpeed = 0;
        vibration.wRightMotorSpeed = 0;
    } else if (event == 3 || event == 5 || event == 7 || event == 27) {
        vibration.wLeftMotorSpeed = strength;
        vibration.wRightMotorSpeed = (WORD)(strength * 2 / 3);
    } else {
        vibration.wLeftMotorSpeed = (WORD)(strength / 2);
        vibration.wRightMotorSpeed = strength;
    }
    xinput_set_state(0, &vibration);
    xinput_rumble_until = GetTickCount() + duration;
    InterlockedExchange(&xinput_rumble_active, 1);
}

static void update_xinput_rumble(void) {
    DWORD now = GetTickCount();
    if (InterlockedCompareExchange(&xinput_rumble_pattern, 0, 0) == 25 &&
        (LONG)(now - xinput_rumble_until) < 0) {
        DWORD elapsed = now - xinput_rumble_pattern_start;
        DWORD envelope;
        WORD strength;
        XINPUT_VIBRATION vibration;
        /* Slow rise, broad floating center, and slow fall through the full
           animation, one second beyond the 8.93-second source sample. */
        if (elapsed < 2200)
            envelope = elapsed * 1000 / 2200;
        else if (elapsed < 7200)
            envelope = 1000;
        else
            envelope = (9930 - elapsed) * 1000 / 2730;
        strength = (WORD)(xinput_rumble_hover * envelope / 1000);
        ZeroMemory(&vibration, sizeof(vibration));
        vibration.wLeftMotorSpeed = strength;
        vibration.wRightMotorSpeed = (WORD)(strength / 5);
        if (xinput_set_state) xinput_set_state(0, &vibration);
        return;
    }
    if (InterlockedCompareExchange(&xinput_rumble_pattern, 0, 0) == 26 &&
        (LONG)(now - xinput_rumble_until) < 0) {
        DWORD elapsed = now - xinput_rumble_pattern_start;
        WORD left = 0, right = 0;
        XINPUT_VIBRATION vibration;
        /* Alternating swishes trace the three-part nunchuck flourish. */
        if (elapsed < 260) right =
            (WORD)(xinput_rumble_nunchuck * (260 - elapsed) / 260);
        else if (elapsed >= 340 && elapsed < 650) left =
            (WORD)(xinput_rumble_nunchuck * (650 - elapsed) / 310);
        else if (elapsed >= 760 && elapsed < 1130) right =
            (WORD)(xinput_rumble_nunchuck * (1130 - elapsed) / 370);
        ZeroMemory(&vibration, sizeof(vibration));
        vibration.wLeftMotorSpeed = left;
        vibration.wRightMotorSpeed = right;
        if (xinput_set_state) xinput_set_state(0, &vibration);
        return;
    }
    if (InterlockedCompareExchange(&xinput_rumble_pattern, 0, 0) == 23 &&
        (LONG)(now - xinput_rumble_until) < 0) {
        DWORD elapsed = now - xinput_rumble_pattern_start;
        DWORD ramp = elapsed > 600 ? 600 : elapsed;
        WORD strength = (WORD)(xinput_rumble_catapult_stretch * ramp / 600);
        XINPUT_VIBRATION vibration;
        ZeroMemory(&vibration, sizeof(vibration));
        vibration.wLeftMotorSpeed = strength;
        vibration.wRightMotorSpeed = (WORD)(strength * 2 / 3);
        if (xinput_set_state) xinput_set_state(0, &vibration);
        return;
    }
    if (InterlockedCompareExchange(&xinput_rumble_pattern, 0, 0) == 24 &&
        (LONG)(now - xinput_rumble_until) < 0) {
        DWORD elapsed = now - xinput_rumble_pattern_start;
        DWORD remaining = elapsed > 260 ? 0 : 260 - elapsed;
        XINPUT_VIBRATION vibration;
        ZeroMemory(&vibration, sizeof(vibration));
        vibration.wLeftMotorSpeed =
            (WORD)(xinput_rumble_catapult_launch * remaining / 260);
        vibration.wRightMotorSpeed =
            (WORD)(xinput_rumble_catapult_launch * remaining / 520);
        if (xinput_set_state) xinput_set_state(0, &vibration);
        return;
    }
    if (InterlockedCompareExchange(&xinput_rumble_pattern, 0, 0) == 21 &&
        (LONG)(now - xinput_rumble_until) < 0) {
        DWORD elapsed = now - xinput_rumble_pattern_start;
        WORD left = 0, right = 0;
        XINPUT_VIBRATION vibration;
        if (elapsed < 280) {
            left = (WORD)(xinput_rumble_boomerang * (280 - elapsed) / 280);
            right = (WORD)(xinput_rumble_boomerang * elapsed / 280);
        } else if (elapsed >= 570 && elapsed < 900) {
            DWORD returning = elapsed - 570;
            left = (WORD)(xinput_rumble_boomerang * returning / 330);
            right = (WORD)(xinput_rumble_boomerang * (330 - returning) / 330);
        }
        ZeroMemory(&vibration, sizeof(vibration));
        vibration.wLeftMotorSpeed = left;
        vibration.wRightMotorSpeed = right;
        if (xinput_set_state) xinput_set_state(0, &vibration);
        return;
    }
    if (InterlockedCompareExchange(&xinput_rumble_pattern, 0, 0) == 20 &&
        (LONG)(now - xinput_rumble_until) < 0) {
        DWORD elapsed = now - xinput_rumble_pattern_start;
        WORD left = 0, right = 0;
        XINPUT_VIBRATION vibration;
        if (elapsed < 100) {
            left = (WORD)(xinput_rumble_mud / 4); right = xinput_rumble_mud;
        } else if (elapsed >= 165 && elapsed < 285) {
            left = (WORD)(xinput_rumble_mud * 2 / 3); right = xinput_rumble_mud;
        } else if (elapsed >= 360 && elapsed < 500) {
            left = xinput_rumble_mud; right = (WORD)(xinput_rumble_mud / 2);
        } else if (elapsed >= 585 && elapsed < 750) {
            left = (WORD)(xinput_rumble_mud * 3 / 4); right = xinput_rumble_mud;
        } else if (elapsed >= 845 && elapsed < 1060) {
            left = xinput_rumble_mud; right = (WORD)(xinput_rumble_mud * 2 / 3);
        }
        ZeroMemory(&vibration, sizeof(vibration));
        vibration.wLeftMotorSpeed = left;
        vibration.wRightMotorSpeed = right;
        if (xinput_set_state) xinput_set_state(0, &vibration);
        return;
    }
    if (InterlockedCompareExchange(&xinput_rumble_active, 0, 0) &&
        (LONG)(now - xinput_rumble_until) >= 0) {
        XINPUT_VIBRATION vibration;
        ZeroMemory(&vibration, sizeof(vibration));
        if (xinput_set_state) xinput_set_state(0, &vibration);
        InterlockedExchange(&xinput_rumble_pattern, 0);
        InterlockedExchange(&xinput_rumble_active, 0);
    }
}

static HWND WINAPI compatible_get_focus(void) {
    if (InterlockedCompareExchange(&fullscreen_active, 0, 0) && game_window)
        return game_window;
    return original_get_focus ? original_get_focus() : NULL;
}

static SHORT WINAPI compatible_get_async_key_state(int virtual_key) {
    SHORT state = original_get_async_key_state
        ? original_get_async_key_state(virtual_key) : 0;
    if (InterlockedCompareExchange(&fullscreen_active, 0, 0) &&
        virtual_key >= 0 && virtual_key < 256) {
        if (fullscreen_key_down[virtual_key])
            state |= (SHORT)0x8000;
    }
    if (virtual_key >= '1' && virtual_key <= '3' &&
        xinput_number_down[virtual_key - '1'])
        state |= (SHORT)0x8000;
    if (virtual_key == VK_PAUSE && xinput_pause_down)
        state |= (SHORT)0x8000;
    return state;
}

static DWORD xinput_buttons(const XINPUT_GAMEPAD *pad) {
    DWORD buttons = 0;
    if (pad->wButtons & XINPUT_GAMEPAD_A) buttons |= JOY_BUTTON1;
    if (pad->wButtons & XINPUT_GAMEPAD_B) buttons |= JOY_BUTTON2;
    if (pad->wButtons & XINPUT_GAMEPAD_X) buttons |= JOY_BUTTON3;
    if (pad->wButtons & XINPUT_GAMEPAD_Y) buttons |= JOY_BUTTON4;
    if (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) buttons |= JOY_BUTTON5;
    if (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) buttons |= JOY_BUTTON6;
    if (pad->wButtons & XINPUT_GAMEPAD_BACK) buttons |= JOY_BUTTON7;
    if (pad->wButtons & XINPUT_GAMEPAD_START) buttons |= JOY_BUTTON8;
    if (pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB) buttons |= JOY_BUTTON9;
    if (pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) buttons |= JOY_BUTTON10;
    if (pad->bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
        buttons |= JOY_BUTTON11;
    if (pad->bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
        buttons |= JOY_BUTTON12;
    return buttons;
}

static void xinput_position(const XINPUT_GAMEPAD *pad, UINT *x, UINT *y) {
    LONG sx = pad->sThumbLX;
    LONG sy = pad->sThumbLY;
    if (sx > -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE &&
        sx < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) sx = 0;
    if (sy > -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE &&
        sy < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) sy = 0;
    *x = (DWORD)(sx + 32768);
    *y = (DWORD)(32767 - sy);
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) *x = 0;
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) *x = 65535;
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) *y = 0;
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) *y = 65535;
}

static void update_xinput_shortcuts(DWORD buttons) {
    HWND target = game_window ? game_window : joystick_capture_window;
    HWND properties = FindWindowA(NULL, "Pitfall Properties");
    int i;
    if (InterlockedCompareExchange(&xinput_binding_active, 0, 0) ||
        (properties && IsWindowVisible(properties))) {
        ZeroMemory((void *)xinput_number_down, sizeof(xinput_number_down));
        xinput_pause_down = xinput_fullscreen_down = 0;
        xinput_music_down = xinput_ambient_down = 0;
        xinput_video_previous_down = xinput_video_next_down = 0;
        xinput_filter_previous_down = xinput_filter_next_down = 0;
        return;
    }
    for (i = 0; i < 3; ++i) {
        int button = xinput_bonus_buttons[i];
        BOOL down = button >= 1 && button <= 32 &&
            (buttons & (1u << (button - 1))) != 0;
        if (down != (xinput_number_down[i] != 0)) {
            xinput_number_down[i] = (BYTE)down;
            if (target)
                PostMessageA(target, down ? WM_KEYDOWN : WM_KEYUP,
                             '1' + i, 0);
        }
    }
    {
        int button = xinput_pause_button;
        BOOL down = button >= 1 && button <= 32 &&
            (buttons & (1u << (button - 1))) != 0;
        if (down != (xinput_pause_down != 0)) {
            xinput_pause_down = (BYTE)down;
            if (target)
                PostMessageA(target, down ? WM_KEYDOWN : WM_KEYUP,
                             VK_PAUSE, 0);
        }
    }
    {
        int button = xinput_fullscreen_button;
        BOOL down = button >= 1 && button <= 32 &&
            (buttons & (1u << (button - 1))) != 0;
        if (down && !xinput_fullscreen_down && control_thread_id)
            PostThreadMessageA(control_thread_id,
                               WM_PITMOD_TOGGLE_FULLSCREEN, 0, 0);
        xinput_fullscreen_down = (BYTE)down;
    }
    {
        int button = xinput_music_button;
        BOOL down = button >= 1 && button <= 32 &&
            (buttons & (1u << (button - 1))) != 0;
        if (down && !xinput_music_down && control_thread_id)
            PostThreadMessageA(control_thread_id,
                               WM_PITMOD_SELECT_MUSIC, 0, 0);
        xinput_music_down = (BYTE)down;
    }
    {
        int button = xinput_ambient_button;
        BOOL down = button >= 1 && button <= 32 &&
            (buttons & (1u << (button - 1))) != 0;
        if (down && !xinput_ambient_down && control_thread_id)
            PostThreadMessageA(control_thread_id,
                               WM_PITMOD_SELECT_AMBIENT, 0, 0);
        xinput_ambient_down = (BYTE)down;
    }
    {
        int button = xinput_video_previous_button;
        BOOL down = button >= 1 && button <= 32 &&
            (buttons & (1u << (button - 1))) != 0;
        if (down && !xinput_video_previous_down && control_thread_id)
            PostThreadMessageA(control_thread_id,
                               WM_PITMOD_VIDEO_PREVIOUS, 0, 0);
        xinput_video_previous_down = (BYTE)down;
    }
    {
        int button = xinput_video_next_button;
        BOOL down = button >= 1 && button <= 32 &&
            (buttons & (1u << (button - 1))) != 0;
        if (down && !xinput_video_next_down && control_thread_id)
            PostThreadMessageA(control_thread_id,
                               WM_PITMOD_VIDEO_NEXT, 0, 0);
        xinput_video_next_down = (BYTE)down;
    }
    {
        int button = xinput_filter_previous_button;
        BOOL down = button >= 1 && button <= 32 &&
            (buttons & (1u << (button - 1))) != 0;
        if (down && !xinput_filter_previous_down && control_thread_id)
            PostThreadMessageA(control_thread_id,
                               WM_PITMOD_FILTER_PREVIOUS, 0, 0);
        xinput_filter_previous_down = (BYTE)down;
    }
    {
        int button = xinput_filter_next_button;
        BOOL down = button >= 1 && button <= 32 &&
            (buttons & (1u << (button - 1))) != 0;
        if (down && !xinput_filter_next_down && control_thread_id)
            PostThreadMessageA(control_thread_id,
                               WM_PITMOD_FILTER_NEXT, 0, 0);
        xinput_filter_next_down = (BYTE)down;
    }
}

static MMRESULT WINAPI compatible_joy_get_pos(UINT id, LPJOYINFO info) {
    XINPUT_STATE state;
    if (!xinput_get_state || id != 0)
        return original_joy_get_pos(id, info);
    ZeroMemory(&state, sizeof(state));
    if (xinput_get_state(0, &state) != ERROR_SUCCESS)
        return JOYERR_UNPLUGGED;
    if (!info) return MMSYSERR_INVALPARAM;
    xinput_position(&state.Gamepad, &info->wXpos, &info->wYpos);
    info->wZpos = 32767;
    info->wButtons = xinput_buttons(&state.Gamepad);
    return JOYERR_NOERROR;
}

static UINT WINAPI compatible_joy_get_num_devs(void) {
    if (xinput_get_state) {
        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(state));
        if (!xinput_auto_enable_applied && xinput_auto_enable &&
            xinput_get_state(0, &state) == ERROR_SUCCESS) {
            *GAME_JOYSTICK_ENABLED = 1;
            xinput_auto_enable_applied = TRUE;
        }
        return 1;
    }
    return original_joy_get_num_devs();
}

static MMRESULT WINAPI compatible_joy_get_dev_caps(UINT_PTR id,
                                                    LPJOYCAPSA caps,
                                                    UINT size) {
    JOYCAPSA value;
    if (!xinput_get_state || id != 0)
        return original_joy_get_dev_caps(id, caps, size);
    if (!caps) return MMSYSERR_INVALPARAM;
    ZeroMemory(&value, sizeof(value));
    value.wMid = MM_MICROSOFT;
    value.wPid = 1;
    lstrcpyA(value.szPname, "XInput Controller");
    value.wXmin = value.wYmin = value.wZmin = 0;
    value.wXmax = value.wYmax = value.wZmax = 65535;
    value.wNumButtons = 12;
    value.wPeriodMin = 1;
    value.wPeriodMax = 1000;
    value.wNumAxes = 2;
    value.wMaxAxes = 2;
    value.wMaxButtons = 12;
    CopyMemory(caps, &value, size < sizeof(value) ? size : sizeof(value));
    return JOYERR_NOERROR;
}

static DWORD WINAPI send_joystick_messages(void *unused) {
    JOYINFO current, previous;
    BOOL have_previous = FALSE;
    (void)unused;
    ZeroMemory(&previous, sizeof(previous));
    while (InterlockedCompareExchange(&joystick_running, 1, 1)) {
        if (joystick_capture_window &&
            compatible_joy_get_pos(0, &current) == JOYERR_NOERROR) {
            BOOL moved = !have_previous || current.wXpos != previous.wXpos ||
                         current.wYpos != previous.wYpos;
            DWORD changed = have_previous
                ? current.wButtons ^ previous.wButtons : current.wButtons;
            if (!joystick_capture_changed || moved)
                PostMessageA(joystick_capture_window, MM_JOY1MOVE,
                             current.wButtons,
                             MAKELPARAM(current.wXpos, current.wYpos));
            if (changed) {
                DWORD down = changed & current.wButtons;
                DWORD up = changed & previous.wButtons;
                if (down) {
                    /* The Win95 dialog stores the changed-button byte as the
                       assignment. If two XInput buttons arrive in one poll,
                       a combined mask (for example 3) has no Button N label
                       and can trip its duplicate validator. Capture one
                       deterministic button per notification. */
                    down &= (DWORD)(0u - down);
                    PostMessageA(joystick_capture_window, MM_JOY1BUTTONDOWN,
                                 current.wButtons | (down << 8),
                                 MAKELPARAM(current.wXpos, current.wYpos));
                }
                if (up) {
                    up &= (DWORD)(0u - up);
                    PostMessageA(joystick_capture_window, MM_JOY1BUTTONUP,
                                 current.wButtons | (up << 8),
                                 MAKELPARAM(current.wXpos, current.wYpos));
                }
            }
            previous = current;
            have_previous = TRUE;
        } else {
            have_previous = FALSE;
        }
        Sleep(joystick_capture_period ? joystick_capture_period : 17);
    }
    return 0;
}

static MMRESULT WINAPI compatible_joy_set_capture(HWND window, UINT id,
                                                   UINT period, BOOL changed) {
    if (!xinput_get_state || id != 0)
        return original_joy_set_capture(window, id, period, changed);
    joystick_capture_window = window;
    joystick_capture_period = period ? period : 17;
    joystick_capture_changed = changed;
    if (InterlockedCompareExchange(&joystick_running, 1, 0) == 0) {
        joystick_thread = CreateThread(NULL, 0, send_joystick_messages,
                                       NULL, 0, NULL);
        if (!joystick_thread) {
            InterlockedExchange(&joystick_running, 0);
            return MMSYSERR_NOMEM;
        }
    }
    return JOYERR_NOERROR;
}

static MMRESULT WINAPI compatible_joy_release_capture(UINT id) {
    if (!xinput_get_state || id != 0)
        return original_joy_release_capture(id);
    joystick_capture_window = NULL;
    InterlockedExchange(&joystick_running, 0);
    if (joystick_thread) {
        WaitForSingleObject(joystick_thread, 1000);
        CloseHandle(joystick_thread);
        joystick_thread = NULL;
    }
    return JOYERR_NOERROR;
}

static BOOL install_xinput_hook(void) {
    static const char *names[] = {
        "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"
    };
    DWORD old_protect, ignored;
    SIZE_T bytes = (BYTE *)GAME_JOYGETPOS_IAT -
                   (BYTE *)GAME_JOYRELEASE_IAT + sizeof(void *);
    unsigned i;
    char ini[MAX_PATH], *slash;
    if (GetModuleFileNameA(NULL, ini, sizeof(ini))) {
        slash = ini + lstrlenA(ini);
        while (slash > ini && slash[-1] != '\\' && slash[-1] != '/') --slash;
        lstrcpyA(slash, "PITMOD.INI");
        xinput_auto_enable = GetPrivateProfileIntA(
            "XInput", "AutoEnableJoystick", 1, ini) != 0;
        xinput_hotplug = GetPrivateProfileIntA(
            "XInput", "HotPlug", 1, ini) != 0;
        xinput_bonus_buttons[0] = GetPrivateProfileIntA(
            "XInput", "BonusKey1Button", 1, ini);
        xinput_bonus_buttons[1] = GetPrivateProfileIntA(
            "XInput", "BonusKey2Button", 2, ini);
        xinput_bonus_buttons[2] = GetPrivateProfileIntA(
            "XInput", "BonusKey3Button", 3, ini);
        xinput_pause_button = GetPrivateProfileIntA(
            "XInput", "PauseButton", 8, ini);
        xinput_fullscreen_button = GetPrivateProfileIntA(
            "XInput", "FullscreenButton", 7, ini);
        xinput_music_button = GetPrivateProfileIntA(
            "XInput", "MusicButton", 5, ini);
        xinput_ambient_button = GetPrivateProfileIntA(
            "XInput", "AmbientButton", 6, ini);
        xinput_video_previous_button = GetPrivateProfileIntA(
            "XInput", "VideoPreviousButton", 11, ini);
        xinput_video_next_button = GetPrivateProfileIntA(
            "XInput", "VideoNextButton", 12, ini);
        xinput_filter_previous_button = GetPrivateProfileIntA(
            "XInput", "FilterPreviousButton", 0, ini);
        xinput_filter_next_button = GetPrivateProfileIntA(
            "XInput", "FilterNextButton", 0, ini);
        xinput_rumble_enabled = GetPrivateProfileIntA(
            "XInput", "Rumble", 1, ini) != 0;
        {
            int value = GetPrivateProfileIntA(
                "XInput", "RumbleActionPercent", 18, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_action = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleImpactPercent", 40, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_impact = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleSharedGruntPercent", 38, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_shared = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleExplosionPercent", 55, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_explosion = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleDamagePercent", 65, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_damage = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleMovementPercent", 12, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_movement = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleRopePercent", 5, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_rope = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleVerticalSlidePercent", 4, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_vertical_slide = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleZiplinePercent", 16, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_zipline = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleStunPercent", 45, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_stun = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumblePickupPercent", 18, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_pickup = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleBagPercent", 10, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_bag = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleCoinPercent", 8, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_coin = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleTrampolinePercent", 18, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_trampoline = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleExtraLifePercent", 15, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_extra_life = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleMudPercent", 18, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_mud = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleBoomerangPercent", 16, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_boomerang = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleRockDoorPercent", 10, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_rock_door = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleCatapultStretchPercent", 14, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_catapult_stretch = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleCatapultLaunchPercent", 24, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_catapult_launch = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleHoverPercent", 9, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_hover = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleNunchuckPercent", 13, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_nunchuck = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleEnemyExplosionPercent", 35, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_enemy_explosion = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleWallBricksPercent", 8, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_wall_bricks = (WORD)(value * 65535 / 100);
            value = GetPrivateProfileIntA(
                "XInput", "RumbleMinecartPercent", 14, ini);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            xinput_rumble_minecart = (WORD)(value * 65535 / 100);
        }
    }
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        xinput_module = LoadLibraryA(names[i]);
        if (xinput_module) break;
    }
    if (xinput_module)
        xinput_get_state = (DWORD (WINAPI *)(DWORD, XINPUT_STATE *))
            GetProcAddress(xinput_module, "XInputGetState");
    if (xinput_module)
        xinput_set_state = (DWORD (WINAPI *)(DWORD, XINPUT_VIBRATION *))
            GetProcAddress(xinput_module, "XInputSetState");
    if (!xinput_get_state) return TRUE;
    if (!VirtualProtect(GAME_JOYRELEASE_IAT, bytes, PAGE_READWRITE,
                        &old_protect)) return FALSE;
    original_joy_release_capture =
        (MMRESULT (WINAPI *)(UINT))*GAME_JOYRELEASE_IAT;
    original_joy_set_capture =
        (MMRESULT (WINAPI *)(HWND, UINT, UINT, BOOL))*GAME_JOYSETCAPTURE_IAT;
    original_joy_get_num_devs =
        (UINT (WINAPI *)(void))*GAME_JOYGETNUMDEVS_IAT;
    original_joy_get_dev_caps =
        (MMRESULT (WINAPI *)(UINT_PTR, LPJOYCAPSA, UINT))
        *GAME_JOYGETDEVCAPS_IAT;
    original_joy_get_pos =
        (MMRESULT (WINAPI *)(UINT, LPJOYINFO))*GAME_JOYGETPOS_IAT;
    *GAME_JOYRELEASE_IAT = compatible_joy_release_capture;
    *GAME_JOYSETCAPTURE_IAT = compatible_joy_set_capture;
    *GAME_JOYGETNUMDEVS_IAT = compatible_joy_get_num_devs;
    *GAME_JOYGETDEVCAPS_IAT = compatible_joy_get_dev_caps;
    *GAME_JOYGETPOS_IAT = compatible_joy_get_pos;
    VirtualProtect(GAME_JOYRELEASE_IAT, bytes, old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), GAME_JOYRELEASE_IAT, bytes);
    return TRUE;
}

static LRESULT CALLBACK low_level_keyboard(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION &&
        InterlockedCompareExchange(&fullscreen_active, 0, 0)) {
        KBDLLHOOKSTRUCT *key = (KBDLLHOOKSTRUCT *)lp;
        if (key && key->vkCode < 256) {
            if (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN)
                fullscreen_key_down[key->vkCode] = 1;
            else if (wp == WM_KEYUP || wp == WM_SYSKEYUP)
                fullscreen_key_down[key->vkCode] = 0;
        }
    }
    return CallNextHookEx(keyboard_hook, code, wp, lp);
}

static BOOL find_music_track(UINT track, char *path) {
    static const char *extensions[] = { "wav", "mp3", "wma" };
    unsigned int index;
    for (index = 0; index < sizeof(extensions) / sizeof(extensions[0]); ++index) {
        wsprintfA(path, "%s\\track%02u.%s", music_root, track,
                  extensions[index]);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return TRUE;
    }
    return FALSE;
}

static DWORD milliseconds_to_msf(DWORD milliseconds) {
    DWORD frames = (DWORD)(((unsigned long long)milliseconds * 75ULL + 500ULL) /
                           1000ULL);
    return MCI_MAKE_MSF(frames / (60 * 75), (frames / 75) % 60, frames % 75);
}

static DWORD wav_track_length(const char *path) {
    BYTE header[12], chunk[8], format[16];
    DWORD read, data_size = 0, bytes_per_second = 0;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!ReadFile(file, header, sizeof(header), &read, NULL) ||
        read != sizeof(header) || memcmp(header, "RIFF", 4) ||
        memcmp(header + 8, "WAVE", 4)) {
        CloseHandle(file);
        return 0;
    }
    while (ReadFile(file, chunk, sizeof(chunk), &read, NULL) &&
           read == sizeof(chunk)) {
        DWORD size;
        CopyMemory(&size, chunk + 4, sizeof(size));
        if (!memcmp(chunk, "fmt ", 4) && size >= sizeof(format)) {
            if (!ReadFile(file, format, sizeof(format), &read, NULL) ||
                read != sizeof(format)) break;
            CopyMemory(&bytes_per_second, format + 8,
                       sizeof(bytes_per_second));
            if (size > sizeof(format))
                SetFilePointer(file, size - sizeof(format), NULL, FILE_CURRENT);
        } else if (!memcmp(chunk, "data", 4)) {
            data_size = size;
            break;
        } else {
            SetFilePointer(file, size, NULL, FILE_CURRENT);
        }
        if (size & 1) SetFilePointer(file, 1, NULL, FILE_CURRENT);
    }
    CloseHandle(file);
    if (!data_size || !bytes_per_second) return 0;
    return milliseconds_to_msf((DWORD)
        (((unsigned long long)data_size * 1000ULL) / bytes_per_second));
}

static DWORD compressed_track_length(const char *path) {
    char command[MAX_PATH + 80], result[32];
    DWORD milliseconds;
    mciSendStringA("close pitlength wait", NULL, 0, NULL);
    wsprintfA(command, "open \"%s\" type mpegvideo alias pitlength wait", path);
    if (mciSendStringA(command, NULL, 0, NULL)) return 0;
    if (mciSendStringA("set pitlength time format milliseconds wait",
                       NULL, 0, NULL) ||
        mciSendStringA("status pitlength length wait", result,
                       sizeof(result), NULL)) {
        mciSendStringA("close pitlength wait", NULL, 0, NULL);
        return 0;
    }
    milliseconds = strtoul(result, NULL, 10);
    mciSendStringA("close pitlength wait", NULL, 0, NULL);
    return milliseconds_to_msf(milliseconds);
}

static DWORD music_track_length(const char *path) {
    const char *extension = strrchr(path, '.');
    if (extension && !lstrcmpiA(extension, ".wav"))
        return wav_track_length(path);
    return compressed_track_length(path);
}

typedef struct {
    LONG generation;
    HWND callback;
    DWORD duration_ms;
    DWORD loop_duration_ms;
    UINT track;
} MUSIC_WAIT;

static DWORD WINAPI wait_for_music_end(void *parameter) {
    MUSIC_WAIT *wait = (MUSIC_WAIT *)parameter;
    ULONGLONG deadline = GetTickCount64() + wait->duration_ms;
    HWND callback;
    while (wait->generation ==
           InterlockedCompareExchange(&music_generation, 0, 0)) {
        ULONGLONG now = GetTickCount64();
        MCIERROR seek_error, play_error;
        char event[160];
        if (now < deadline) {
            Sleep((DWORD)((deadline - now) > 100 ? 100 : (deadline - now)));
            continue;
        }
        /* Do not ask the 1995 CD-audio state machine to close and reopen a
           completed MPEG decoder. Rewind the still-selected virtual track in
           place; user toggles and level changes invalidate this generation. */
        if (wait->track != current_music_track ||
            !((*GAME_MUSIC_ENABLED && wait->track == (UINT)*GAME_MUSIC_TRACK) ||
              (*GAME_AMBIENT_ENABLED && wait->track == (UINT)*GAME_AMBIENT_TRACK)))
            break;
        seek_error = mciSendStringA("seek pitmusic to start wait", NULL, 0, NULL);
        play_error = seek_error ? seek_error :
            mciSendStringA("play pitmusic from 0", NULL, 0, NULL);
        wsprintfA(event, "music internal loop track=%u seek=%lu play=%lu",
                  wait->track, (DWORD)seek_error, (DWORD)play_error);
        write_log(event);
        if (play_error) break;
        deadline = GetTickCount64() + wait->loop_duration_ms;
    }
    callback = wait->callback;
    if (!callback || !IsWindow(callback))
        callback = FindWindowA(NULL, "Pitfall: The Mayan Adventure");
    if (wait->generation == InterlockedCompareExchange(&music_generation, 0, 0) &&
        callback)
        PostMessageA(callback, MM_MCINOTIFY, MCI_NOTIFY_SUCCESSFUL,
                     (LPARAM)MUSIC_DEVICE_ID);
    HeapFree(GetProcessHeap(), 0, wait);
    return 0;
}

static void log_music_command(const char *name, MCIDEVICEID device,
                              DWORD_PTR flags, DWORD from) {
    char event[160];
    wsprintfA(event, "music %s device=%lu flags=%08lX from=%08lX",
              name, (DWORD)device, (DWORD)flags, from);
    write_log(event);
}

static void apply_music_volume(void) {
    if (music_wave) waveOutSetVolume(music_wave, music_volume);
}

static void close_wave_music(void) {
    if (music_wave) {
        waveOutReset(music_wave);
        if (music_wave_header.dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(music_wave, &music_wave_header,
                                   sizeof(music_wave_header));
        waveOutClose(music_wave);
        music_wave = NULL;
    }
    ZeroMemory(&music_wave_header, sizeof(music_wave_header));
    if (music_wave_file) {
        HeapFree(GetProcessHeap(), 0, music_wave_file);
        music_wave_file = NULL;
    }
    music_wave_paused = FALSE;
}

static BOOL play_wave_music(const char *path, DWORD offset_ms) {
    HANDLE file;
    DWORD size, read, pos;
    BYTE *data = NULL, *pcm = NULL;
    DWORD pcm_size = 0;
    WAVEFORMATEX format;
    MMRESULT result;
    close_wave_music();
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    size = GetFileSize(file, NULL);
    data = HeapAlloc(GetProcessHeap(), 0, size);
    if (!data || !ReadFile(file, data, size, &read, NULL) || read != size) {
        if (data) HeapFree(GetProcessHeap(), 0, data);
        CloseHandle(file);
        return FALSE;
    }
    CloseHandle(file);
    if (size < 12 || memcmp(data, "RIFF", 4) || memcmp(data + 8, "WAVE", 4)) {
        HeapFree(GetProcessHeap(), 0, data);
        return FALSE;
    }
    ZeroMemory(&format, sizeof(format));
    for (pos = 12; pos + 8 <= size;) {
        DWORD chunk = *(DWORD *)(data + pos + 4);
        BYTE *body = data + pos + 8;
        if (pos + 8 + chunk > size) break;
        if (!memcmp(data + pos, "fmt ", 4))
            CopyMemory(&format, body, chunk < sizeof(format) ? chunk : sizeof(format));
        else if (!memcmp(data + pos, "data", 4)) {
            pcm = body;
            pcm_size = chunk;
        }
        pos += 8 + ((chunk + 1) & ~1u);
    }
    if (!pcm || !pcm_size || !format.nBlockAlign || !format.nAvgBytesPerSec) {
        HeapFree(GetProcessHeap(), 0, data);
        return FALSE;
    }
    result = waveOutOpen(&music_wave, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        music_wave = NULL;
        HeapFree(GetProcessHeap(), 0, data);
        return FALSE;
    }
    if (offset_ms) {
        DWORD skip = (DWORD)((ULONGLONG)format.nAvgBytesPerSec * offset_ms / 1000);
        skip -= skip % format.nBlockAlign;
        if (skip < pcm_size) { pcm += skip; pcm_size -= skip; }
    }
    music_wave_file = data;
    music_wave_header.lpData = (LPSTR)pcm;
    music_wave_header.dwBufferLength = pcm_size;
    music_wave_header.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
    music_wave_header.dwLoops = 0xFFFFFFFFu;
    apply_music_volume();
    if (waveOutPrepareHeader(music_wave, &music_wave_header,
                             sizeof(music_wave_header)) != MMSYSERR_NOERROR ||
        waveOutWrite(music_wave, &music_wave_header,
                     sizeof(music_wave_header)) != MMSYSERR_NOERROR) {
        close_wave_music();
        return FALSE;
    }
    return TRUE;
}

static MCIERROR run_music_transport(const char *command) {
    MCIERROR error = mciSendStringA(command, NULL, 0, NULL);
    if (error && current_music_track) {
        char event[160];
        wsprintfA(event, "music transport failed command=%s error=%lu",
                  command, (DWORD)error);
        write_log(event);
    }
    return error;
}

static void close_actual_music_device(void) {
    MCIDEVICEID device = actual_music_device;
    if (device) {
        MCIERROR stop_error = mciSendCommandA(device, MCI_STOP, MCI_WAIT, 0);
        MCIERROR close_error = mciSendCommandA(device, MCI_CLOSE, MCI_WAIT, 0);
        char event[160];
        wsprintfA(event,
                  "music direct teardown device=%lu stop=%lu close=%lu",
                  (DWORD)device, (DWORD)stop_error, (DWORD)close_error);
        write_log(event);
        actual_music_device = 0;
    }
}

static void force_close_virtual_music(void) {
    /* Controller toggles mean "off", not the game's temporary pause/resume
       STOP. Tear down every MCI decoder so a completed/restarted ambience
       stream cannot survive underneath the newly selected music stream. */
    EnterCriticalSection(&music_lock);
    InterlockedIncrement(&music_generation);
    close_wave_music();
    close_actual_music_device();
    run_music_transport("stop all wait");
    run_music_transport("close all wait");
    current_music_track = 0;
    stopped_music_position = 0;
    music_stopped_at = 0;
    LeaveCriticalSection(&music_lock);
}

static MMRESULT WINAPI compatible_aux_get_volume(UINT device, LPDWORD volume) {
    if (music_backend == MUSIC_BACKEND_OFF) return MMSYSERR_NODRIVER;
    if (!virtual_music_enabled)
        return original_aux_get_volume(device, volume);
    if (volume) *volume = music_volume;
    return MMSYSERR_NOERROR;
}

static UINT WINAPI compatible_aux_get_num_devs(void) {
    if (music_backend == MUSIC_BACKEND_OFF) return 0;
    if (!virtual_music_enabled) return original_aux_get_num_devs();
    return 1;
}

static MMRESULT WINAPI compatible_aux_get_dev_caps(UINT_PTR device,
                                                    LPAUXCAPSA caps,
                                                    UINT size) {
    AUXCAPSA value;
    if (music_backend == MUSIC_BACKEND_OFF) return MMSYSERR_NODRIVER;
    if (!virtual_music_enabled)
        return original_aux_get_dev_caps(device, caps, size);
    if (device != 0 || !caps) return MMSYSERR_BADDEVICEID;
    ZeroMemory(&value, sizeof(value));
    value.wMid = MM_MICROSOFT;
    value.wPid = 1;
    value.vDriverVersion = 0x0100;
    lstrcpyA(value.szPname, "Pitfall Virtual CD Audio");
    value.wTechnology = AUXCAPS_CDAUDIO;
    value.dwSupport = AUXCAPS_VOLUME | AUXCAPS_LRVOLUME;
    CopyMemory(caps, &value, size < sizeof(value) ? size : sizeof(value));
    return MMSYSERR_NOERROR;
}

static MMRESULT WINAPI compatible_aux_set_volume(UINT device, DWORD volume) {
    if (music_backend == MUSIC_BACKEND_OFF) return MMSYSERR_NODRIVER;
    if (!virtual_music_enabled)
        return original_aux_set_volume(device, volume);
    music_volume = volume;
    if (current_music_track) apply_music_volume();
    return MMSYSERR_NOERROR;
}

static MCIERROR WINAPI compatible_mci_send_command(MCIDEVICEID device,
        UINT message, DWORD_PTR flags, DWORD_PTR parameter) {
    char track_path[MAX_PATH], command[MAX_PATH + 96];
    if (music_backend == MUSIC_BACKEND_OFF)
        return MCIERR_DEVICE_NOT_INSTALLED;
    if (!virtual_music_enabled)
        return original_mci_send_command(device, message, flags, parameter);
    if (message == MCI_OPEN && device == 0) {
        MCI_OPEN_PARMSA *open = (MCI_OPEN_PARMSA *)parameter;
        log_music_command("open", device, flags, 0);
        if (open) open->wDeviceID = MUSIC_DEVICE_ID;
        return 0;
    }
    if (device != MUSIC_DEVICE_ID)
        return original_mci_send_command(device, message, flags, parameter);
    switch (message) {
    case MCI_SET:
        return 0;
    case MCI_STATUS:
        if (parameter) {
            MCI_STATUS_PARMS *status = (MCI_STATUS_PARMS *)parameter;
            switch (status->dwItem) {
            case MCI_STATUS_LENGTH:
                if (status->dwTrack < 100 &&
                    virtual_track_lengths[status->dwTrack])
                    status->dwReturn = virtual_track_lengths[status->dwTrack];
                else
                    return MCIERR_OUTOFRANGE;
                break;
            case MCI_STATUS_NUMBER_OF_TRACKS:
                status->dwReturn = virtual_track_count;
                break;
            case MCI_STATUS_MEDIA_PRESENT:
            case MCI_STATUS_READY: status->dwReturn = TRUE; break;
            case MCI_STATUS_MODE: status->dwReturn = MCI_MODE_PLAY; break;
            case MCI_STATUS_CURRENT_TRACK: status->dwReturn = 2; break;
            case MCI_STATUS_TIME_FORMAT: status->dwReturn = MCI_FORMAT_TMSF; break;
            default: status->dwReturn = 0; break;
            }
        }
        return 0;
    case MCI_PLAY:
        if (parameter) {
            MCI_PLAY_PARMS *play = (MCI_PLAY_PARMS *)parameter;
            UINT track = MCI_TMSF_TRACK(play->dwFrom);
            UINT minute = MCI_TMSF_MINUTE(play->dwFrom);
            UINT second = MCI_TMSF_SECOND(play->dwFrom);
            UINT frame = MCI_TMSF_FRAME(play->dwFrom);
            DWORD offset = (minute * 60 + second) * 1000 + frame * 1000 / 75;
            log_music_command("play", device, flags, play->dwFrom);
            EnterCriticalSection(&music_lock);
            /* Transition and preference requests can arrive concurrently.
               Keep one wave device per active track. */
            if (track == current_music_track && music_wave &&
                !music_wave_paused && offset == 0) {
                LeaveCriticalSection(&music_lock);
                return 0;
            }
            /* Pitfall implements its pause menu as STOP followed by PLAY from
               the beginning of the same CD track.  A physical CD driver kept
               enough state for this to behave like resume. */
            if (track == current_music_track && stopped_music_position &&
                GetTickCount() - music_stopped_at < 15000 && offset == 0)
                offset = stopped_music_position;
            else if (track != current_music_track)
                stopped_music_position = 0;
            if (!find_music_track(track, track_path)) {
                LeaveCriticalSection(&music_lock);
                return MCIERR_FILE_NOT_FOUND;
            }
            if (track == current_music_track && music_wave && music_wave_paused) {
                waveOutRestart(music_wave);
                music_wave_paused = FALSE;
                stopped_music_position = 0;
                LeaveCriticalSection(&music_lock);
                return 0;
            }
            InterlockedIncrement(&music_generation);
            close_wave_music();
            close_actual_music_device();
            run_music_transport("stop all wait");
            run_music_transport("close all wait");
            if (!play_wave_music(track_path, offset)) {
                LeaveCriticalSection(&music_lock);
                return MCIERR_HARDWARE;
            }
            current_music_track = track;
            stopped_music_position = 0;
            LeaveCriticalSection(&music_lock);
        }
        return 0;
    case MCI_STOP:
        log_music_command("stop", device, flags, 0);
        EnterCriticalSection(&music_lock);
        InterlockedIncrement(&music_generation);
        {
            MMTIME position;
            ZeroMemory(&position, sizeof(position));
            position.wType = TIME_MS;
            if (music_wave && waveOutGetPosition(music_wave, &position,
                                                  sizeof(position)) == MMSYSERR_NOERROR)
                stopped_music_position = position.u.ms;
            else stopped_music_position = 0;
            music_stopped_at = GetTickCount();
        }
        if (music_wave) {
            waveOutPause(music_wave);
            music_wave_paused = TRUE;
        }
        LeaveCriticalSection(&music_lock);
        return 0;
    case MCI_PAUSE:
        log_music_command("pause", device, flags, 0);
        EnterCriticalSection(&music_lock);
        if (music_wave) {
            waveOutPause(music_wave);
            music_wave_paused = TRUE;
        }
        LeaveCriticalSection(&music_lock);
        return 0;
    case MCI_RESUME:
        log_music_command("resume", device, flags, 0);
        EnterCriticalSection(&music_lock);
        if (music_wave) {
            waveOutRestart(music_wave);
            music_wave_paused = FALSE;
        }
        LeaveCriticalSection(&music_lock);
        return 0;
    case MCI_CLOSE:
        log_music_command("close", device, flags, 0);
        EnterCriticalSection(&music_lock);
        InterlockedIncrement(&music_generation);
        close_wave_music();
        close_actual_music_device();
        run_music_transport("stop all wait");
        run_music_transport("close all wait");
        current_music_track = 0;
        stopped_music_position = 0;
        LeaveCriticalSection(&music_lock);
        return 0;
    default:
        return 0;
    }
}

static BOOL install_focus_hook(void) {
    DWORD old_protect, ignored;
    SIZE_T hook_bytes = (BYTE *)GAME_GETFOCUS_IAT -
                        (BYTE *)GAME_GETASYNCKEYSTATE_IAT + sizeof(void *);
    if (!VirtualProtect(GAME_GETASYNCKEYSTATE_IAT, hook_bytes, PAGE_READWRITE,
                        &old_protect))
        return FALSE;
    original_get_async_key_state =
        (SHORT (WINAPI *)(int))*GAME_GETASYNCKEYSTATE_IAT;
    original_get_focus = (HWND (WINAPI *)(void))*GAME_GETFOCUS_IAT;
    *GAME_GETASYNCKEYSTATE_IAT = compatible_get_async_key_state;
    *GAME_GETFOCUS_IAT = compatible_get_focus;
    VirtualProtect(GAME_GETASYNCKEYSTATE_IAT, hook_bytes, old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), GAME_GETASYNCKEYSTATE_IAT,
                          hook_bytes);
    return TRUE;
}

static BOOL install_music_hook(void) {
    DWORD old_protect, ignored;
    SIZE_T bytes = (BYTE *)GAME_MCISEND_IAT -
                   (BYTE *)GAME_AUXGETNUMDEVS_IAT + sizeof(void *);
    if (!VirtualProtect(GAME_AUXGETNUMDEVS_IAT, bytes, PAGE_READWRITE,
                        &old_protect))
        return FALSE;
    original_aux_get_num_devs =
        (UINT (WINAPI *)(void))*GAME_AUXGETNUMDEVS_IAT;
    original_aux_get_dev_caps =
        (MMRESULT (WINAPI *)(UINT_PTR, LPAUXCAPSA, UINT))
        *GAME_AUXGETDEVCAPS_IAT;
    original_aux_get_volume =
        (MMRESULT (WINAPI *)(UINT, LPDWORD))*GAME_AUXGETVOLUME_IAT;
    original_aux_set_volume =
        (MMRESULT (WINAPI *)(UINT, DWORD))*GAME_AUXSETVOLUME_IAT;
    original_mci_send_command =
        (MCIERROR (WINAPI *)(MCIDEVICEID, UINT, DWORD_PTR, DWORD_PTR))
        *GAME_MCISEND_IAT;
    *GAME_AUXGETNUMDEVS_IAT = compatible_aux_get_num_devs;
    *GAME_AUXGETDEVCAPS_IAT = compatible_aux_get_dev_caps;
    *GAME_AUXGETVOLUME_IAT = compatible_aux_get_volume;
    *GAME_AUXSETVOLUME_IAT = compatible_aux_set_volume;
    *GAME_MCISEND_IAT = compatible_mci_send_command;
    VirtualProtect(GAME_AUXGETNUMDEVS_IAT, bytes, old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), GAME_AUXGETNUMDEVS_IAT, bytes);
    return TRUE;
}

static void initialize_folder_music(void) {
    char executable[MAX_PATH], probe[MAX_PATH], ini[MAX_PATH];
    char backend[32], folder[MAX_PATH];
    char *slash;
    UINT track;
    virtual_music_enabled = FALSE;
    virtual_track_count = 0;
    ZeroMemory(virtual_track_lengths, sizeof(virtual_track_lengths));
    if (!GetModuleFileNameA(NULL, executable, sizeof(executable))) return;
    slash = executable + lstrlenA(executable);
    while (slash > executable && slash[-1] != '\\' && slash[-1] != '/') --slash;
    *slash = 0;
    wsprintfA(ini, "%s\\PITMOD.INI", executable);
    GetPrivateProfileStringA("Audio", "MusicBackend", "Auto",
                             backend, sizeof(backend), ini);
    if (!lstrcmpiA(backend, "Files")) music_backend = MUSIC_BACKEND_FILES;
    else if (!lstrcmpiA(backend, "CD")) music_backend = MUSIC_BACKEND_CD;
    else if (!lstrcmpiA(backend, "Off")) music_backend = MUSIC_BACKEND_OFF;
    else music_backend = MUSIC_BACKEND_AUTO;
    GetPrivateProfileStringA("Audio", "MusicFolder", "music",
                             folder, sizeof(folder), ini);
    if ((lstrlenA(folder) > 2 && folder[1] == ':') ||
        (folder[0] == '\\' && folder[1] == '\\'))
        lstrcpynA(music_root, folder, sizeof(music_root));
    else
        wsprintfA(music_root, "%s\\%s", executable, folder);
    if (music_backend == MUSIC_BACKEND_CD ||
        music_backend == MUSIC_BACKEND_OFF) return;
    /* Track 1 is the data track.  Only claim a virtual CD when at least one
       audio track exists, so an incomplete installation can still use a real
       disc through the game's original WinMM call. */
    for (track = 2; track <= 99; ++track) {
        if (find_music_track(track, probe)) {
            virtual_music_enabled = TRUE;
            virtual_track_lengths[track] = music_track_length(probe);
            if (track > virtual_track_count) virtual_track_count = track;
        }
    }
    if (music_backend == MUSIC_BACKEND_FILES)
        virtual_music_enabled = TRUE;
}

static void set_video_scaling_mode(int mode) {
    if (mode < 0 || mode > 5) mode = 0;
    video_scaling_mode = mode;
    video_stretch_fullscreen = mode == 5;
    video_integer_scaling = mode <= 2;
    if (mode == 1 || mode == 4)
        video_aspect = VIDEO_ASPECT_RAW;
    else if (mode == 2)
        video_aspect = VIDEO_ASPECT_16_9;
    else
        video_aspect = VIDEO_ASPECT_4_3;
}

static BOOL set_video_filter_mode_live(int mode) {
    IDirect3DSurface9 *replacement = NULL;
    int width, height;
    if (mode < VIDEO_FILTER_NONE || mode > VIDEO_FILTER_FSR)
        mode = VIDEO_FILTER_NONE;
    width = mode == VIDEO_FILTER_NONE ? 320 : 640;
    height = mode == VIDEO_FILTER_NONE ? 200 : 400;
    EnterCriticalSection(&d3d_lock);
    if (d3d_device && FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(
            d3d_device, width, height, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT,
            &replacement, NULL))) {
        LeaveCriticalSection(&d3d_lock);
        return FALSE;
    }
    if (d3d_frame) IDirect3DSurface9_Release(d3d_frame);
    d3d_frame = replacement;
    video_filter = mode;
    video_frame_width = width;
    video_frame_height = height;
    LeaveCriticalSection(&d3d_lock);
    return TRUE;
}

static void initialize_video_options(void) {
    char ini[MAX_PATH], value[32], aspect[32], scaling[32];
    get_pitmod_ini_path(ini);
    GetPrivateProfileStringA("Video", "Scaling", "__PITMOD_MISSING__",
                             scaling, sizeof(scaling), ini);
    if (lstrcmpA(scaling, "__PITMOD_MISSING__")) {
        if (!lstrcmpiA(scaling, "WideInteger") ||
            !lstrcmpiA(scaling, "Integer16:9"))
            set_video_scaling_mode(2);
        else if (!lstrcmpiA(scaling, "Integer") ||
                 !lstrcmpiA(scaling, "IntegerNative"))
            set_video_scaling_mode(1);
        else if (!lstrcmpiA(scaling, "FitNative") ||
                 !lstrcmpiA(scaling, "Raw"))
            set_video_scaling_mode(4);
        else if (!lstrcmpiA(scaling, "Stretch") ||
                 !lstrcmpiA(scaling, "StretchToDisplay"))
            set_video_scaling_mode(5);
        else if (!lstrcmpiA(scaling, "Fit4:3"))
            set_video_scaling_mode(3);
        else
            set_video_scaling_mode(0);
    } else {
        /* Accept the two-key format from earlier test builds. */
        video_integer_scaling = GetPrivateProfileIntA(
            "Video", "IntegerScaling", 0, ini) != 0;
        GetPrivateProfileStringA("Video", "AspectRatio", "4:3",
                                 aspect, sizeof(aspect), ini);
        if (video_integer_scaling)
            set_video_scaling_mode(
                !lstrcmpiA(aspect, "16:9") ? 2 :
                (!lstrcmpiA(aspect, "Raw") ||
                 !lstrcmpiA(aspect, "16:10")) ? 1 : 0);
        else
            set_video_scaling_mode(
                (!lstrcmpiA(aspect, "Raw") ||
                 !lstrcmpiA(aspect, "16:10")) ? 4 : 3);
    }
    GetPrivateProfileStringA("Video", "Filter", "None",
                             value, sizeof(value), ini);
    if (!lstrcmpiA(value, "Scanlines"))
        video_filter = VIDEO_FILTER_SCANLINES;
    else if (!lstrcmpiA(value, "CRT"))
        video_filter = VIDEO_FILTER_CRT;
    else if (!lstrcmpiA(value, "XBR"))
        video_filter = VIDEO_FILTER_XBR;
    else if (!lstrcmpiA(value, "FSR"))
        video_filter = VIDEO_FILTER_FSR;
    else
        video_filter = VIDEO_FILTER_NONE;
    video_frame_width = video_filter == VIDEO_FILTER_NONE ? 320 : 640;
    video_frame_height = video_filter == VIDEO_FILTER_NONE ? 200 : 400;
}

static BOOL start_d3d_presenter(void) {
    D3DPRESENT_PARAMETERS parameters;
    D3DDISPLAYMODEEX display_mode;
    HMONITOR target_monitor;
    UINT adapter, adapter_count;
    if (!render_window) return FALSE;
    if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d)) || !d3d)
        return FALSE;
    target_monitor = MonitorFromWindow(render_window, MONITOR_DEFAULTTONEAREST);
    adapter = D3DADAPTER_DEFAULT;
    adapter_count = IDirect3D9Ex_GetAdapterCount(d3d);
    {
        UINT candidate;
        for (candidate = 0; candidate < adapter_count; ++candidate) {
            if (IDirect3D9Ex_GetAdapterMonitor(d3d, candidate) == target_monitor) {
                adapter = candidate;
                break;
            }
        }
    }
    ZeroMemory(&display_mode, sizeof(display_mode));
    display_mode.Size = sizeof(display_mode);
    if (FAILED(IDirect3D9Ex_GetAdapterDisplayModeEx(d3d, adapter,
                                                    &display_mode, NULL))) {
        IDirect3D9Ex_Release(d3d);
        d3d = NULL;
        return FALSE;
    }
    ZeroMemory(&parameters, sizeof(parameters));
    parameters.Windowed = FALSE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.BackBufferWidth = display_mode.Width;
    parameters.BackBufferHeight = display_mode.Height;
    parameters.BackBufferFormat = display_mode.Format;
    parameters.FullScreen_RefreshRateInHz = display_mode.RefreshRate;
    parameters.hDeviceWindow = render_window;
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    if (FAILED(IDirect3D9Ex_CreateDeviceEx(d3d, adapter,
            D3DDEVTYPE_HAL, render_window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
            &parameters, &display_mode, &d3d_device))) {
        IDirect3D9Ex_Release(d3d);
        d3d = NULL;
        return FALSE;
    }
    IDirect3DDevice9Ex_SetMaximumFrameLatency(d3d_device, 1);
    if (FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(d3d_device,
            video_frame_width, video_frame_height,
            D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT,
            &d3d_frame, NULL))) {
        IDirect3DDevice9Ex_Release(d3d_device);
        IDirect3D9Ex_Release(d3d);
        d3d_device = NULL;
        d3d = NULL;
        return FALSE;
    }
    return TRUE;
}

static void stop_d3d_presenter(void) {
    EnterCriticalSection(&d3d_lock);
    if (d3d_frame) IDirect3DSurface9_Release(d3d_frame);
    if (d3d_device) IDirect3DDevice9Ex_Release(d3d_device);
    if (d3d) IDirect3D9Ex_Release(d3d);
    d3d_frame = NULL;
    d3d_device = NULL;
    d3d = NULL;
    LeaveCriticalSection(&d3d_lock);
}

static LONG measure_horizontal_motion(DWORD *matches_out) {
    LONG shift, best_shift = 0;
    DWORD best_matches = 0;
    int x, y;
    for (shift = -8; shift <= 8; ++shift) {
        DWORD matches = 0;
        for (y = 32; y < 168; y += 4) {
            for (x = 16; x < 304; x += 4) {
                int previous_x = x + shift;
                if (previous_x >= 0 && previous_x < 320 &&
                    converted_frame[y * 320 + x] ==
                    previous_changed_frame[y * 320 + previous_x])
                    ++matches;
            }
        }
        if (matches > best_matches) {
            best_matches = matches;
            best_shift = shift;
        }
    }
    *matches_out = best_matches;
    return best_shift;
}

static void __cdecl emulate_vblank(void) {
    static LARGE_INTEGER frequency, epoch;
    static unsigned long long tick;
    static int refresh_waits;
    LARGE_INTEGER now;
    LONGLONG target, remaining;
    int index;
    int present_after_wait = 0;
    if (InterlockedCompareExchange(&fullscreen_active, 0, 0))
        *GAME_ACTIVE_FLAG = 1;
    if (InterlockedCompareExchange(&fullscreen_active, 0, 0) &&
        render_window && IsWindow(render_window)) {
        if (InterlockedCompareExchange(&sync_direct_present, 1, 0) == 0)
            Sleep(3);
        present_after_wait = 1;
    }
    /* Direct3D Present(D3DPRESENT_INTERVAL_ONE) is the refresh wait.  Do not
       put an independent DWM clock in front of the swap chain. */
    if (present_after_wait && d3d_device) {
        draw_frame_once();
        return;
    }
    if (!refresh_waits) {
        DWM_TIMING_INFO timing;
        ZeroMemory(&timing, sizeof(timing));
        timing.cbSize = sizeof(timing);
        if (SUCCEEDED(DwmGetCompositionTimingInfo(NULL, &timing)) &&
            timing.rateRefresh.uiDenominator) {
            unsigned long hz = timing.rateRefresh.uiNumerator /
                               timing.rateRefresh.uiDenominator;
            refresh_waits = (int)((hz + 30) / 60);
            if (refresh_waits < 1) refresh_waits = 1;
            if (refresh_waits > 4) refresh_waits = 4;
        } else {
            refresh_waits = -1;
        }
    }
    if (refresh_waits > 0) {
        for (index = 0; index < refresh_waits; ++index) {
            if (FAILED(DwmFlush())) {
                refresh_waits = -1;
                break;
            }
        }
        if (refresh_waits > 0) {
            if (present_after_wait) draw_frame_once();
            return;
        }
    }
    /* Fallback for systems where desktop composition timing is unavailable. */
    if (!tick) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&epoch);
        tick = 1;
    }
    target = epoch.QuadPart +
        (LONGLONG)((tick * (unsigned long long)frequency.QuadPart) / 60ULL);
    QueryPerformanceCounter(&now);
    /* Do not run a burst of catch-up frames after a debugger stop, focus
       transition, loading pause, or other long interruption. */
    if (now.QuadPart - target > frequency.QuadPart / 30) {
        epoch = now;
        tick = 1;
        target = epoch.QuadPart + frequency.QuadPart / 60;
    }
    for (;;) {
        QueryPerformanceCounter(&now);
        remaining = target - now.QuadPart;
        if (remaining <= 0) break;
        if (remaining * 1000 > frequency.QuadPart * 2)
            Sleep(1);
        else
            SwitchToThread();
    }
    if (present_after_wait) draw_frame_once();
    ++tick;
}

static DWORD WINAPI run_game_timer(void *unused) {
    LARGE_INTEGER frequency, start, now;
    unsigned long long tick = 1;
    (void)unused;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    while (InterlockedCompareExchange(&game_timer_running, 1, 1)) {
        LONGLONG target = start.QuadPart +
            (LONGLONG)((tick * (unsigned long long)frequency.QuadPart) / 60ULL);
        for (;;) {
            LONGLONG remaining;
            QueryPerformanceCounter(&now);
            remaining = target - now.QuadPart;
            if (remaining <= 0 ||
                !InterlockedCompareExchange(&game_timer_running, 1, 1)) break;
            if (remaining * 1000 > frequency.QuadPart * 2)
                Sleep(1);
            else
                SwitchToThread();
        }
        if (!InterlockedCompareExchange(&game_timer_running, 1, 1)) break;
        game_timer_callback(PIT_TIMER_ID, 0, game_timer_user, 0, 0);
        ++tick;
    }
    return 0;
}

static MMRESULT WINAPI replacement_time_set_event(
        UINT delay, UINT resolution, LPTIMECALLBACK callback,
        DWORD_PTR user, UINT flags) {
    if (delay == 17 && callback == GAME_TIMER_CALLBACK &&
        (flags & TIME_PERIODIC)) {
        if (InterlockedCompareExchange(&game_timer_running, 1, 0) == 0) {
            game_timer_callback = callback;
            game_timer_user = user;
            game_timer_thread = CreateThread(NULL, 0, run_game_timer,
                                              NULL, 0, NULL);
            if (game_timer_thread) {
                write_log("exact 60 Hz gameplay timer active");
                return PIT_TIMER_ID;
            }
            InterlockedExchange(&game_timer_running, 0);
        }
    }
    return original_time_set_event(delay, resolution, callback, user, flags);
}

static MMRESULT WINAPI replacement_time_kill_event(UINT timer_id) {
    if (timer_id == PIT_TIMER_ID) {
        InterlockedExchange(&game_timer_running, 0);
        if (game_timer_thread) {
            WaitForSingleObject(game_timer_thread, 1000);
            CloseHandle(game_timer_thread);
            game_timer_thread = NULL;
        }
        return TIMERR_NOERROR;
    }
    return original_time_kill_event(timer_id);
}

static BOOL install_timer_hooks(void) {
    DWORD old_protect;
    original_time_set_event = (void *)*GAME_TIMESET_IAT;
    original_time_kill_event = (void *)*GAME_TIMEKILL_IAT;
    if (!VirtualProtect(GAME_TIMEKILL_IAT,
                        (BYTE *)GAME_TIMESET_IAT - (BYTE *)GAME_TIMEKILL_IAT +
                            sizeof(void *),
                        PAGE_READWRITE, &old_protect)) return FALSE;
    *GAME_TIMESET_IAT = replacement_time_set_event;
    *GAME_TIMEKILL_IAT = replacement_time_kill_event;
    VirtualProtect(GAME_TIMEKILL_IAT,
                   (BYTE *)GAME_TIMESET_IAT - (BYTE *)GAME_TIMEKILL_IAT +
                       sizeof(void *),
                   old_protect, &old_protect);
    FlushInstructionCache(GetCurrentProcess(), GAME_TIMEKILL_IAT,
                          (BYTE *)GAME_TIMESET_IAT - (BYTE *)GAME_TIMEKILL_IAT +
                              sizeof(void *));
    return TRUE;
}
typedef struct {
    BITMAPINFOHEADER header;
    RGBQUAD colors[256];
} FRAME_INFO;

static void write_log(const char *event) {
    SYSTEMTIME time;
    char line[256];
    DWORD written;
    HANDLE file;
    GetLocalTime(&time);
    wsprintfA(line, "%02u:%02u:%02u.%03u %s game=%p display=%p presenting=%ld\r\n",
              time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
              event, game_window, display_window,
              InterlockedCompareExchange(&presenting, 0, 0));
    file = CreateFileA("pitmod.log", FILE_APPEND_DATA,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        WriteFile(file, line, lstrlenA(line), &written, NULL);
        CloseHandle(file);
    }
}

static void write_timing_log(unsigned long frames, LONGLONG total,
                             LONGLONG maximum, LONGLONG frequency,
                             unsigned long changes, unsigned long duplicates,
                             unsigned long longest_repeat) {
    char event[240];
    unsigned long average_us = frames
        ? (unsigned long)((total * 1000000) / frequency / frames) : 0;
    unsigned long maximum_us = (unsigned long)((maximum * 1000000) / frequency);
    wsprintfA(event, "present timing: frames=%lu average=%lu.%03lu ms max=%lu.%03lu ms changes=%lu duplicates=%lu longest-repeat=%lu",
              frames, average_us / 1000, average_us % 1000,
              maximum_us / 1000, maximum_us % 1000,
              changes, duplicates, longest_repeat);
    write_log(event);
}

static void write_change_samples(void) {
    HANDLE file;
    DWORD written;
    unsigned long index;
    char line[160];
    file = CreateFileA("pitmod-timing.csv", GENERIC_WRITE,
                       FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    lstrcpyA(line, "sample,time_us,interval_us,changed,hash,present_us,present_result,camera_x,camera_y,motion_shift,motion_matches\r\n");
    WriteFile(file, line, lstrlenA(line), &written, NULL);
    for (index = 0; index < change_sample_count; ++index) {
        wsprintfA(line, "%lu,%lu,%lu,%lu,%08lX,%lu,%08lX,%ld,%ld,%ld,%lu\r\n", index + 1,
                  change_samples[index].time_us,
                  change_samples[index].interval_us,
                  change_samples[index].changed,
                  change_samples[index].hash,
                  change_samples[index].present_us,
                  change_samples[index].present_result,
                  change_samples[index].camera_x,
                  change_samples[index].camera_y,
                  change_samples[index].motion_shift,
                  change_samples[index].motion_matches);
        WriteFile(file, line, lstrlenA(line), &written, NULL);
    }
    CloseHandle(file);
}

static uint32_t frame_hash(void) {
    const BYTE *bytes = (const BYTE *)converted_frame;
    uint32_t hash = 2166136261u;
    unsigned long index;
    for (index = 0; index < sizeof(converted_frame); ++index) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return hash;
}

static void convert_frame(void) {
    BYTE *palette = GAME_PALETTE;
    int source_row, x;
    ZeroMemory(converted_frame, sizeof(converted_frame));
    for (source_row = 0; source_row < 199; ++source_row) {
        /* Pitfall copies 320 bytes with REP MOVSD, which advances EDI by 320,
           and then subtracts 640. The effective start-to-start row stride is
           therefore 320 bytes upward through the bottom-up framebuffer. */
        BYTE *source = FB_BITS - source_row * 320;
        uint32_t *destination = converted_frame + (198 - source_row) * 320;
        for (x = 0; x < 320; ++x) {
            BYTE index = source[x];
            BYTE *color = palette + index * 4;
            /* GAME_PALETTE points at the red byte within each four-byte
               record. The VGA upload reads red, then the two preceding bytes
               as green and blue. */
            destination[x] = ((uint32_t)color[0] << 16) |
                             ((uint32_t)color[-1] << 8) |
                             (uint32_t)color[-2];
        }
    }
}

static uint32_t scale_color(uint32_t color, unsigned int red,
                            unsigned int green, unsigned int blue) {
    return ((((color >> 16) & 255) * red / 100) << 16) |
           ((((color >> 8) & 255) * green / 100) << 8) |
           ((color & 255) * blue / 100);
}

static unsigned int color_luma(uint32_t color) {
    return (((color >> 16) & 255) * 77 +
            ((color >> 8) & 255) * 150 +
            (color & 255) * 29) >> 8;
}

static uint32_t lerp_color(uint32_t a, uint32_t b, unsigned int weight) {
    unsigned int inverse = 256 - weight;
    unsigned int red = (((a >> 16) & 255) * inverse +
                        ((b >> 16) & 255) * weight + 128) >> 8;
    unsigned int green = (((a >> 8) & 255) * inverse +
                          ((b >> 8) & 255) * weight + 128) >> 8;
    unsigned int blue = ((a & 255) * inverse +
                         (b & 255) * weight + 128) >> 8;
    return (red << 16) | (green << 8) | blue;
}

static int clamp_channel(int value) {
    return value < 0 ? 0 : value > 255 ? 255 : value;
}

static const uint32_t *apply_fsr_filter(void) {
    int x, y;
    /* FSR 1 is a spatial upscaler: EASU first reconstructs edges and RCAS
       then restores local contrast. This CPU/D3D9 adaptation uses the same
       two-stage design at the game's natural 2x scale. The edge pass selects
       interpolation along, rather than across, the dominant local edge. */
    for (y = 0; y < 400; ++y) {
        int sy = y ? (2 * y - 1) / 4 : 0;
        unsigned int fy = y ? (unsigned int)((2 * y - 1) & 3) * 64 : 0;
        int sy1 = sy < 199 ? sy + 1 : sy;
        for (x = 0; x < 640; ++x) {
            int sx = x ? (2 * x - 1) / 4 : 0;
            unsigned int fx = x ? (unsigned int)((2 * x - 1) & 3) * 64 : 0;
            int sx1 = sx < 319 ? sx + 1 : sx;
            int cx = fx < 128 ? sx : sx1;
            int cy = fy < 128 ? sy : sy1;
            int left = cx ? cx - 1 : cx;
            int right = cx < 319 ? cx + 1 : cx;
            int up = cy ? cy - 1 : cy;
            int down = cy < 199 ? cy + 1 : cy;
            uint32_t p00 = converted_frame[sy * 320 + sx];
            uint32_t p10 = converted_frame[sy * 320 + sx1];
            uint32_t p01 = converted_frame[sy1 * 320 + sx];
            uint32_t p11 = converted_frame[sy1 * 320 + sx1];
            uint32_t horizontal0 = lerp_color(p00, p10, fx);
            uint32_t horizontal1 = lerp_color(p01, p11, fx);
            uint32_t bilinear = lerp_color(horizontal0, horizontal1, fy);
            unsigned int gx = color_luma(converted_frame[cy * 320 + right]);
            unsigned int gx0 = color_luma(converted_frame[cy * 320 + left]);
            unsigned int gy = color_luma(converted_frame[down * 320 + cx]);
            unsigned int gy0 = color_luma(converted_frame[up * 320 + cx]);
            unsigned int dx = gx > gx0 ? gx - gx0 : gx0 - gx;
            unsigned int dy = gy > gy0 ? gy - gy0 : gy0 - gy;
            unsigned int total = dx + dy;
            uint32_t directional = bilinear;
            if (dx > dy) {
                uint32_t top = converted_frame[sy * 320 + cx];
                uint32_t bottom = converted_frame[sy1 * 320 + cx];
                directional = lerp_color(top, bottom, fy);
            } else if (dy > dx) {
                uint32_t a = converted_frame[cy * 320 + sx];
                uint32_t b = converted_frame[cy * 320 + sx1];
                directional = lerp_color(a, b, fx);
            }
            if (total) {
                unsigned int edge = (dx > dy ? dx - dy : dy - dx) * 192 /
                                    total;
                bilinear = lerp_color(bilinear, directional, edge);
            }
            fsr_easu_frame[y * 640 + x] = bilinear;
        }
    }
    /* Mild RCAS-style cross sharpening with neighborhood clipping. */
    for (y = 0; y < 400; ++y) {
        int up = y ? y - 1 : y;
        int down = y < 399 ? y + 1 : y;
        for (x = 0; x < 640; ++x) {
            int left = x ? x - 1 : x;
            int right = x < 639 ? x + 1 : x;
            uint32_t c = fsr_easu_frame[y * 640 + x];
            uint32_t n = fsr_easu_frame[up * 640 + x];
            uint32_t s = fsr_easu_frame[down * 640 + x];
            uint32_t w = fsr_easu_frame[y * 640 + left];
            uint32_t e = fsr_easu_frame[y * 640 + right];
            int cr = (c >> 16) & 255, cg = (c >> 8) & 255, cb = c & 255;
            int rr = cr + (4 * cr - (int)((n >> 16) & 255) -
                (int)((s >> 16) & 255) - (int)((w >> 16) & 255) -
                (int)((e >> 16) & 255)) * 18 / 100;
            int rg = cg + (4 * cg - (int)((n >> 8) & 255) -
                (int)((s >> 8) & 255) - (int)((w >> 8) & 255) -
                (int)((e >> 8) & 255)) * 18 / 100;
            int rb = cb + (4 * cb - (int)(n & 255) - (int)(s & 255) -
                (int)(w & 255) - (int)(e & 255)) * 18 / 100;
            filtered_frame[y * 640 + x] =
                ((uint32_t)clamp_channel(rr) << 16) |
                ((uint32_t)clamp_channel(rg) << 8) |
                (uint32_t)clamp_channel(rb);
        }
    }
    return filtered_frame;
}

static const uint32_t *apply_video_filter(void) {
    int x, y;
    if (video_filter == VIDEO_FILTER_NONE) return converted_frame;
    if (video_filter == VIDEO_FILTER_FSR) return apply_fsr_filter();
    if (video_filter == VIDEO_FILTER_XBR) {
        /* A compact Scale2x-style edge-aware pass. It preserves hard pixel
           edges while smoothing diagonal stair-steps, which gives this
           artwork the useful part of an xBR-like treatment without a shader
           dependency. */
        for (y = 0; y < 200; ++y) {
            for (x = 0; x < 320; ++x) {
                uint32_t p = converted_frame[y * 320 + x];
                uint32_t b = converted_frame[(y ? y - 1 : y) * 320 + x];
                uint32_t d = converted_frame[y * 320 + (x ? x - 1 : x)];
                uint32_t f = converted_frame[y * 320 + (x < 319 ? x + 1 : x)];
                uint32_t h = converted_frame[(y < 199 ? y + 1 : y) * 320 + x];
                uint32_t e0 = p, e1 = p, e2 = p, e3 = p;
                if (b != h && d != f) {
                    if (d == b) e0 = d;
                    if (b == f) e1 = f;
                    if (d == h) e2 = d;
                    if (h == f) e3 = f;
                }
                filtered_frame[(y * 2) * 640 + x * 2] = e0;
                filtered_frame[(y * 2) * 640 + x * 2 + 1] = e1;
                filtered_frame[(y * 2 + 1) * 640 + x * 2] = e2;
                filtered_frame[(y * 2 + 1) * 640 + x * 2 + 1] = e3;
            }
        }
        return filtered_frame;
    }
    for (y = 0; y < 400; ++y) {
        BOOL dark = (y & 1) != 0;
        for (x = 0; x < 640; ++x) {
            uint32_t color = converted_frame[(y >> 1) * 320 + (x >> 1)];
            if (video_filter == VIDEO_FILTER_SCANLINES) {
                if (dark) color = scale_color(color, 68, 68, 68);
            } else {
                unsigned int r = 82, g = 82, b = 82;
                if (x % 3 == 0) r = 100;
                else if (x % 3 == 1) g = 100;
                else b = 100;
                if (dark) { r = r * 78 / 100; g = g * 78 / 100; b = b * 78 / 100; }
                color = scale_color(color, r, g, b);
            }
            filtered_frame[y * 640 + x] = color;
        }
    }
    return filtered_frame;
}

static void draw_frame_once(void) {
    FRAME_INFO info;
    RECT client, target;
    HDC dc;
    D3DLOCKED_RECT locked;
    IDirect3DSurface9 *backbuffer;
    int row;
    LARGE_INTEGER present_start, present_end;
    uint32_t current_hash;
    CHANGE_SAMPLE *direct_sample = NULL;
    HRESULT present_result;
    const uint32_t *video_frame;
    if (!render_window || !IsWindow(render_window)) return;
    ZeroMemory(&info, sizeof(info));
    info.header.biSize = sizeof(BITMAPINFOHEADER);
    info.header.biWidth = video_frame_width;
    info.header.biHeight = -video_frame_height;
    info.header.biPlanes = 1;
    info.header.biBitCount = 32;
    info.header.biCompression = BI_RGB;
    GetClientRect(render_window, &client);
    fit_video_target(&target, client.right, client.bottom);
    convert_frame();
    video_frame = apply_video_filter();
    EnterCriticalSection(&d3d_lock);
    if (d3d_device && d3d_frame) {
        QueryPerformanceCounter(&present_start);
        current_hash = frame_hash();
        if (change_sample_count < MAX_CHANGE_SAMPLES) {
            direct_sample = &change_samples[change_sample_count++];
            direct_sample->time_us = (DWORD)(((present_start.QuadPart - direct_capture_start.QuadPart) * 1000000) /
                                              capture_frequency);
            direct_sample->interval_us = direct_sample_valid
                ? (DWORD)(((present_start.QuadPart - direct_last_sample.QuadPart) * 1000000) /
                          capture_frequency) : 0;
            direct_sample->changed = !direct_sample_valid || current_hash != direct_previous_hash;
            direct_sample->hash = current_hash;
            direct_sample->present_us = 0;
            direct_sample->present_result = 0;
            direct_sample->camera_x = *GAME_CAMERA_X;
            direct_sample->camera_y = *GAME_CAMERA_Y;
            direct_sample->motion_shift = 0;
            direct_sample->motion_matches = 0;
        }
        direct_previous_hash = current_hash;
        direct_last_sample = present_start;
        direct_sample_valid = TRUE;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(d3d_frame, &locked, NULL, 0))) {
            for (row = 0; row < video_frame_height; ++row)
                CopyMemory((BYTE *)locked.pBits + row * locked.Pitch,
                           video_frame + row * video_frame_width,
                           video_frame_width * sizeof(uint32_t));
            IDirect3DSurface9_UnlockRect(d3d_frame);
            backbuffer = NULL;
            if (SUCCEEDED(IDirect3DDevice9_GetBackBuffer(d3d_device, 0, 0,
                    D3DBACKBUFFER_TYPE_MONO, &backbuffer))) {
                IDirect3DDevice9_ColorFill(d3d_device, backbuffer, NULL,
                                           D3DCOLOR_XRGB(0, 0, 0));
                IDirect3DDevice9_StretchRect(d3d_device, d3d_frame, NULL,
                                             backbuffer, &target, D3DTEXF_POINT);
                IDirect3DSurface9_Release(backbuffer);
                present_result = IDirect3DDevice9Ex_PresentEx(
                    d3d_device, NULL, NULL, NULL, NULL, 0);
                QueryPerformanceCounter(&present_end);
                if (direct_sample) {
                    direct_sample->present_us = (DWORD)(((present_end.QuadPart - present_start.QuadPart) * 1000000) /
                                                         capture_frequency);
                    direct_sample->present_result = present_result;
                }
                LeaveCriticalSection(&d3d_lock);
                return;
            }
        }
    }
    LeaveCriticalSection(&d3d_lock);
    dc = GetDC(render_window);
    if (dc) {
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(dc,
            target.left, target.top,
            target.right - target.left, target.bottom - target.top,
            0, 0, video_frame_width, video_frame_height, video_frame,
            (BITMAPINFO *)&info, DIB_RGB_COLORS, SRCCOPY);
        /* StretchDIBits may remain in the calling thread's GDI batch after
           ReleaseDC.  Submit it now so the compositor cannot occasionally
           reach its next refresh with the preceding frame still queued. */
        GdiFlush();
        ReleaseDC(render_window, dc);
    }
}

static void fit_4_3(RECT *out, int width, int height) {
    int draw_width = width;
    int draw_height = (draw_width * 3) / 4;
    if (draw_height > height) {
        draw_height = height;
        draw_width = (draw_height * 4) / 3;
    }
    out->left = (width - draw_width) / 2;
    out->top = (height - draw_height) / 2;
    out->right = out->left + draw_width;
    out->bottom = out->top + draw_height;
}

static void fit_video_target(RECT *out, int width, int height) {
    int aspect_numerator = video_aspect == VIDEO_ASPECT_16_9 ? 16 : 4;
    int aspect_denominator = video_aspect == VIDEO_ASPECT_16_9 ? 9 : 3;
    if (video_stretch_fullscreen) {
        out->left = 0;
        out->top = 0;
        out->right = width;
        out->bottom = height;
    } else if (video_integer_scaling) {
        /* Preserve an exact multiple of the native 200-line height, then
           stretch only the width to the selected display aspect. This gives
           the closest fitting aspect-correct viewport while keeping vertical
           pixels and scanline spacing stable. */
        int scale_y = height / 200;
        int scale_x = video_aspect == VIDEO_ASPECT_RAW
            ? width / 320
            : (width * aspect_denominator) / (200 * aspect_numerator);
        int scale = scale_x < scale_y ? scale_x : scale_y;
        int draw_width, draw_height;
        if (scale < 1) scale = 1;
        draw_height = 200 * scale;
        draw_width = video_aspect == VIDEO_ASPECT_RAW
            ? 320 * scale
            : (draw_height * aspect_numerator + aspect_denominator / 2) /
              aspect_denominator;
        out->left = (width - draw_width) / 2;
        out->top = (height - draw_height) / 2;
        out->right = out->left + draw_width;
        out->bottom = out->top + draw_height;
    } else {
        if (video_aspect == VIDEO_ASPECT_RAW) {
            int draw_width = width;
            int draw_height = draw_width * 5 / 8;
            if (draw_height > height) {
                draw_height = height;
                draw_width = draw_height * 8 / 5;
            }
            out->left = (width - draw_width) / 2;
            out->top = (height - draw_height) / 2;
            out->right = out->left + draw_width;
            out->bottom = out->top + draw_height;
        } else if (video_aspect == VIDEO_ASPECT_16_9) {
            int draw_width = width;
            int draw_height = draw_width * 9 / 16;
            if (draw_height > height) {
                draw_height = height;
                draw_width = draw_height * 16 / 9;
            }
            out->left = (width - draw_width) / 2;
            out->top = (height - draw_height) / 2;
            out->right = out->left + draw_width;
            out->bottom = out->top + draw_height;
        } else {
            fit_4_3(out, width, height);
        }
    }
}

static DWORD WINAPI present_frames(void *unused) {
    FRAME_INFO info;
    const uint32_t *video_frame;
    LARGE_INTEGER frequency, previous, now;
    LONGLONG total_ticks = 0, maximum_ticks = 0;
    unsigned long timed_frames = 0;
    unsigned long changes = 0, duplicates = 0;
    unsigned long repeat_run = 0, longest_repeat = 0;
    uint32_t previous_hash = 0;
    BOOL previous_changed_valid = FALSE;
    LARGE_INTEGER capture_start, last_change;
    (void)unused;
    ZeroMemory(&info, sizeof(info));
    info.header.biSize = sizeof(BITMAPINFOHEADER);
    info.header.biWidth = video_frame_width;
    info.header.biHeight = -video_frame_height;
    info.header.biPlanes = 1;
    info.header.biBitCount = 32;
    info.header.biCompression = BI_RGB;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous);
    capture_start = previous;
    last_change = previous;
    capture_frequency = frequency.QuadPart;
    change_sample_count = 0;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    while (InterlockedCompareExchange(&presenting, 1, 1)) {
        RECT client, target;
        HDC dc;
        *GAME_ACTIVE_FLAG = 1;
        if (InterlockedCompareExchange(&sync_direct_present, 0, 0)) {
            Sleep(2);
            continue;
        }
        if (!game_window || !IsWindow(game_window)) {
            game_window = FindWindowA(NULL, "Pitfall: The Mayan Adventure");
            Sleep(10);
            continue;
        }
        if (!render_window || !IsWindow(render_window)) {
            Sleep(10);
            continue;
        }
        GetClientRect(render_window, &client);
        fit_video_target(&target, client.right, client.bottom);
        convert_frame();
        video_frame = apply_video_filter();
        {
            uint32_t current_hash = frame_hash();
            if (timed_frames != 0 && current_hash == previous_hash) {
                ++duplicates;
                ++repeat_run;
                if (repeat_run > longest_repeat) longest_repeat = repeat_run;
            } else {
                if (timed_frames != 0) {
                    LARGE_INTEGER changed_at;
                    ++changes;
                    QueryPerformanceCounter(&changed_at);
                    if (change_sample_count < MAX_CHANGE_SAMPLES) {
                        change_samples[change_sample_count].time_us =
                            (DWORD)(((changed_at.QuadPart - capture_start.QuadPart) * 1000000) /
                                    frequency.QuadPart);
                        change_samples[change_sample_count].interval_us =
                            (DWORD)(((changed_at.QuadPart - last_change.QuadPart) * 1000000) /
                                    frequency.QuadPart);
                        change_samples[change_sample_count].changed = 1;
                        change_samples[change_sample_count].hash = current_hash;
                        change_samples[change_sample_count].present_us = 0;
                        change_samples[change_sample_count].present_result = 0;
                        change_samples[change_sample_count].camera_x = *GAME_CAMERA_X;
                        change_samples[change_sample_count].camera_y = *GAME_CAMERA_Y;
                        if (previous_changed_valid) {
                            change_samples[change_sample_count].motion_shift =
                                measure_horizontal_motion(
                                    &change_samples[change_sample_count].motion_matches);
                        } else {
                            change_samples[change_sample_count].motion_shift = 0;
                            change_samples[change_sample_count].motion_matches = 0;
                        }
                        CopyMemory(previous_changed_frame, converted_frame,
                                   sizeof(previous_changed_frame));
                        previous_changed_valid = TRUE;
                        ++change_sample_count;
                    }
                    last_change = changed_at;
                }
                repeat_run = 0;
            }
            previous_hash = current_hash;
        }
        dc = GetDC(render_window);
        if (dc) {
            SetStretchBltMode(dc, COLORONCOLOR);
            StretchDIBits(dc,
                target.left, target.top, target.right - target.left, target.bottom - target.top,
                0, 0, video_frame_width, video_frame_height, video_frame,
                (BITMAPINFO *)&info, DIB_RGB_COLORS, SRCCOPY);
            ReleaseDC(render_window, dc);
        }
        DwmFlush();
        /* With the emulated retrace aligned to DWM, Pitfall starts drawing
           immediately after the same refresh boundary. Sampling right here
           can therefore capture a half-written sprite. Offset our read of
           the next frame until the game's small 320x200 draw has finished. */
        Sleep(2);
        QueryPerformanceCounter(&now);
        if (timed_frames != 0) {
            LONGLONG elapsed = now.QuadPart - previous.QuadPart;
            total_ticks += elapsed;
            if (elapsed > maximum_ticks) maximum_ticks = elapsed;
        }
        previous = now;
        ++timed_frames;
    }
    if (timed_frames > 1)
        write_timing_log(timed_frames - 1, total_ticks, maximum_ticks,
                         frequency.QuadPart, changes, duplicates,
                         longest_repeat);
    write_change_samples();
    return 0;
}

static void enter_fullscreen(void) {
    MONITORINFO monitor = { sizeof(monitor) };
    if (!game_window) game_window = FindWindowA(NULL, "Pitfall: The Mayan Adventure");
    if (InterlockedCompareExchange(&fullscreen_active, 0, 0)) return;
    if (!game_window || !display_window) {
        write_log("enter rejected: required window missing");
        return;
    }
    render_window = display_window;
    QueryPerformanceFrequency((LARGE_INTEGER *)&capture_frequency);
    QueryPerformanceCounter(&direct_capture_start);
    direct_last_sample = direct_capture_start;
    direct_previous_hash = 0;
    direct_sample_valid = FALSE;
    change_sample_count = 0;
    ZeroMemory((void *)fullscreen_key_down, sizeof(fullscreen_key_down));
    InterlockedExchange(&sync_direct_present, 0);
    saved_style = GetWindowLongA(game_window, GWL_STYLE);
    saved_exstyle = GetWindowLongA(game_window, GWL_EXSTYLE);
    saved_menu = GetMenu(game_window);
    GetWindowRect(game_window, &saved_rect);
    GetMonitorInfoA(MonitorFromWindow(game_window, MONITOR_DEFAULTTONEAREST), &monitor);
    SetMenu(game_window, NULL);
    SetWindowLongA(game_window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowLongA(game_window, GWL_EXSTYLE,
                   saved_exstyle & ~(WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE));
    SetWindowPos(game_window, HWND_TOPMOST,
        monitor.rcMonitor.left, monitor.rcMonitor.top,
        monitor.rcMonitor.right - monitor.rcMonitor.left,
        monitor.rcMonitor.bottom - monitor.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    SetWindowLongA(render_window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowLongA(render_window, GWL_EXSTYLE,
                   WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT);
    SetWindowPos(render_window, HWND_TOPMOST,
        monitor.rcMonitor.left, monitor.rcMonitor.top,
        monitor.rcMonitor.right - monitor.rcMonitor.left,
        monitor.rcMonitor.bottom - monitor.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    if (start_d3d_presenter())
        write_log("Direct3D9Ex exclusive presenter active");
    else
        write_log("Direct3D9 unavailable; using GDI presenter");
    {
        RECT client;
        HDC dc = GetDC(render_window);
        if (dc) {
            GetClientRect(render_window, &client);
            PatBlt(dc, 0, 0, client.right, client.bottom, BLACKNESS);
            ReleaseDC(render_window, dc);
        }
    }
    if (InterlockedCompareExchange(&presenting, 1, 0) == 0) {
        presenter_thread = CreateThread(NULL, 0, present_frames, NULL, 0, NULL);
    }
    InterlockedExchange(&fullscreen_active, 1);
    *GAME_ACTIVE_FLAG = 1;
    write_log("entered fullscreen");
}

static void leave_fullscreen(void) {
    /* Stop new game-thread presents before waiting for/releasing the device. */
    if (InterlockedCompareExchange(&fullscreen_active, 0, 1) != 1) return;
    InterlockedExchange(&presenting, 0);
    InterlockedExchange(&sync_direct_present, 0);
    ZeroMemory((void *)fullscreen_key_down, sizeof(fullscreen_key_down));
    if (presenter_thread) {
        WaitForSingleObject(presenter_thread, 1000);
        CloseHandle(presenter_thread);
        presenter_thread = NULL;
    }
    stop_d3d_presenter();
    if (game_window) {
        SetWindowLongA(game_window, GWL_STYLE, saved_style);
        SetWindowLongA(game_window, GWL_EXSTYLE, saved_exstyle);
        SetMenu(game_window, saved_menu);
        SetWindowPos(game_window, HWND_NOTOPMOST,
            saved_rect.left, saved_rect.top,
            saved_rect.right - saved_rect.left, saved_rect.bottom - saved_rect.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    if (display_window && display_window != game_window)
        ShowWindow(display_window, SW_HIDE);
    render_window = NULL;
    write_log("left fullscreen");
}

typedef struct {
    BOOL found_music;
    BOOL found_ambient;
    BOOL music_checked;
    BOOL ambient_checked;
} SOUND_CHECKBOX_SCAN;

static BOOL CALLBACK restore_sound_checkbox(HWND child, LPARAM parameter) {
    SOUND_CHECKBOX_SCAN *scan = (SOUND_CHECKBOX_SCAN *)parameter;
    char text[64];
    GetWindowTextA(child, text, sizeof(text));
    if (!lstrcmpA(text, "CD Music")) {
        SendMessageA(child, BM_SETCHECK,
                     InterlockedCompareExchange(&preferred_music_enabled, 0, 0)
                         ? BST_CHECKED : BST_UNCHECKED, 0);
        scan->found_music = TRUE;
    } else if (!lstrcmpA(text, "CD Ambient Sounds")) {
        SendMessageA(child, BM_SETCHECK,
                     InterlockedCompareExchange(&preferred_ambient_enabled, 0, 0)
                         ? BST_CHECKED : BST_UNCHECKED, 0);
        scan->found_ambient = TRUE;
    }
    return TRUE;
}

static BOOL CALLBACK capture_sound_checkbox(HWND child, LPARAM parameter) {
    SOUND_CHECKBOX_SCAN *scan = (SOUND_CHECKBOX_SCAN *)parameter;
    char text[64];
    GetWindowTextA(child, text, sizeof(text));
    if (!lstrcmpA(text, "CD Music")) {
        scan->found_music = TRUE;
        scan->music_checked = SendMessageA(child, BM_GETCHECK, 0, 0) ==
                              BST_CHECKED;
    } else if (!lstrcmpA(text, "CD Ambient Sounds")) {
        scan->found_ambient = TRUE;
        scan->ambient_checked = SendMessageA(child, BM_GETCHECK, 0, 0) ==
                                BST_CHECKED;
    }
    return TRUE;
}

static void set_control_font(HWND control) {
    SendMessageA(control, WM_SETFONT,
                 (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

static const char *xinput_button_name(int value) {
    static const char *names[] = {
        "None", "A", "B", "X", "Y", "Left Bumper", "Right Bumper",
        "Back / View", "Start / Menu", "Left Stick", "Right Stick",
        "Left Trigger", "Right Trigger"
    };
    if (value < 0 || value > 12) value = 0;
    return names[value];
}

static HWND create_binding_button(HWND page, int id, int x, int y, int value) {
    HWND button = CreateWindowExA(0, "BUTTON", xinput_button_name(value),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        x, y, 112, 20, page, (HMENU)(INT_PTR)id, dll_instance, NULL);
    set_control_font(button);
    SetWindowLongPtrA(button, GWLP_USERDATA, value);
    return button;
}

static INT_PTR CALLBACK pitmod_page_proc(HWND page, UINT message,
                                         WPARAM wp, LPARAM lp) {
    if (message == WM_INITDIALOG) {
        HWND control;
        control = CreateWindowExA(0, "BUTTON", "Video",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            8, 8, 190, 105, page, NULL, dll_instance, NULL);
        set_control_font(control);
        control = CreateWindowExA(0, "STATIC", "Scaling:",
            WS_CHILD | WS_VISIBLE, 18, 31, 50, 16, page, NULL,
            dll_instance, NULL);
        set_control_font(control);
        control = CreateWindowExA(0, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            70, 27, 116, 150, page, (HMENU)IDC_PITMOD_SCALING,
            dll_instance, NULL);
        set_control_font(control);
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"Integer 4:3");
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"Integer Native (16:10)");
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"Integer 16:9");
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"Fit 4:3");
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"Fit Native (16:10)");
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"Stretch to Display");
        SendMessageA(control, CB_SETCURSEL, video_scaling_mode, 0);
        control = CreateWindowExA(0, "STATIC", "Filter:",
            WS_CHILD | WS_VISIBLE, 18, 60, 50, 16, page, NULL,
            dll_instance, NULL);
        set_control_font(control);
        control = CreateWindowExA(0, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            70, 56, 116, 120, page, (HMENU)IDC_PITMOD_FILTER,
            dll_instance, NULL);
        set_control_font(control);
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"None");
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"Scanlines");
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"CRT Mask");
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"XBR");
        SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)"FSR 1");
        SendMessageA(control, CB_SETCURSEL, video_filter, 0);
        control = CreateWindowExA(0, "STATIC",
            "Filter applies next fullscreen.",
            WS_CHILD | WS_VISIBLE, 18, 87, 165, 16, page, NULL,
            dll_instance, NULL);
        set_control_font(control);
        return TRUE;
    }
    if (message == WM_COMMAND &&
        (HIWORD(wp) == CBN_SELCHANGE || HIWORD(wp) == BN_CLICKED)) {
        PropSheet_Changed(GetParent(page), page);
        return TRUE;
    }
    if (message == WM_NOTIFY &&
        ((NMHDR *)lp)->code == PSN_APPLY) {
        char ini[MAX_PATH];
        SOUND_CHECKBOX_SCAN sound;
        LRESULT scaling = SendDlgItemMessageA(page, IDC_PITMOD_SCALING,
                                               CB_GETCURSEL, 0, 0);
        LRESULT filter = SendDlgItemMessageA(page, IDC_PITMOD_FILTER,
                                              CB_GETCURSEL, 0, 0);
        if (scaling < 0 || scaling > 5) scaling = 0;
        if (filter < 0 || filter > 4) filter = 0;
        get_pitmod_ini_path(ini);
        WritePrivateProfileStringA("Video", "Scaling",
                                   video_scaling_names[scaling], ini);
        WritePrivateProfileStringA("Video", "Filter",
                                   video_filter_names[filter], ini);
        ZeroMemory(&sound, sizeof(sound));
        EnumChildWindows(GetParent(page), capture_sound_checkbox,
                         (LPARAM)&sound);
        if (sound_pending_properties == GetParent(page) &&
            InterlockedCompareExchange(&sound_pending_valid, 0, 0)) {
            sound.found_music = TRUE;
            sound.found_ambient = TRUE;
            sound.music_checked = InterlockedCompareExchange(
                &sound_pending_music, 0, 0) != 0;
            sound.ambient_checked = InterlockedCompareExchange(
                &sound_pending_ambient, 0, 0) != 0;
        }
        if (sound.found_music && sound.found_ambient) {
            InterlockedExchange(&preferred_music_enabled,
                                sound.music_checked);
            InterlockedExchange(&preferred_ambient_enabled,
                                sound.ambient_checked);
            InterlockedExchange(&preferred_music_loaded, 1);
            InterlockedExchange(&preferred_ambient_loaded, 1);
            write_game_dword("Option_CDMusic", sound.music_checked);
            write_game_dword("Option_CDAmbient", sound.ambient_checked);
        }
        sound_pending_properties = NULL;
        InterlockedExchange(&sound_pending_valid, 0);
        initialize_video_options();
        SetWindowLongPtrA(page, DWLP_MSGRESULT, PSNRET_NOERROR);
        return TRUE;
    }
    if (message == WM_NOTIFY && ((NMHDR *)lp)->code == PSN_RESET) {
        if (sound_pending_properties == GetParent(page)) {
            sound_pending_properties = NULL;
            InterlockedExchange(&sound_pending_valid, 0);
        }
        return TRUE;
    }
    return FALSE;
}

static INT_PTR CALLBACK pitmod_input_page_proc(HWND page, UINT message,
                                               WPARAM wp, LPARAM lp) {
    static int capture_id;
    static DWORD previous_buttons;
    static DWORD capture_started;
    static const int ids[] = {
        IDC_PITMOD_PAUSE_BUTTON, IDC_PITMOD_FULLSCREEN_BUTTON,
        IDC_PITMOD_MUSIC_BUTTON, IDC_PITMOD_AMBIENT_BUTTON,
        IDC_PITMOD_VIDEO_PREVIOUS_BUTTON, IDC_PITMOD_VIDEO_NEXT_BUTTON,
        IDC_PITMOD_FILTER_PREVIOUS_BUTTON, IDC_PITMOD_FILTER_NEXT_BUTTON,
        IDC_PITMOD_BONUS1_BUTTON, IDC_PITMOD_BONUS2_BUTTON,
        IDC_PITMOD_BONUS3_BUTTON
    };
    if (message == WM_INITDIALOG) {
        HWND control;
        control = CreateWindowExA(0, "BUTTON", "Controller Shortcuts",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            8, 8, 387, 152, page, NULL, dll_instance, NULL);
        set_control_font(control);
#define ADD_INPUT_LABEL(text, x, y) do { \
            control = CreateWindowExA(0, "STATIC", text, WS_CHILD | WS_VISIBLE, \
                x, y, 72, 16, page, NULL, dll_instance, NULL); \
            set_control_font(control); \
        } while (0)
        ADD_INPUT_LABEL("Pause:", 18, 29);
        create_binding_button(page, IDC_PITMOD_PAUSE_BUTTON,
                              76, 25, xinput_pause_button);
        ADD_INPUT_LABEL("Fullscreen:", 202, 29);
        create_binding_button(page, IDC_PITMOD_FULLSCREEN_BUTTON,
                              272, 25, xinput_fullscreen_button);
        ADD_INPUT_LABEL("Music:", 18, 51);
        create_binding_button(page, IDC_PITMOD_MUSIC_BUTTON,
                              76, 47, xinput_music_button);
        ADD_INPUT_LABEL("Ambience:", 202, 51);
        create_binding_button(page, IDC_PITMOD_AMBIENT_BUTTON,
                              272, 47, xinput_ambient_button);
        ADD_INPUT_LABEL("Prev video:", 18, 73);
        create_binding_button(page, IDC_PITMOD_VIDEO_PREVIOUS_BUTTON,
                              76, 69, xinput_video_previous_button);
        ADD_INPUT_LABEL("Next video:", 202, 73);
        create_binding_button(page, IDC_PITMOD_VIDEO_NEXT_BUTTON,
                              272, 69, xinput_video_next_button);
        ADD_INPUT_LABEL("Prev filter:", 18, 95);
        create_binding_button(page, IDC_PITMOD_FILTER_PREVIOUS_BUTTON,
                              76, 91, xinput_filter_previous_button);
        ADD_INPUT_LABEL("Next filter:", 202, 95);
        create_binding_button(page, IDC_PITMOD_FILTER_NEXT_BUTTON,
                              272, 91, xinput_filter_next_button);
        ADD_INPUT_LABEL("Bonus 1:", 18, 117);
        create_binding_button(page, IDC_PITMOD_BONUS1_BUTTON,
                              76, 113, xinput_bonus_buttons[0]);
        ADD_INPUT_LABEL("Bonus 2:", 202, 117);
        create_binding_button(page, IDC_PITMOD_BONUS2_BUTTON,
                              272, 113, xinput_bonus_buttons[1]);
        ADD_INPUT_LABEL("Bonus 3:", 18, 139);
        create_binding_button(page, IDC_PITMOD_BONUS3_BUTTON,
                              76, 135, xinput_bonus_buttons[2]);
        control = CreateWindowExA(0, "BUTTON", "Clear All",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            272, 135, 112, 20, page, (HMENU)IDC_PITMOD_CLEAR_BUTTONS,
            dll_instance, NULL);
        set_control_font(control);
#undef ADD_INPUT_LABEL
        control = CreateWindowExA(0, "BUTTON", "Enable controller rumble",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            18, 166, 165, 18, page, (HMENU)IDC_PITMOD_RUMBLE,
            dll_instance, NULL);
        set_control_font(control);
        SendMessageA(control, BM_SETCHECK,
                     xinput_rumble_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
        control = CreateWindowExA(0, "STATIC",
            "Wait 5 seconds to unassign.", WS_CHILD | WS_VISIBLE,
            205, 168, 180, 16, page, NULL, dll_instance, NULL);
        set_control_font(control);
        SetTimer(page, 1, 30, NULL);
        return TRUE;
    }
    if (message == WM_COMMAND && HIWORD(wp) == BN_CLICKED) {
        int id = LOWORD(wp), index;
        if (id == IDC_PITMOD_RUMBLE) {
            PropSheet_Changed(GetParent(page), page);
            return TRUE;
        }
        if (id == IDC_PITMOD_CLEAR_BUTTONS) {
            capture_id = 0;
            InterlockedExchange(&xinput_binding_active, 0);
            for (index = 0; index < (int)(sizeof(ids) / sizeof(ids[0])); ++index) {
                HWND button = GetDlgItem(page, ids[index]);
                SetWindowLongPtrA(button, GWLP_USERDATA, 0);
                SetWindowTextA(button, xinput_button_name(0));
            }
            PropSheet_Changed(GetParent(page), page);
            return TRUE;
        }
        for (index = 0; index < (int)(sizeof(ids) / sizeof(ids[0])); ++index) {
            if (id == ids[index]) {
                XINPUT_STATE state;
                ZeroMemory(&state, sizeof(state));
                previous_buttons = xinput_get_state &&
                    xinput_get_state(0, &state) == ERROR_SUCCESS
                    ? xinput_buttons(&state.Gamepad) : 0;
                if (capture_id && capture_id != id)
                    SetWindowTextA(GetDlgItem(page, capture_id),
                        xinput_button_name((int)GetWindowLongPtrA(
                            GetDlgItem(page, capture_id), GWLP_USERDATA)));
                capture_id = id;
                capture_started = GetTickCount();
                SetWindowTextA(GetDlgItem(page, id), "Press controller input...");
                InterlockedExchange(&xinput_binding_active, 1);
                return TRUE;
            }
        }
    }
    if (message == WM_TIMER && wp == 1 && xinput_get_state) {
        XINPUT_STATE state;
        if (capture_id && GetTickCount() - capture_started >= 5000) {
            HWND button = GetDlgItem(page, capture_id);
            SetWindowLongPtrA(button, GWLP_USERDATA, 0);
            SetWindowTextA(button, xinput_button_name(0));
            capture_id = 0;
            InterlockedExchange(&xinput_binding_active, 0);
            PropSheet_Changed(GetParent(page), page);
            return TRUE;
        }
        ZeroMemory(&state, sizeof(state));
        if (xinput_get_state(0, &state) == ERROR_SUCCESS) {
            DWORD buttons = xinput_buttons(&state.Gamepad);
            DWORD pressed = capture_id ? buttons & ~previous_buttons : 0;
            if (capture_id && pressed) {
                int value;
                for (value = 1; value <= 12; ++value)
                    if (pressed & (1u << (value - 1))) break;
                SetWindowLongPtrA(GetDlgItem(page, capture_id),
                                  GWLP_USERDATA, value);
                SetWindowTextA(GetDlgItem(page, capture_id),
                               xinput_button_name(value));
                capture_id = 0;
                PropSheet_Changed(GetParent(page), page);
            } else if (!capture_id && !buttons) {
                InterlockedExchange(&xinput_binding_active, 0);
            }
            previous_buttons = buttons;
        }
        return TRUE;
    }
    if (message == WM_NOTIFY && ((NMHDR *)lp)->code == PSN_APPLY) {
        static const char *keys[] = {
            "PauseButton", "FullscreenButton", "MusicButton",
            "AmbientButton", "VideoPreviousButton", "VideoNextButton",
            "FilterPreviousButton", "FilterNextButton", "BonusKey1Button",
            "BonusKey2Button", "BonusKey3Button"
        };
        int *values[] = {
            &xinput_pause_button, &xinput_fullscreen_button,
            &xinput_music_button, &xinput_ambient_button,
            &xinput_video_previous_button, &xinput_video_next_button,
            &xinput_filter_previous_button, &xinput_filter_next_button,
            &xinput_bonus_buttons[0], &xinput_bonus_buttons[1],
            &xinput_bonus_buttons[2]
        };
        char ini[MAX_PATH];
        int index;
        get_pitmod_ini_path(ini);
        WritePrivateProfileStringA("XInput", "Rumble",
            IsDlgButtonChecked(page, IDC_PITMOD_RUMBLE) == BST_CHECKED
                ? "1" : "0", ini);
        xinput_rumble_enabled =
            IsDlgButtonChecked(page, IDC_PITMOD_RUMBLE) == BST_CHECKED;
        for (index = 0; index < (int)(sizeof(ids) / sizeof(ids[0])); ++index) {
            char number[8];
            LONG_PTR selected = GetWindowLongPtrA(
                GetDlgItem(page, ids[index]), GWLP_USERDATA);
            *values[index] = (int)selected;
            wsprintfA(number, "%ld", selected);
            WritePrivateProfileStringA("XInput", keys[index], number, ini);
        }
        capture_id = 0;
        InterlockedExchange(&xinput_binding_active, 0);
        SetWindowLongPtrA(page, DWLP_MSGRESULT, PSNRET_NOERROR);
        return TRUE;
    }
    if (message == WM_NOTIFY && ((NMHDR *)lp)->code == PSN_KILLACTIVE) {
        if (capture_id) {
            HWND button = GetDlgItem(page, capture_id);
            SetWindowTextA(button, xinput_button_name(
                (int)GetWindowLongPtrA(button, GWLP_USERDATA)));
            capture_id = 0;
        }
        InterlockedExchange(&xinput_binding_active, 0);
        SetWindowLongPtrA(page, DWLP_MSGRESULT, FALSE);
        return TRUE;
    }
    if (message == WM_DESTROY) {
        KillTimer(page, 1);
        capture_id = 0;
        InterlockedExchange(&xinput_binding_active, 0);
    }
    return FALSE;
}

static BOOL add_pitmod_page(HWND properties) {
    struct {
        DLGTEMPLATE dialog;
        WORD menu;
        WORD window_class;
        WORD title;
    } template_data;
    PROPSHEETPAGEA page;
    HPROPSHEETPAGE handle;
    ZeroMemory(&template_data, sizeof(template_data));
    template_data.dialog.style = WS_CHILD | WS_CAPTION | DS_CONTROL;
    template_data.dialog.dwExtendedStyle = 0;
    template_data.dialog.cdit = 0;
    template_data.dialog.x = 0;
    template_data.dialog.y = 0;
    template_data.dialog.cx = 405;
    template_data.dialog.cy = 190;
    ZeroMemory(&page, sizeof(page));
    page.dwSize = sizeof(page);
    page.dwFlags = PSP_DLGINDIRECT | PSP_USETITLE;
    page.hInstance = dll_instance;
    page.pResource = (LPCDLGTEMPLATE)&template_data;
    page.pszTitle = "PitMod Video";
    page.pfnDlgProc = pitmod_page_proc;
    handle = CreatePropertySheetPageA(&page);
    if (!handle) return FALSE;
    if (!SendMessageA(properties, PSM_ADDPAGE, 0, (LPARAM)handle)) {
        DestroyPropertySheetPage(handle);
        return FALSE;
    }
    page.pszTitle = "PitMod Input";
    page.pfnDlgProc = pitmod_input_page_proc;
    handle = CreatePropertySheetPageA(&page);
    if (!handle) return FALSE;
    if (!SendMessageA(properties, PSM_ADDPAGE, 0, (LPARAM)handle)) {
        DestroyPropertySheetPage(handle);
        return FALSE;
    }
    return TRUE;
}

static DWORD WINAPI watch_hotkey(void *unused) {
    MSG message;
    UINT_PTR xinput_timer;
    HWND restored_properties = NULL;
    HWND pitmod_properties = NULL;
    UINT preference_track = (UINT)-1;
    int preference_mode = -1;
    (void)unused;
    /* MOD_NOREPEAT is 0x4000 and is defined only by newer SDK headers. */
    if (!RegisterHotKey(NULL, 1, 0x4000, VK_F11)) {
        write_log("F11 registration failed");
        return 1;
    }
    keyboard_hook = SetWindowsHookExA(WH_KEYBOARD_LL, low_level_keyboard,
                                      dll_instance, 0);
    if (keyboard_hook)
        write_log("low-level keyboard capture active");
    else
        write_log("low-level keyboard capture failed");
    write_log("F11 control registered");
    /* Poll shortcuts independently of the game's legacy joystick capture.
       The Simon bonus game releases/stops polling that API entirely. */
    xinput_timer = SetTimer(NULL, 0, 16, NULL);
    while (InterlockedCompareExchange(&control_running, 1, 1) &&
           GetMessageA(&message, NULL, 0, 0) > 0) {
        if (message.message == WM_HOTKEY && message.wParam == 1) {
            write_log("F11 pressed");
            if (InterlockedCompareExchange(&fullscreen_active, 0, 0))
                GAME_LEAVE_FULLSCREEN();
            else {
                *GAME_PACKED_MODE_FLAG = 0;
                GAME_ENTER_FULLSCREEN();
            }
        } else if (message.message == WM_PITMOD_TOGGLE_FULLSCREEN) {
            write_log("controller fullscreen pressed");
            if (InterlockedCompareExchange(&fullscreen_active, 0, 0))
                GAME_LEAVE_FULLSCREEN();
            else {
                *GAME_PACKED_MODE_FLAG = 0;
                GAME_ENTER_FULLSCREEN();
            }
        } else if (message.message == WM_PITMOD_VIDEO_PREVIOUS ||
                   message.message == WM_PITMOD_VIDEO_NEXT) {
            if (InterlockedCompareExchange(&fullscreen_active, 0, 0) &&
                d3d_device) {
                char ini[MAX_PATH], event[96];
                int direction = message.message == WM_PITMOD_VIDEO_NEXT
                    ? 1 : -1;
                int mode = (video_scaling_mode + direction + 6) % 6;
                set_video_scaling_mode(mode);
                get_pitmod_ini_path(ini);
                WritePrivateProfileStringA("Video", "Scaling",
                                           video_scaling_names[mode], ini);
                wsprintfA(event, "controller video mode %s",
                          video_scaling_names[mode]);
                write_log(event);
            }
        } else if (message.message == WM_PITMOD_FILTER_PREVIOUS ||
                   message.message == WM_PITMOD_FILTER_NEXT) {
            if (InterlockedCompareExchange(&fullscreen_active, 0, 0) &&
                d3d_device) {
                char ini[MAX_PATH], event[96];
                int direction = message.message == WM_PITMOD_FILTER_NEXT
                    ? 1 : -1;
                int mode = (video_filter + direction + 5) % 5;
                if (set_video_filter_mode_live(mode)) {
                    get_pitmod_ini_path(ini);
                    WritePrivateProfileStringA("Video", "Filter",
                                               video_filter_names[mode], ini);
                    wsprintfA(event, "controller video filter %s",
                              video_filter_names[mode]);
                    write_log(event);
                }
            }
        } else if (message.message == WM_PITMOD_SELECT_MUSIC) {
            if (!game_window || !IsWindow(game_window))
                game_window = FindWindowA(NULL, "Pitfall: The Mayan Adventure");
            if (*GAME_MUSIC_ENABLED) {
                *GAME_MUSIC_ENABLED = 0;
                InterlockedExchange(&preferred_music_enabled, 0);
                GAME_PLAY_CD_TRACK(game_window, 0);
                force_close_virtual_music();
                write_log("controller disabled CD music");
            } else {
                UINT track = (UINT)*GAME_MUSIC_TRACK;
                *GAME_AMBIENT_ENABLED = 0;
                *GAME_MUSIC_ENABLED = 1;
                InterlockedExchange(&preferred_ambient_enabled, 0);
                InterlockedExchange(&preferred_music_enabled, 1);
                /* Intro music and ambience can point at the same file. Only
                   change the logical selection in that case; do not rewind. */
                if (current_music_track != track)
                    GAME_PLAY_CD_TRACK(game_window, track);
                write_log("controller enabled CD music");
            }
            InterlockedExchange(&preferred_music_loaded, 1);
            InterlockedExchange(&preferred_ambient_loaded, 1);
            write_game_dword("Option_CDMusic", preferred_music_enabled);
            write_game_dword("Option_CDAmbient", preferred_ambient_enabled);
            preference_mode = -1;
            restored_properties = NULL;
        } else if (message.message == WM_PITMOD_SELECT_AMBIENT) {
            if (!game_window || !IsWindow(game_window))
                game_window = FindWindowA(NULL, "Pitfall: The Mayan Adventure");
            if (*GAME_AMBIENT_ENABLED) {
                *GAME_AMBIENT_ENABLED = 0;
                InterlockedExchange(&preferred_ambient_enabled, 0);
                GAME_PLAY_CD_TRACK(game_window, 0);
                force_close_virtual_music();
                write_log("controller disabled CD ambience");
            } else {
                UINT track = (UINT)*GAME_AMBIENT_TRACK;
                *GAME_MUSIC_ENABLED = 0;
                *GAME_AMBIENT_ENABLED = 1;
                InterlockedExchange(&preferred_music_enabled, 0);
                InterlockedExchange(&preferred_ambient_enabled, 1);
                if (current_music_track != track)
                    GAME_PLAY_CD_TRACK(game_window, track);
                write_log("controller enabled CD ambience");
            }
            InterlockedExchange(&preferred_music_loaded, 1);
            InterlockedExchange(&preferred_ambient_loaded, 1);
            write_game_dword("Option_CDMusic", preferred_music_enabled);
            write_game_dword("Option_CDAmbient", preferred_ambient_enabled);
            preference_mode = -1;
            restored_properties = NULL;
        } else if (message.message == WM_TIMER &&
                   message.wParam == xinput_timer && xinput_get_state) {
            XINPUT_STATE state;
            BOOL connected;
            HWND properties = FindWindowA(NULL, "Pitfall Properties");
            if (properties && properties != pitmod_properties) {
                if (add_pitmod_page(properties))
                    pitmod_properties = properties;
            } else if (!properties) {
                pitmod_properties = NULL;
            }
            if (properties && properties != restored_properties) {
                SOUND_CHECKBOX_SCAN scan;
                ZeroMemory(&scan, sizeof(scan));
                EnumChildWindows(properties, restore_sound_checkbox,
                                 (LPARAM)&scan);
                if (scan.found_music && scan.found_ambient) {
                    restored_properties = properties;
                    sound_pending_properties = properties;
                    InterlockedExchange(&sound_pending_music,
                        preferred_music_enabled);
                    InterlockedExchange(&sound_pending_ambient,
                        preferred_ambient_enabled);
                    InterlockedExchange(&sound_pending_valid, 1);
                }
            } else if (properties && properties == restored_properties) {
                SOUND_CHECKBOX_SCAN scan;
                ZeroMemory(&scan, sizeof(scan));
                EnumChildWindows(properties, capture_sound_checkbox,
                                 (LPARAM)&scan);
                if (scan.found_music && scan.found_ambient) {
                    InterlockedExchange(&sound_pending_music,
                                        scan.music_checked);
                    InterlockedExchange(&sound_pending_ambient,
                                        scan.ambient_checked);
                    InterlockedExchange(&sound_pending_valid, 1);
                }
            } else if (!properties) {
                restored_properties = NULL;
            }
            if (!properties && preferred_music_loaded &&
                preferred_ambient_loaded) {
                int mode = preferred_music_enabled ? 1 :
                           preferred_ambient_enabled ? 2 : 0;
                UINT track = mode == 1 ? (UINT)*GAME_MUSIC_TRACK :
                             mode == 2 ? (UINT)*GAME_AMBIENT_TRACK : 0;
                if (mode != preference_mode || track != preference_track) {
                    preference_mode = mode;
                    preference_track = track;
                    if (mode == 1) {
                        *GAME_AMBIENT_ENABLED = 0;
                        *GAME_MUSIC_ENABLED = 1;
                    } else if (mode == 2) {
                        *GAME_MUSIC_ENABLED = 0;
                        *GAME_AMBIENT_ENABLED = 1;
                    }
                    if (mode && track >= 2 && track <= 99 &&
                        current_music_track != track) {
                        if (!game_window || !IsWindow(game_window))
                            game_window = FindWindowA(
                                NULL, "Pitfall: The Mayan Adventure");
                        if (game_window)
                            GAME_PLAY_CD_TRACK(game_window, track);
                    } else if (!mode && current_music_track) {
                        if (game_window)
                            GAME_PLAY_CD_TRACK(game_window, 0);
                        force_close_virtual_music();
                    }
                }
            }
            ZeroMemory(&state, sizeof(state));
            connected = xinput_get_state(0, &state) == ERROR_SUCCESS;
            if (connected)
                update_xinput_shortcuts(xinput_buttons(&state.Gamepad));
            else
                update_xinput_shortcuts(0);
            if (xinput_hotplug && connected && !xinput_connected) {
                xinput_connected = TRUE;
                *GAME_JOYSTICK_AVAILABLE = 1;
                if (xinput_auto_enable) *GAME_JOYSTICK_ENABLED = 1;
                if (game_window)
                    compatible_joy_set_capture(game_window, 0, 17, FALSE);
            } else if (xinput_hotplug && !connected && xinput_connected) {
                xinput_connected = FALSE;
                *GAME_JOYSTICK_AVAILABLE = 0;
                *GAME_JOYSTICK_ENABLED = 0;
                compatible_joy_release_capture(0);
            }
            update_xinput_rumble();
        }
    }
    if (keyboard_hook) {
        UnhookWindowsHookEx(keyboard_hook);
        keyboard_hook = NULL;
    }
    UnregisterHotKey(NULL, 1);
    KillTimer(NULL, xinput_timer);
    if (xinput_set_state) {
        XINPUT_VIBRATION vibration;
        ZeroMemory(&vibration, sizeof(vibration));
        xinput_set_state(0, &vibration);
    }
    return 0;
}

static LRESULT CALLBACK display_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    display_window = hwnd;
    switch (message) {
    case WM_COPYDATA:
        return TRUE;
    case 0x403:
        enter_fullscreen();
        return TRUE;
    case 0x404:
        leave_fullscreen();
        return TRUE;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp < 256) fullscreen_key_down[wp] = 1;
        if (game_window && IsWindow(game_window))
            PostMessageA(game_window, message, wp, lp);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wp < 256) fullscreen_key_down[wp] = 0;
        if (game_window && IsWindow(game_window))
            PostMessageA(game_window, message, wp, lp);
        return 0;
    case WM_CHAR:
        /* Exclusive Direct3D must keep this display window activated.  Relay
           its keyboard input to Pitfall instead of transferring focus and
           thereby causing Windows to tear down exclusive mode. */
        if (game_window && IsWindow(game_window))
            PostMessageA(game_window, message, wp, lp);
        return 0;
    case WM_DESTROY:
        if (hwnd == display_window) display_window = NULL;
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    default:
        return DefWindowProcA(hwnd, message, wp, lp);
    }
}

static BOOL hook_game_import(const char *dll_name, const char *function_name,
                             void *replacement, void **original) {
    BYTE *base = (BYTE *)game_module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt;
    IMAGE_IMPORT_DESCRIPTOR *descriptor;
    IMAGE_DATA_DIRECTORY directory;
    void *resolved;
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;
    nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;
    directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress) return FALSE;
    resolved = (void *)GetProcAddress(GetModuleHandleA(dll_name), function_name);
    descriptor = (IMAGE_IMPORT_DESCRIPTOR *)(base + directory.VirtualAddress);
    for (; descriptor->Name; ++descriptor) {
        IMAGE_THUNK_DATA *iat, *names;
        unsigned index;
        if (lstrcmpiA((char *)(base + descriptor->Name), dll_name)) continue;
        iat = (IMAGE_THUNK_DATA *)(base + descriptor->FirstThunk);
        names = descriptor->OriginalFirstThunk
            ? (IMAGE_THUNK_DATA *)(base + descriptor->OriginalFirstThunk) : NULL;
        for (index = 0; iat[index].u1.Function; ++index) {
            BOOL match = FALSE;
            DWORD old_protect, ignored;
            if (names && !(names[index].u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                IMAGE_IMPORT_BY_NAME *name = (IMAGE_IMPORT_BY_NAME *)
                    (base + names[index].u1.AddressOfData);
                match = !lstrcmpA((char *)name->Name, function_name);
            } else if (resolved && (void *)iat[index].u1.Function == resolved) {
                match = TRUE;
            }
            if (!match) continue;
            if (!VirtualProtect(&iat[index].u1.Function, sizeof(void *),
                                PAGE_READWRITE, &old_protect)) return FALSE;
            if (original) *original = (void *)iat[index].u1.Function;
            iat[index].u1.Function = (ULONG_PTR)replacement;
            VirtualProtect(&iat[index].u1.Function, sizeof(void *), old_protect,
                           &ignored);
            FlushInstructionCache(GetCurrentProcess(), &iat[index].u1.Function,
                                  sizeof(void *));
            return TRUE;
        }
    }
    return FALSE;
}

static DWORD WINAPI compatible_get_version(void) {
    ULONG_PTR delta = (ULONG_PTR)game_module - 0x400000u;
    ULONG_PTR caller = (ULONG_PTR)__builtin_return_address(0) - delta;
    if (caller == 0x43C2CDu || caller == 0x43C2D6u) {
        write_log("reported Windows 95 to fullscreen capability check");
        return 0xC3B60004u;
    }
    return original_get_version();
}

static DWORD WINAPI compatible_load_module(LPCSTR path, LPVOID parameters) {
    const char *name = path, *cursor;
    if (path) {
        for (cursor = path; *cursor; ++cursor)
            if (*cursor == '\\' || *cursor == '/') name = cursor + 1;
        if (!lstrcmpiA(name, "DISPDIB.DLL")) {
            write_log("emulated DISPDIB LoadModule request");
            return 0x1000;
        }
    }
    return original_load_module(path, parameters);
}

static int WINAPI compatible_message_box(HWND owner, LPCSTR text,
                                         LPCSTR caption, UINT type) {
    if (text && strstr(text, "256 color")) {
        write_log("suppressed obsolete 256-color warning");
        return IDOK;
    }
    return original_message_box(owner, text, caption, type);
}

static BOOL patch_game_bytes(DWORD va, const BYTE *expected,
                             const BYTE *replacement, SIZE_T size,
                             const char *description) {
    BYTE *address = (BYTE *)game_module + (va - 0x400000u);
    DWORD old_protect, ignored;
    if (memcmp(address, expected, size)) {
        char message[160];
        wsprintfA(message, "runtime patch mismatch at %08lX: %s", va,
                  description);
        write_log(message);
        return FALSE;
    }
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_protect))
        return FALSE;
    CopyMemory(address, replacement, size);
    VirtualProtect(address, size, old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), address, size);
    return TRUE;
}

#define PATCH_BYTES(va, old_bytes, new_bytes, label) \
    if (!patch_game_bytes((va), (const BYTE *)(old_bytes), \
                          (const BYTE *)(new_bytes), sizeof(old_bytes) - 1, \
                          (label))) return FALSE

static BOOL relocate_game_framebuffer(void) {
    static const struct { DWORD va, original; } references[] = {
        { 0x43DC8F, 0x000A0000 }, { 0x437CAA, 0x000AF8C0 },
        { 0x437D00, 0x000A0280 }, { 0x437D19, 0x000A0280 },
        { 0x437D32, 0x000A0280 }, { 0x437D4B, 0x000A0280 }
    };
    unsigned index;
    for (index = 0; index < sizeof(references) / sizeof(references[0]); ++index) {
        BYTE *instruction = (BYTE *)game_module + references[index].va - 0x400000u;
        if (instruction[0] != 0xBF ||
            *(DWORD *)(instruction + 1) != references[index].original) {
            write_log("framebuffer relocation byte check failed");
            return FALSE;
        }
    }
    framebuffer = (BYTE *)VirtualAlloc(NULL, 0x10000, MEM_RESERVE | MEM_COMMIT,
                                       PAGE_READWRITE);
    if (!framebuffer) return FALSE;
    for (index = 0; index < sizeof(references) / sizeof(references[0]); ++index) {
        BYTE *operand = (BYTE *)game_module + references[index].va - 0x400000u + 1;
        DWORD old_protect, ignored;
        DWORD target = (DWORD)(ULONG_PTR)
            (framebuffer + references[index].original - 0x000A0000u);
        if (!VirtualProtect(operand, sizeof(target), PAGE_EXECUTE_READWRITE,
                            &old_protect)) return FALSE;
        CopyMemory(operand, &target, sizeof(target));
        VirtualProtect(operand, sizeof(target), old_protect, &ignored);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    return TRUE;
}

static void (__cdecl *vblank_function_slot)(void) = emulate_vblank;

static BOOL install_vblank_runtime_hook(void) {
    static const BYTE expected[] = {
        0x52,0x50,0xBA,0xDA,0x03,0x00,0x00,0xEC,0x24,0x08,
        0x75,0xFB,0xEC,0x24,0x08,0x74,0xFB,0x58,0x5A,0xC3
    };
    BYTE replacement[sizeof(expected)];
    DWORD slot = (DWORD)(ULONG_PTR)&vblank_function_slot;
    ZeroMemory(replacement, sizeof(replacement));
    replacement[0] = 0x9C; /* pushfd */
    replacement[1] = 0x60; /* pushad */
    replacement[2] = 0xFF;
    replacement[3] = 0x15; /* call dword ptr [absolute address] */
    CopyMemory(replacement + 4, &slot, sizeof(slot));
    replacement[8] = 0x61; /* popad */
    replacement[9] = 0x9D; /* popfd */
    replacement[10] = 0xC3;
    memset(replacement + 11, 0x90, sizeof(replacement) - 11);
    return patch_game_bytes(0x435F56, expected, replacement,
                            sizeof(expected), "vertical retrace hook");
}

static BOOL install_runtime_compatibility(void) {
    if (!relocate_game_framebuffer()) return FALSE;
    if (!install_vblank_runtime_hook()) return FALSE;

    /* These are the compatibility-only branches from the former executable
       patch. They are changed in memory after exact retail-byte checks. */
    PATCH_BYTES(0x43D9A0, "\x75\x0C", "\xEB\x0C", "native display path");
    PATCH_BYTES(0x4470AE, "\x75\x05", "\xEB\x05", "CD shutdown check 1");
    PATCH_BYTES(0x443E99, "\x75\x05", "\xEB\x05", "CD shutdown check 2");
    PATCH_BYTES(0x440460, "\x74", "\xEB", "sound setting persistence");
    PATCH_BYTES(0x441A95, "\x74", "\xEB", "sound-effects control");
    PATCH_BYTES(0x43CE1F, "\x75\x0A", "\xEB\x0A", "sound initialization");
    PATCH_BYTES(0x43CC1A, "\x74\x45", "\xEB\x45", "replacement WAIL path");
    PATCH_BYTES(0x43DC07, "\x8B\x0D\x58\x30\x45\x00\x85\xC9\x0F\x84\xB0\x00\x00\x00",
                "\xE9\xFC\x00\x00\x00\x90\x90\x90\x90\x90\x90\x90\x90\x90",
                "VGA register programming");
    PATCH_BYTES(0x43DD0D, "\xBA\xC6\x03\x00\x00", "\xE9\x3E\x00\x00\x00",
                "VGA palette upload");
    PATCH_BYTES(0x43DD6D, "\xE8\xAE\xEA\xFF\xFF", "\x90\x90\x90\x90\x90",
                "replacement display lifetime");
    PATCH_BYTES(0x43DE30, "\xBA", "\xC3", "VGA palette routine");
    PATCH_BYTES(0x43E957, "\x74\x47", "\xEB\x6A", "runtime palette upload");
    PATCH_BYTES(0x43E66A, "\x74\x4C", "\xEB\x74", "fading palette upload");
    PATCH_BYTES(0x43EA82, "\x74\x4C", "\xEB\x74", "alternate palette upload");
    PATCH_BYTES(0x4411AA, "\x01", "\x00", "packed framebuffer selection");
    PATCH_BYTES(0x437CA2, "\x75\x34", "\x90\x90", "packed renderer");

    if (!hook_game_import("KERNEL32.dll", "GetVersion",
                          compatible_get_version, (void **)&original_get_version))
        return FALSE;
    if (!hook_game_import("KERNEL32.dll", "LoadModuleA",
                          compatible_load_module, (void **)&original_load_module) &&
        !hook_game_import("KERNEL32.dll", "LoadModule",
                          compatible_load_module, (void **)&original_load_module))
        return FALSE;
    if (!hook_game_import("USER32.dll", "MessageBoxA",
                          compatible_message_box, (void **)&original_message_box))
        return FALSE;
    return TRUE;
}

#undef PATCH_BYTES

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        WNDCLASSA wc;
        dll_instance = instance;
        game_module = GetModuleHandleA(NULL);
        write_log("DLL-only process attach started");
        DisableThreadLibraryCalls(instance);
        InitializeCriticalSection(&d3d_lock);
        InitializeCriticalSection(&music_lock);
        /* The original game loop is paced with millisecond multimedia timers.
           Modern Windows otherwise permits coarser wakeups and can leave an
           old single-threaded game unscheduled for two or three refreshes. */
        timeBeginPeriod(1);
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        if (!install_runtime_compatibility()) {
            write_log("DLL-only runtime compatibility initialization failed");
            return FALSE;
        }
        write_log("DLL-only runtime compatibility initialized");
        if (!install_focus_hook()) { write_log("focus hook failed"); return FALSE; }
        if (!install_xinput_hook()) { write_log("XInput hook failed"); return FALSE; }
        if (!install_local_settings_hook()) { write_log("settings hook failed"); return FALSE; }
        initialize_video_options();
        initialize_folder_music();
        if (!install_music_hook()) { write_log("music hook failed"); return FALSE; }
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = display_proc;
        /* The original DisplayDib server exposes a process-wide class. Pitfall
           passes its EXE instance when creating the window, so this must be a
           global class rather than a DLL-private class. */
        wc.style = CS_GLOBALCLASS;
        wc.hInstance = instance;
        wc.lpszClassName = "DisplayDibWindow";
        if (!RegisterClassA(&wc)) { write_log("DisplayDib class registration failed"); return FALSE; }
        InterlockedExchange(&control_running, 1);
        control_thread = CreateThread(NULL, 0, watch_hotkey, NULL, 0,
                                      &control_thread_id);
        if (!control_thread) { write_log("control thread creation failed"); return FALSE; }
        write_log("DLL-only process attach completed");
    } else if (reason == DLL_PROCESS_DETACH) {
        InterlockedExchange(&control_running, 0);
        InterlockedExchange(&presenting, 0);
        InterlockedExchange(&game_timer_running, 0);
        InterlockedExchange(&joystick_running, 0);
        timeEndPeriod(1);
    }
    return TRUE;
}
