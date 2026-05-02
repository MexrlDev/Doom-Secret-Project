#include "core.h"
#include "hijack.h"
#include "doomgeneric.h"

extern u8  *screen;
extern u32 *curpal;
extern void *G;
extern void *D;
extern struct video_ctx v;

static int active = 0;

void I_InitGraphics(void) { }
void I_ShutdownGraphics(void) { }
void I_SetWindowTitle(const char *title) { (void)title; }

/* ---- new stubs ---- */
void *I_VideoBuffer(void) {
    return screen;
}

void I_SetPalette(int palette_index) {
    /* palette updates are handled via curpal */
}

int I_GetPaletteIndex(int r, int g, int b) {
    return 0;   /* rarely called */
}

/* ---------------------------------------------------------------- */
void I_FinishUpdate(void) {
    u32 *fb = (u32 *)v.fbs[active];

    int scale_x = SCR_W / 320;
    int scale_y = SCR_H / 200;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;

    int src_w = 320;
    int src_h = 200;
    int dst_w = src_w * scale;
    int dst_h = src_h * scale;
    int off_x = (SCR_W - dst_w) / 2;
    int off_y = (SCR_H - dst_h) / 2;

    const u8 *src = screen;

    for (int y = 0; y < src_h; y++) {
        int base_y = off_y + y * scale;
        for (int x = 0; x < src_w; x++) {
            u8 idx = src[y * src_w + x];
            u32 color = curpal[idx];
            int base_x = off_x + x * scale;
            for (int dy = 0; dy < scale; dy++) {
                u32 *row = &fb[(base_y + dy) * SCR_W + base_x];
                for (int dx = 0; dx < scale; dx++)
                    row[dx] = color;
            }
        }
    }

    /* black bars */
    for (int y = 0; y < SCR_H; y++) {
        for (int x = 0; x < SCR_W; x++) {
            if (x < off_x || x >= off_x + dst_w || y < off_y || y >= off_y + dst_h)
                fb[y * SCR_W + x] = 0xFF000000;
        }
    }

    video_flip(&v, active);
    active ^= 1;
}
