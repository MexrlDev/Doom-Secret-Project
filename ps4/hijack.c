#include "hijack.h"

int video_hijack(struct video_ctx *v, void *G, void *D, u64 eboot_base, struct ext_args *ext) {
    v->G = G;
    v->D = D;               // store dlsym pointer for later use
    v->handle = -1;
    v->eq = 0;
    v->vmem = 0;
    v->fbs[0] = v->fbs[1] = 0;

    // Resolve essential functions (using D)
    void *usleep    = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelUsleep");
    void *cancel    = SYM(G, D, LIBKERNEL_HANDLE, "scePthreadCancel");
    void *load_mod  = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelLoadStartModule");
    void *alloc_dm  = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelAllocateDirectMemory");
    void *map_dm    = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelMapDirectMemory");
    void *dm_size   = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelGetDirectMemorySize");
    void *create_eq = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelCreateEqueue");
    void *delete_eq = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelDeleteEqueue");

    if (!usleep || !load_mod || !alloc_dm || !map_dm) return -1;

    // Kill existing graphics thread
    if (cancel) {
        u64 gs = *(u64 *)(eboot_base + EBOOT_GS_THREAD);
        if (gs) NC(G, cancel, gs, 0,0,0,0,0);
    }
    NC(G, usleep, 300000, 0,0,0,0,0);

    // Load video out module
    s32 vid_mod = (s32)NC(G, load_mod, (u64)"libSceVideoOut.sprx", 0,0,0,0,0);
    if (vid_mod < 0) return -2;
    v->vid_open  = SYM(G, D, vid_mod, "sceVideoOutOpen");
    v->vid_close = SYM(G, D, vid_mod, "sceVideoOutClose");
    v->vid_reg   = SYM(G, D, vid_mod, "sceVideoOutRegisterBuffers");
    v->vid_flip  = SYM(G, D, vid_mod, "sceVideoOutSubmitFlip");
    v->vid_rate  = SYM(G, D, vid_mod, "sceVideoOutSetFlipRate");
    v->vid_evt   = SYM(G, D, vid_mod, "sceVideoOutAddFlipEvent");

    if (!v->vid_open || !v->vid_close || !v->vid_reg || !v->vid_flip) return -3;

    // Close existing video out handle
    s32 emu_vid = *(s32 *)(eboot_base + EBOOT_VIDOUT);
    if (v->vid_close && emu_vid >= 0) NC(G, v->vid_close, (u64)emu_vid, 0,0,0,0,0);
    NC(G, usleep, 100000, 0,0,0,0,0);

    // Open a fresh video out handle
    v->handle = (s32)NC(G, v->vid_open, 0xFF, 0, 0, 0, 0, 0);
    if (v->handle < 0) return -4;

    // Create event queue for vsync
    if (create_eq) NC(G, create_eq, (u64)&v->eq, (u64)"vidq", 0,0,0,0);
    if (v->vid_evt && v->eq) NC(G, v->vid_evt, v->eq, (u64)v->handle, 0,0,0,0);

    // Allocate direct memory for framebuffers
    u64 mem_total = dm_size ? NC(G, dm_size, 0,0,0,0,0,0) : 0x300000000ULL;
    u64 phys = 0;
    NC(G, alloc_dm, 0, mem_total, FB_TOTAL, 0x200000, 3, (u64)&phys);
    NC(G, map_dm, (u64)&v->vmem, FB_TOTAL, 0x33, 0, phys, 0x200000);
    if (!v->vmem) {
        if (v->vid_close) NC(G, v->vid_close, (u64)v->handle, 0,0,0,0,0);
        if (v->eq && delete_eq) NC(G, delete_eq, v->eq, 0,0,0,0,0);
        return -5;
    }

    // Clear the direct memory to avoid garbage
    u32 *fb0 = (u32*)v->vmem;
    u32 *fb1 = (u32*)((u8*)v->vmem + FB_ALIGNED);
    for (int i = 0; i < SCR_W * SCR_H; i++) {
        fb0[i] = 0xFF000000;
        fb1[i] = 0xFF000000;
    }

    // Register framebuffers
    u8 attr[64] = {0};
    *(u32*)(attr + 0)  = 0x80000000;
    *(u32*)(attr + 4)  = 1;
    *(u32*)(attr + 12) = SCR_W;
    *(u32*)(attr + 16) = SCR_H;
    *(u32*)(attr + 20) = SCR_W;

    v->fbs[0] = fb0;
    v->fbs[1] = fb1;

    if (NC(G, v->vid_reg, (u64)v->handle, 0, (u64)v->fbs, 2, (u64)attr, 0) != 0) {
        // cleanup
        if (v->vid_close) NC(G, v->vid_close, (u64)v->handle, 0,0,0,0,0);
        return -6;
    }

    // Set flip rate to 0
    if (v->vid_rate) NC(G, v->vid_rate, (u64)v->handle, 0, 0,0,0,0);

    return 0;
}

void video_cleanup(struct video_ctx *v) {
    if (!v->G) return;

    // Resolve needed functions using stored D
    void *usleep    = SYM(v->G, v->D, LIBKERNEL_HANDLE, "sceKernelUsleep");
    void *delete_eq = SYM(v->G, v->D, LIBKERNEL_HANDLE, "sceKernelDeleteEqueue");
    // munmap not currently used .. suppress warning by omitting

    // Clear screen
    if (v->fbs[0]) {
        u32 *fb = (u32*)v->fbs[0];
        for (int i = 0; i < SCR_W * SCR_H; i++) fb[i] = 0xFF000000;
    }
    if (v->fbs[1]) {
        u32 *fb = (u32*)v->fbs[1];
        for (int i = 0; i < SCR_W * SCR_H; i++) fb[i] = 0xFF000000;
    }

    // Flip to clear
    if (v->vid_flip && v->handle >= 0)
        NC(v->G, v->vid_flip, (u64)v->handle, 0, 1, 0, 0, 0);
    if (usleep) NC(v->G, usleep, 50000, 0,0,0,0,0);

    if (v->vid_close && v->handle >= 0)
        NC(v->G, v->vid_close, (u64)v->handle, 0,0,0,0,0);

    if (v->eq && delete_eq)
        NC(v->G, delete_eq, v->eq, 0,0,0,0,0);
}

void video_flip(struct video_ctx *v, int active) {
    if (!v->vid_flip || v->handle < 0) return;
    NC(v->G, v->vid_flip, (u64)v->handle, (u64)active, 1, 0, 0, 0);
    // Wait for flip event
    if (v->eq) {
        void *wait_eq = SYM(v->G, v->D, LIBKERNEL_HANDLE, "sceKernelWaitEqueue");
        if (wait_eq) {
            u8 evt[64]; s32 cnt = 0;
            NC(v->G, wait_eq, v->eq, (u64)evt, 1, (u64)&cnt, 0, 0);
        }
    }
}
