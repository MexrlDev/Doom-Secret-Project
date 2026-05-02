#include "core.h"
#include "doomgeneric.h"
#include "tables.h"
extern void *G;
extern void *D;
extern s32 pad_h;
extern void *pad_read_fn;

#define KEYBUF_SIZE 16
static int keybuf[KEYBUF_SIZE];
static int keybuf_head = 0, keybuf_tail = 0;

// Map PS4 controller bits to Doom key constants
enum {
    KEY_RIGHT  = 0xAE,
    KEY_LEFT   = 0xAC,
    KEY_UP     = 0xAD,
    KEY_DOWN   = 0xAF,
    KEY_FIRE   = 0x9D,   // Control
    KEY_USE    = 0x20,   // Space
    KEY_STRAFE = 0xB8,   // Alt
    KEY_ESCAPE = 0x1B,
    KEY_ENTER  = 0x0D,
    KEY_TAB    = 0x09,
    KEY_TILDE  = 0x60,   // console
};

static void push_key(int k) {
    if (((keybuf_tail + 1) % KEYBUF_SIZE) != keybuf_head) {
        keybuf[keybuf_tail] = k;
        keybuf_tail = (keybuf_tail + 1) % KEYBUF_SIZE;
    }
}

static u8 ds_to_pad_nes(u32 raw) {
    // same as ds_to_nes from the NES emu, but uses it for direction bits
    u8 r = 0;
    if (raw & 0x00004000) r |= 0x01; // CROSS    -> A
    if (raw & 0x00008000) r |= 0x02; // SQUARE   -> B
    if (raw & 0x00001000) r |= 0x04; // TRIANGLE -> Sel
    if (raw & 0x00002000) r |= 0x08; // CIRCLE   -> Start
    if (raw & 0x00000008) r |= 0x08; // OPTIONS  -> Start
    if (raw & 0x00000010) r |= 0x10; // UP
    if (raw & 0x00000040) r |= 0x20; // DOWN
    if (raw & 0x00000080) r |= 0x40; // LEFT
    if (raw & 0x00000020) r |= 0x80; // RIGHT
    return r;
}

// Current pressed state (we only push on edge)
static u8 prev_state = 0;

void I_ReadKeys(void) {
    if (pad_h < 0 || !pad_read_fn) return;

    u8 buf[128] = {0};
    s32 n = (s32)NC(G, pad_read_fn, (u64)pad_h, (u64)buf, 1, 0, 0, 0);
    if (n <= 0 || (u32)n >= 0x80000000) return;

    u32 raw = *(u32*)buf;
    if (raw & 0x80000000) return;

    u8 state = ds_to_pad_nes(raw & 0x001FFFFF);

    // Detect edges (button pressed since last frame)
    u8 changed = state & ~prev_state;
    prev_state = state;

    // Map each newly pressed button to a Doom key
    if (changed & 0x10) push_key(KEY_UP);
    if (changed & 0x20) push_key(KEY_DOWN);
    if (changed & 0x40) push_key(KEY_LEFT);
    if (changed & 0x80) push_key(KEY_RIGHT);
    if (changed & 0x01) push_key(KEY_FIRE);    // A
    if (changed & 0x02) push_key(KEY_USE);     // B
    if (changed & 0x04) push_key(KEY_TAB);     // Triangle (automap)
    if (changed & 0x08) push_key(KEY_ESCAPE);  // Start (menu)
}

int I_GetKey(void) {
    if (keybuf_head == keybuf_tail) return 0;
    int k = keybuf[keybuf_head];
    keybuf_head = (keybuf_head + 1) % KEYBUF_SIZE;
    return k;
}
