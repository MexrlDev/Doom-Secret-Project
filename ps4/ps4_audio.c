#include "doomgeneric.h"

extern void *G;
extern void *D;
extern s32 audio_h;
extern void *aud_out_fn;

#define SAMPLE_RATE     48000
#define SAMPLES_PER_BUF 256

static s16 mix_buffer[SAMPLES_PER_BUF * 2]; // stereo interleaved

void I_InitSound(void) {
    // Audio handle already opened in main.c
}

void I_SubmitSound(void) {
    if (audio_h < 0 || !aud_out_fn) return;
    if (sndSamples <= 0) return;

    s16 *src = (s16 *)sndOutput;
    int in_len = sndSamples;          // number of mono samples

    // Simple nearest-neighbour resampling (no float)
    int out_len = SAMPLES_PER_BUF;
    for (int i = 0; i < out_len; i++) {
        int src_idx = (i * in_len) / out_len;  // integer mapping
        if (src_idx >= in_len) src_idx = in_len - 1;
        s16 sample = src[src_idx];
        mix_buffer[i * 2]     = sample;   // left
        mix_buffer[i * 2 + 1] = sample;   // right
    }

    // Push to hardware (blocking call)
    NC(G, aud_out_fn, (u64)audio_h, (u64)mix_buffer, 0, 0, 0, 0);
}

void I_ShutdownSound(void) {
    // Audio handle is closed later by main.c
}
