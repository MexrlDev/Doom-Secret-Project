#include "core.h"
#include "hijack.h"
#include "doomgeneric.h"
#include "tables.h"

extern void *G;
extern void *D;
extern struct video_ctx v;
extern u32 *curpal;      // Doom's palette (256 ARGB colours, loaded from WAD)

static int active = 0;

void I_InitGraphics(void) {
    // Do nothing – our framebuffers are already set up by video_hijack
}

void I_ShutdownGraphics(void) {
    // nothing
}

void I_SetWindowTitle(const char *title) {
    (void)title;
}

// ------------------------------------------------------------
//  Scale the Doom indexed screen to 1080p
// ------------------------------------------------------------
void I_FinishUpdate(void) {
    u32 *fb = (u32 *)v.fbs[active];

    // Calculate integer scale factor that best fits the screen
    int scale_x = SCR_W / 320; // 6
    int scale_y = SCR_H / 200; // 5
    int scale = (scale_x < scale_y) ? scale_x : scale_y; // 5
    if (scale < 1) scale = 1;

    int src_w = 320;
    int src_h = 200;
    int dst_w = src_w * scale; // 1600
    int dst_h = src_h * scale; // 1000
    int off_x = (SCR_W - dst_w) / 2; // 160
    int off_y = (SCR_H - dst_h) / 2; // 40

    const u8 *src = screen; // Doom's generic framebuffer

    for (int y = 0; y < src_h; y++) {
        int base_y = off_y + y * scale;
        for (int x = 0; x < src_w; x++) {
            u8 idx = src[y * src_w + x];
            u32 color = curpal[idx];  // pre‑converted ARGB from Doom's palette
            int base_x = off_x + x * scale;
            for (int dy = 0; dy < scale; dy++) {
                u32 *row = &fb[(base_y + dy) * SCR_W + base_x];
                for (int dx = 0; dx < scale; dx++)
                    row[dx] = color;
            }
        }
    }

    // Black bars
    for (int y = 0; y < SCR_H; y++) {
        for (int x = 0; x < SCR_W; x++) {
            if (x < off_x || x >= off_x + dst_w || y < off_y || y >= off_y + dst_h)
                fb[y * SCR_W + x] = 0xFF000000;
        }
    }

    video_flip(&v, active);
    active ^= 1;
}
