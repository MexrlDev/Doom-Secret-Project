#ifndef DOOMGENERIC_H
#define DOOMGENERIC_H

#include "core.h"       // u8, u32, s16, size_t, …

// Doom engine framebuffer and palette
extern u8  *screen;     // 320×200 indexed framebuffer
extern u32 *curpal;     // 256‑entry ARGB palette

// Doom sound output
extern void *sndOutput; // 16‑bit mono samples
extern int  sndSamples; // number of samples available

// Platform functions – implemented in PS4 layer
void I_Init(void);
void I_Shutdown(void);
void I_Error(const char *msg);
void I_Quit(void);
void I_PrintStr(const char *str);

void I_InitGraphics(void);
void I_ShutdownGraphics(void);
void I_FinishUpdate(void);
void I_ReadKeys(void);
int  I_GetKey(void);
void I_SetWindowTitle(const char *title);

void I_InitNetwork(void);
void I_NetCmd(void);

void I_StartTic(void);
void I_StartFrame(void);
void I_UpdateSound(void);
void I_SubmitSound(void);

// Generic engine entry points
void doomgeneric_Init(void);
void doomgeneric_Tick(void);
void doomgeneric_Shutdown(void);

#endif
