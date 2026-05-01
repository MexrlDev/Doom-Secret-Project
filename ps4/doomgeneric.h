/* PS4 DoomGeneric – no standard headers */
#include "doomgeneric.h"

#define DOOMGENERIC_RESX  320
#define DOOMGENERIC_RESY  200

typedef u32 pixel_t;          /* 32‑bit ARGB */

pixel_t* DG_ScreenBuffer = NULL;

/* ---- memory functions (provided by ps4_system) ---- */
extern void* my_malloc(u32 size);
extern void  my_free(void *ptr, u32 size);

/* ---- engine entry points ---- */
extern void D_DoomMain(void);
extern void D_Display(void);

void doomgeneric_Init(void) {
    DG_ScreenBuffer = (pixel_t*)my_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
    if (!DG_ScreenBuffer) I_Error("Failed to allocate screen buffer");

    /* screen and curpal will be set up by D_DoomMain */
    screen = (u8*)DG_ScreenBuffer;
    D_DoomMain();
}

void doomgeneric_Tick(void) {
    D_Display();
}

void doomgeneric_Shutdown(void) {
    if (DG_ScreenBuffer) {
        my_free(DG_ScreenBuffer, DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
        DG_ScreenBuffer = NULL;
    }
}
