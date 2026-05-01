#ifndef CORE_H
#define CORE_H

// Basic integer types for freestanding environment
typedef unsigned long  u64;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;
typedef long           s64;
typedef int            s32;
typedef short          s16;
typedef signed char    s8;

typedef u32 size_t;

// Gadget offset and kernel handle for syscall resolution
#define GADGET_OFFSET    0x31AA9
#define LIBKERNEL_HANDLE 0x2001

// Offsets in the EBOOT for existing thread/video handles
#define EBOOT_GS_THREAD  0x057F89B0
#define EBOOT_VIDOUT     0x02d695d0

// Display parameters (1920x1080)
#define SCR_W       1920
#define SCR_H       1080
#define FB_SIZE     (SCR_W * SCR_H * 4)
#define FB_ALIGNED  ((FB_SIZE + 0x1FFFFF) & ~0x1FFFFF)
#define FB_TOTAL    (FB_ALIGNED * 2)       // double buffering

// External arguments passed from Lua
struct ext_args {
    s64 status;
    s64 step;
    u32 frame_count;
    u32 _pad;
    s32 log_fd;          // UDP log socket
    s32 pad_fd;          // unused
    u8  log_addr[16];    // remote UDP address
    u64 dbg[8];          // additional data
};

// Native syscall invocation via gadget
__attribute__((naked))
static u64 native_call(void *gadget, void *fn,
                       u64 a1, u64 a2, u64 a3,
                       u64 a4, u64 a5, u64 a6)
{
    __asm__ volatile (
        "pushq %%rbx\n\t"
        "movq %%rsi, %%rbx\n\t"
        "movq %%rdi, %%rax\n\t"
        "movq %%rdx, %%rdi\n\t"
        "movq %%rcx, %%rsi\n\t"
        "movq %%r8,  %%rdx\n\t"
        "movq %%r9,  %%rcx\n\t"
        "movq 16(%%rsp), %%r8\n\t"
        "movq 24(%%rsp), %%r9\n\t"
        "callq *%%rax\n\t"
        "popq %%rbx\n\t"
        "retq" ::: "memory"
    );
}

// Resolve a function from a loaded module
static void *resolve_sym(void *gadget, void *dlsym_fn, s32 handle, const char *name) {
    void *addr = 0;
    native_call(gadget, dlsym_fn, (u64)handle, (u64)name, (u64)&addr, 0, 0, 0);
    return addr;
}

#define NC  native_call
#define SYM resolve_sym

#define NULL 0

#endif
