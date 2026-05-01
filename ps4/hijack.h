#ifndef HIJACK_H
#define HIJACK_H

#include "core.h"

// Forward declaration to avoid "struct declared inside parameter list" warning
struct ext_args;

// Video context holding handles and framebuffers
struct video_ctx {
    void *G;                // gadget pointer
    void *D;                // dlsym function pointer
    void *vid_open;
    void *vid_close;
    void *vid_reg;
    void *vid_flip;
    void *vid_rate;
    void *vid_evt;
    s32   handle;           // video out handle
    u64   eq;               // event queue
    void *vmem;             // direct memory pointer
    void *fbs[2];           // two framebuffers
};

// Hijack video: kill existing thread, close old handle, open new one, allocate buffers.
int video_hijack(struct video_ctx *v, void *G, void *D, u64 eboot_base, struct ext_args *ext);

// Release video resources and restore
void video_cleanup(struct video_ctx *v);

// Flip framebuffer and wait for vsync
void video_flip(struct video_ctx *v, int active);

#endif
