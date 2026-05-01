#include "core.h"
#include "hijack.h"
#include "ftp.h"
#include "tables.h"
#include "doomgeneric.h"

// ------------------------------------------------------------
//  Globals – shared with the platform implementation files
// ------------------------------------------------------------
void *G = NULL;
void *D = NULL;
struct video_ctx v;
s32 pad_h = -1;
void *pad_read_fn = NULL;
void *usleep_fn = NULL;
s32 log_fd = -1;
u8 log_sa[16];
s32 audio_h = -1;
void *aud_out_fn = NULL;
void *aud_close_fn = NULL;

// Forward declaration for the FTP waiting phase
static void draw_centered_text_scaled(u32 *fb, const char *s, int y, u8 color, int scale);

// ------------------------------------------------------------
//  UDP log helper
// ------------------------------------------------------------
static void udp_log(const char *msg) {
    if (log_fd < 0) return;
    void *sendto = SYM(G, D, LIBKERNEL_HANDLE, "sendto");
    if (!sendto) return;
    int len = 0;
    while (msg[len]) len++;
    NC(G, sendto, (u64)log_fd, (u64)msg, (u64)len, 0, (u64)log_sa, 16);
}

// ------------------------------------------------------------
//  Minimal framebuffer text drawing for the FTP screen
// ------------------------------------------------------------
static void draw_char_scaled(u32 *fb, int x, int y, char ch, u32 color, int scale) {
    if (ch < 32 || ch > 90) ch = '?';
    int idx = ch - 32;
    const u8 *glyph = font_data[idx];
    for (int r = 0; r < 8; r++) {
        u8 bits = glyph[r];
        for (int c = 0; c < 8; c++) {
            if (bits & (0x80 >> c)) {
                int base_x = x + c * scale;
                int base_y = y + r * scale;
                for (int dy = 0; dy < scale; dy++) {
                    u32 *row = &fb[(base_y + dy) * SCR_W + base_x];
                    for (int dx = 0; dx < scale; dx++)
                        row[dx] = color;
                }
            }
        }
    }
}

static void draw_str_scaled(u32 *fb, int x, int y, const char *s, u32 color, int scale) {
    while (*s) {
        draw_char_scaled(fb, x, y, *s, color, scale);
        x += 8 * scale;
        s++;
    }
}

static void draw_centered_text_scaled(u32 *fb, const char *s, int y, u32 color, int scale) {
    int len = 0;
    while (s[len]) len++;
    int w = len * 8 * scale;
    int x = (SCR_W - w) / 2;
    draw_str_scaled(fb, x, y, s, color, scale);
}

