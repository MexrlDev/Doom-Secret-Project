#include "doomgeneric.h"

extern void *G;
extern void *D;
extern s32 audio_h;
extern void *aud_out_fn;

#define SAMPLE_RATE     48000
#define SAMPLES_PER_BUF 256

// --- Sound globals (used by the engine) ---
void *sndOutput = NULL;
int   sndSamples = 0;

static s16 mix_buffer[SAMPLES_PER_BUF * 2];

void I_InitSound(void) { }

void I_SubmitSound(void) {
    if (audio_h < 0 || !aud_out_fn) return;
    if (sndSamples <= 0) return;

    s16 *src = (s16 *)sndOutput;
    int in_len = sndSamples;

    int out_len = SAMPLES_PER_BUF;
    for (int i = 0; i < out_len; i++) {
        int src_idx = (i * in_len) / out_len;
        if (src_idx >= in_len) src_idx = in_len - 1;
        s16 sample = src[src_idx];
        mix_buffer[i * 2]     = sample;
        mix_buffer[i * 2 + 1] = sample;
    }

    NC(G, aud_out_fn, (u64)audio_h, (u64)mix_buffer, 0, 0, 0, 0);
}

void I_ShutdownSound(void) { }

/* ---- remaining sound stubs (I_* functions) ---- */
int  I_SoundIsPlaying(int handle) { return 0; }
void I_StopSound(int handle) { }
int  I_GetSfxLumpNum(void *sfxinfo) { return 0; }
int  I_StartSound(int handle, void *data, int vol, int sep, int pitch, int pri) { return 0; }
void I_PauseSong(int handle) { }
void I_ResumeSong(int handle) { }
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch) { }
void I_PrecacheSounds(void) { }
void I_ShutdownMusic(void) { }
void I_SetMusicVolume(int volume) { }
int  I_MusicIsPlaying(void) { return 0; }
int  I_RegisterSong(void *data, int len) { return 0; }
void I_PlaySong(int handle, int looping) { }
void I_StopSong(int handle) { }
void I_UnRegisterSong(int handle) { }
// Note: I_UpdateSound() is already defined in ps4_system.c, so we omit it here.
