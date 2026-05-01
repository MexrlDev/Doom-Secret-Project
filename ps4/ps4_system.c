#include "core.h"
#include "doomgeneric.h"

extern void *G;
extern void *D;
extern s32 log_fd;
extern u8 log_sa[16];

/* local size_t (same as u64 from core.h) */
typedef u64 size_t;

/* Fake FILE type */
typedef int MY_FILE;

static s32 fd_table[16] = { -1, -1, -1, -1, -1, -1, -1, -1,
                            -1, -1, -1, -1, -1, -1, -1, -1 };

/* ------------------------------------------------------------------ */
/*  Memory / string utilities                                          */
/* ------------------------------------------------------------------ */

void *memset(void *s, int c, size_t n) {
    u8 *p = (u8*)s;
    while (n--) *p++ = (u8)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    u8 *d = (u8*)dest;
    const u8 *s = (const u8*)src;
    while (n--) *d++ = *s++;
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const u8 *p1 = (const u8*)s1, *p2 = (const u8*)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    if (!n) return 0;
    while (--n && *s1 && *s1 == *s2) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

/* ------------------------------------------------------------------ */
/*  Custom memory allocator (mmap based)                               */
/* ------------------------------------------------------------------ */

void *my_malloc(u32 size) {
    void *mmap = SYM(G, D, LIBKERNEL_HANDLE, "mmap");
    if (!mmap) return NULL;
    void *ptr = (void*)NC(G, mmap, 0, (u64)size, 3, 0x1002, (u64)-1, 0);
    if ((s64)ptr == -1) return NULL;
    return ptr;
}

void my_free(void *ptr, u32 size) {
    void *munmap = SYM(G, D, LIBKERNEL_HANDLE, "munmap");
    if (munmap && ptr) NC(G, munmap, (u64)ptr, (u64)size, 0, 0, 0, 0);
}

/* ------------------------------------------------------------------ */
/*  Platform callbacks                                                 */
/* ------------------------------------------------------------------ */

void I_Error(const char *msg) {
    void *sendto = SYM(G, D, LIBKERNEL_HANDLE, "sendto");
    if (sendto && log_fd >= 0) {
        size_t len = strlen(msg);
        NC(G, sendto, (u64)log_fd, (u64)msg, (u64)len, 0, (u64)log_sa, 16);
    }
    while (1) {}
}

void I_Quit(void) { }

void I_PrintStr(const char *str) {
    void *sendto = SYM(G, D, LIBKERNEL_HANDLE, "sendto");
    if (sendto && log_fd >= 0) {
        size_t len = strlen(str);
        NC(G, sendto, (u64)log_fd, (u64)str, (u64)len, 0, (u64)log_sa, 16);
    }
}

void I_Init(void) { }
void I_Shutdown(void) { }

/* ------------------------------------------------------------------ */
/*  File I/O (only needed for reading the WAD)                         */
/* ------------------------------------------------------------------ */

MY_FILE *fopen(const char *path, const char *mode) {
    (void)mode;
    void *kopen = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelOpen");
    if (!kopen) return NULL;
    s32 fd = (s32)NC(G, kopen, (u64)path, 0, 0, 0, 0, 0);
    if (fd < 0) return NULL;

    for (int i = 0; i < 16; i++) {
        if (fd_table[i] == -1) {
            fd_table[i] = fd;
            return (MY_FILE *)(u64)(i + 1);
        }
    }
    void *kclose = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelClose");
    if (kclose) NC(G, kclose, (u64)fd, 0, 0, 0, 0, 0);
    return NULL;
}

int fclose(MY_FILE *stream) {
    if (!stream) return -1;
    int idx = (int)((u64)stream - 1);
    if (idx < 0 || idx >= 16 || fd_table[idx] == -1) return -1;
    void *kclose = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelClose");
    if (kclose) NC(G, kclose, (u64)fd_table[idx], 0, 0, 0, 0, 0);
    fd_table[idx] = -1;
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, MY_FILE *stream) {
    if (!stream) return 0;
    int idx = (int)((u64)stream - 1);
    if (idx < 0 || idx >= 16 || fd_table[idx] == -1) return 0;
    void *kread = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelRead");
    if (!kread) return 0;
    s64 total = (s64)(size * nmemb);
    s32 n = (s32)NC(G, kread, (u64)fd_table[idx], (u64)ptr, (u64)total, 0, 0, 0);
    return (n > 0) ? (size_t)(n / size) : 0;
}

int fseek(MY_FILE *stream, s64 offset, int whence) {
    if (!stream) return -1;
    int idx = (int)((u64)stream - 1);
    if (idx < 0 || idx >= 16 || fd_table[idx] == -1) return -1;
    void *klseek = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelLseek");
    if (!klseek) return -1;
    NC(G, klseek, (u64)fd_table[idx], (u64)offset, (u64)whence, 0, 0, 0);
    return 0;
}
