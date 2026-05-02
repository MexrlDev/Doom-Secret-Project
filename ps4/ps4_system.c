#include "core.h"
#include "doomgeneric.h"

extern void *G;
extern void *D;
extern s32 log_fd;
extern u8 log_sa[16];

/* local size_t (same as u64 from core.h) */
typedef u64 size_t;

/* Fake FILE type (used by the engine’s file I/O) */
typedef int MY_FILE;

/* ---------------------------------------------------------------- */
/*  Global configuration (normally from command line / config file)  */
/* ---------------------------------------------------------------- */
int usemouse           = 0;
int mouse_acceleration = 0;
int mouse_threshold    = 0;
int snd_musicdevice    = 0;

/* ---------------------------------------------------------------- */
/*  Standard file descriptor table (already present)                 */
/* ---------------------------------------------------------------- */
static s32 fd_table[16] = { -1, -1, -1, -1, -1, -1, -1, -1,
                            -1, -1, -1, -1, -1, -1, -1, -1 };

/* ---------------------------------------------------------------- */
/*  Basic string / memory utilities (already present)                */
/* ---------------------------------------------------------------- */
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

/* ---------------------------------------------------------------- */
/*  Missing C library functions                                      */
/* ---------------------------------------------------------------- */

int abs(int x) { return (x < 0) ? -x : x; }

int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = (*s1 >= 'a' && *s1 <= 'z') ? *s1 - 32 : *s1;
        int c2 = (*s2 >= 'a' && *s2 <= 'z') ? *s2 - 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    if (!n) return 0;
    while (n-- && *s1 && *s2) {
        int c1 = (*s1 >= 'a' && *s1 <= 'z') ? *s1 - 32 : *s1;
        int c2 = (*s2 >= 'a' && *s2 <= 'z') ? *s2 - 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    if (n == (size_t)-1) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dest;
}

double fabs(double x) { return (x < 0) ? -x : x; }

/* ---------------------------------------------------------------- */
/*  printf / fprintf / snprintf (minimal – send to UDP log)          */
/* ---------------------------------------------------------------- */

static void log_string(const char *s) {
    void *sendto = SYM(G, D, LIBKERNEL_HANDLE, "sendto");
    if (sendto && log_fd >= 0) {
        size_t len = strlen(s);
        NC(G, sendto, (u64)log_fd, (u64)s, (u64)len, 0, (u64)log_sa, 16);
    }
}

int printf(const char *fmt, ...) {
    log_string(fmt);
    return 0;
}

int fprintf(MY_FILE *stream, const char *fmt, ...) {
    log_string(fmt);
    return 0;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    // just copy the format string (enough for DEH_snprintf usage)
    if (size > 0) {
        size_t len = strlen(fmt);
        if (len >= size) len = size - 1;
        memcpy(buf, fmt, len);
        buf[len] = '\0';
    }
    return 0;
}

/* stderr - just a dummy file-pointer */
MY_FILE *stderr = (MY_FILE*)0;

/* fortified functions (ignore the extra checks) */
int __printf_chk(int flag, const char *fmt, ...) {
    log_string(fmt);
    return 0;
}
int __fprintf_chk(MY_FILE *stream, int flag, const char *fmt, ...) {
    log_string(fmt);
    return 0;
}
int __snprintf_chk(char *buf, size_t maxlen, int flag, size_t os, const char *fmt, ...) {
    if (maxlen > 0) {
        size_t len = strlen(fmt);
        if (len >= maxlen) len = maxlen - 1;
        memcpy(buf, fmt, len);
        buf[len] = '\0';
    }
    return 0;
}

void *__memset_chk(void *s, int c, size_t n, size_t os) {
    return memset(s, c, n);
}
void *__memcpy_chk(void *dest, const void *src, size_t n, size_t os) {
    return memcpy(dest, src, n);
}

/* ---------------------------------------------------------------- */
/*  Memory allocation (replace malloc/calloc/free)                   */
/* ---------------------------------------------------------------- */

void *my_malloc(u32 size);  /* defined below */
void my_free(void *ptr, u32 size); /* defined below */

void *malloc(size_t size) {
    return my_malloc((u32)size);
}

void *calloc(size_t nmemb, size_t size) {
    u32 total = (u32)(nmemb * size);
    void *p = my_malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void free(void *ptr) {
    // We can't know the size, but it's okay for our zone allocator
    // The engine uses its own Z_Malloc/Z_Free, not raw free.
    // This is a compatibility shim.
    // Since we use mmap with fixed sizes, we ignore the free.
}

/* ---------------------------------------------------------------- */
/*  Custom memory allocator (mmap based)                             */
/* ---------------------------------------------------------------- */

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

/* ---------------------------------------------------------------- */
/*  File I/O (extended with ftell / fwrite)                          */
/* ---------------------------------------------------------------- */

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

long ftell(MY_FILE *stream) {
    if (!stream) return -1;
    int idx = (int)((u64)stream - 1);
    if (idx < 0 || idx >= 16 || fd_table[idx] == -1) return -1;
    void *klseek = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelLseek");
    if (!klseek) return -1;
    s64 pos = (s64)NC(G, klseek, (u64)fd_table[idx], 0, 1, 0, 0, 0); /* SEEK_CUR */
    return (long)pos;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, MY_FILE *stream) {
    if (!stream) return 0;
    int idx = (int)((u64)stream - 1);
    if (idx < 0 || idx >= 16 || fd_table[idx] == -1) return 0;
    void *kwrite = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelWrite");
    if (!kwrite) return 0;
    s64 total = (s64)(size * nmemb);
    s32 n = (s32)NC(G, kwrite, (u64)fd_table[idx], (u64)ptr, (u64)total, 0, 0, 0);
    return (n > 0) ? (size_t)(n / size) : 0;
}

/* ---------------------------------------------------------------- */
/*  Platform callbacks                                               */
/* ---------------------------------------------------------------- */

void I_Error(const char *msg) {
    log_string(msg);
    while (1) {}
}

void I_Quit(void) { }
void I_Init(void) { }
void I_Shutdown(void) { }

void I_PrintStr(const char *str) { log_string(str); }

/* ---------------------------------------------------------------- */
/*  Additional I_* stubs required by the engine                      */
/* ---------------------------------------------------------------- */

void *I_ZoneBase(int *size) {
    // Provide a 16 MB heap (adjust if needed)
    static u8 *heap = NULL;
    static int  heap_size = 0;
    if (!heap) {
        heap_size = 16 * 1024 * 1024;
        heap = (u8*)my_malloc(heap_size);
    }
    *size = heap_size;
    return heap;
}

void I_BeginRead(void) { }
void I_EndRead(void) { }

int I_ConsoleStdout(void) {
    return 1;   // pretend we have stdout
}

int I_GetMemoryValue(unsigned int offset, void *address, unsigned int size) {
    // Used by GetSectorAtNullAddress – just return 0 for safety
    return 0;
}

void I_AtExit(void (*func)(void), const char *name) {
    // We don't support atexit
}