// ------------------------------------------------------------
//  main
// ------------------------------------------------------------
__attribute__((section(".text._start")))
void _start(u64 eboot_base, u64 dlsym_addr, struct ext_args *ext) {
    G = (void *)(eboot_base + GADGET_OFFSET);
    D = (void *)dlsym_addr;
    log_fd = ext->log_fd;
    for (int i = 0; i < 16; i++) log_sa[i] = ext->log_addr[i];

    udp_log("Doom payload starting...\n");

    // -------- Resolve essential functions --------
    void *load_mod  = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelLoadStartModule");
    void *kopen     = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelOpen");
    void *kread     = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelRead");
    void *kwrite    = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelWrite");
    void *kclose    = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelClose");
    void *kmkdir    = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelMkdir");
    void *getdents  = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelGetdents");
    if (!getdents) getdents = SYM(G, D, LIBKERNEL_HANDLE, "getdents");
    void *recvfrom  = SYM(G, D, LIBKERNEL_HANDLE, "recvfrom");
    void *sendto    = SYM(G, D, LIBKERNEL_HANDLE, "sendto");
    void *accept    = SYM(G, D, LIBKERNEL_HANDLE, "accept");
    void *mmap      = SYM(G, D, LIBKERNEL_HANDLE, "mmap");
    void *munmap    = SYM(G, D, LIBKERNEL_HANDLE, "munmap");
    void *getsockname = SYM(G, D, LIBKERNEL_HANDLE, "getsockname");
    void *poll      = SYM(G, D, LIBKERNEL_HANDLE, "poll");
    void *sso       = SYM(G, D, LIBKERNEL_HANDLE, "setsockopt");

    usleep_fn = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelUsleep");
    if (!usleep_fn || !load_mod) {
        ext->status = -1; ext->step = 1; return;
    }

    // -------- Hijack video --------
    if (video_hijack(&v, G, D, eboot_base, ext) != 0) {
        udp_log("Video hijack failed\n");
        ext->status = -1; ext->step = 2; return;
    }

    // Clear both framebuffers
    for (int i = 0; i < SCR_W * SCR_H; i++) {
        ((u32*)v.fbs[0])[i] = 0xFF000000;
        ((u32*)v.fbs[1])[i] = 0xFF000000;
    }

    // -------- Init audio out (stereo 48kHz) --------
    s32 aud_mod = (s32)NC(G, load_mod, (u64)"libSceAudioOut.sprx", 0, 0, 0, 0, 0);
    void *aud_open = SYM(G, D, aud_mod, "sceAudioOutOpen");
    aud_out_fn = SYM(G, D, aud_mod, "sceAudioOutOutput");
    aud_close_fn = SYM(G, D, aud_mod, "sceAudioOutClose");

    if (aud_close_fn) {
        for (int h = 0; h < 8; h++) NC(G, aud_close_fn, (u64)h, 0,0,0,0,0);
    }

    if (aud_open) {
        audio_h = (s32)NC(G, aud_open, 0xFF, 0, 0, 256, 48000, 1); // S16 stereo
        if (audio_h < 0) udp_log("Audio open failed\n");
        else udp_log("Audio OK\n");
    }

    // -------- Pad init --------
    s32 pad_mod = (s32)NC(G, load_mod, (u64)"libScePad.sprx", 0,0,0,0,0);
    void *pad_init = SYM(G, D, pad_mod, "scePadInit");
    void *pad_geth = SYM(G, D, pad_mod, "scePadGetHandle");
    pad_read_fn = SYM(G, D, pad_mod, "scePadRead");

    if (pad_init) NC(G, pad_init, 0,0,0,0,0,0);
    pad_h = -1;
    if (pad_geth) pad_h = (s32)NC(G, pad_geth, (u64)ext->dbg[3], 0, 0, 0, 0, 0);

    udp_log(pad_h >= 0 ? "Pad OK\n" : "Pad N/A\n");

    // -------- FTP: wait for doom1.wad --------
    s32 ftp_srv_fd  = (s32)ext->dbg[4];   // port 1337
    s32 ftp_data_fd = (s32)ext->dbg[5];   // port 1338

    // Show FTP waiting screen
    u32 *fb = (u32*)v.fbs[0];
    for (int i = 0; i < SCR_W * SCR_H; i++) fb[i] = 0xFF000000;
    draw_centered_text_scaled(fb, "EGYDEVT EAM DOOM", 200, 0xFFFFFFFF, 4);
    draw_centered_text_scaled(fb, "FTP Server on port 1337", 280, 0xFFAAAAAA, 3);
    draw_centered_text_scaled(fb, "Send doom1.wad to /av_contents/content_tmp/", 360, 0xFFAAAAAA, 3);
    draw_centered_text_scaled(fb, "Waiting for WAD...", 440, 0xFF00FF00, 3);

    // Copy to second framebuffer
    for (int i = 0; i < SCR_W * SCR_H; i++)
        ((u32*)v.fbs[1])[i] = ((u32*)v.fbs[0])[i];
    video_flip(&v, 0);

    struct rom_entry dummy[1];
    int rom_count = ftp_serve(ftp_srv_fd, ftp_data_fd,
                              G, D, load_mod, mmap,
                              kopen, kwrite, kclose, kmkdir,
                              getdents, usleep_fn,
                              recvfrom, sendto, accept, getsockname,
                              log_fd, log_sa, ext->dbg[3],
                              dummy, 1);
    if (rom_count == 0) {
        udp_log("No WAD transferred\n");
        video_cleanup(&v);
        ext->status = 0;
        return;
    }

    udp_log("WAD received, launching Doom...\n");

    // -------- Launch Doom --------
    doomgeneric_Init();

    // Main game loop
    while (1) {
        doomgeneric_Tick();
        if (usleep_fn) NC(G, usleep_fn, 28571, 0,0,0,0,0); // ~35 fps
    }

    doomgeneric_Shutdown();
    video_cleanup(&v);
    ext->status = 0;
}
