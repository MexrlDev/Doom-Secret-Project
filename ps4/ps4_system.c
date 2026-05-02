#include "core.h"
#include "doomgeneric.h"

extern void *G;
extern void *D;
extern s32 log_fd;
extern u8 log_sa[16];

/* size_t (matches system: u64) */
typedef u64 size_t;

/* Fake FILE */
typedef int MY_FILE;

/* ---------- global config (referenced by engine) ---------- */
int usemouse           = 0;
int mouse_acceleration = 0;
int mouse_threshold    = 0;
int snd_musicdevice    = 0;
int screensaver_mode   = 0;
int screenvisible      = 1;
int usegamma           = 0;
int vanilla_keyboard_mapping = 0;

/* ---------- fd table ---------- */
static s32 fd_table[16] = { -1, -1, -1, -1, -1, -1, -1, -1,
                            -1, -1, -1, -1, -1, -1, -1, -1 };

/* ============== Standard C library replacements ============== */
void *memset(void *s, int c, size_t n) {
    u8 *p = (u8*)s; while (n--) *p++ = (u8)c; return s;
}
void *memcpy(void *dest, const void *src, size_t n) {
    u8 *d = (u8*)dest; const u8 *s = (const u8*)src; while (n--) *d++ = *s++; return dest;
}
int memcmp(const void *s1, const void *s2, size_t n) {
    const u8 *p1 = (const u8*)s1, *p2 = (const u8*)s2;
    while (n--) { if (*p1 != *p2) return *p1 - *p2; p1++; p2++; } return 0;
}
char *strcpy(char *dest, const char *src) {
    char *d = dest; while ((*d++ = *src++)); return dest;
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
size_t strlen(const char *s) { size_t n = 0; while (*s++) n++; return n; }
int abs(int x) { return (x < 0) ? -x : x; }
int toupper(int c) { if (c >= 'a' && c <= 'z') return c - 32; return c; }
int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = (*s1 >= 'a' && *s1 <= 'z') ? *s1 - 32 : *s1;
        int c2 = (*s2 >= 'a' && *s2 <= 'z') ? *s2 - 32 : *s2;
        if (c1 != c2) return c1 - c2; s1++; s2++;
    } return *(unsigned char*)s1 - *(unsigned char*)s2;
}
int strncasecmp(const char *s1, const char *s2, size_t n) {
    if (!n) return 0;
    while (n-- && *s1 && *s2) {
        int c1 = (*s1 >= 'a' && *s1 <= 'z') ? *s1 - 32 : *s1;
        int c2 = (*s2 >= 'a' && *s2 <= 'z') ? *s2 - 32 : *s2;
        if (c1 != c2) return c1 - c2; s1++; s2++;
    } if (n == (size_t)-1) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}
char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest; while (n && (*d++ = *src++)) n--; while (n--) *d++ = '\0'; return dest;
}
double fabs(double x) { return (x < 0) ? -x : x; }
char *strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return (char*)s; s++; } return NULL;
}
char *strrchr(const char *s, int c) {
    const char *last = NULL; while (*s) { if (*s == (char)c) last = s; s++; } return (char*)last;
}
char *strdup(const char *s) {
    size_t len = strlen(s) + 1; char *p = (char*)malloc(len);
    if (p) memcpy(p, s, len); return p;
}
char *strstr(const char *haystack, const char *needle) {
    size_t len = strlen(needle); if (!len) return (char*)haystack;
    for (; *haystack; haystack++) if (!strncmp(haystack, needle, len)) return (char*)haystack;
    return NULL;
}
int atoi(const char *s) {
    int n = 0, sign = 1; while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; } else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    return n * sign;
}
double atof(const char *s) {
    double val = 0, power = 1; int sign = 1; while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; } else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { val = val * 10.0 + (*s - '0'); s++; }
    if (*s == '.') { s++; while (*s >= '0' && *s <= '9') {
        val = val * 10.0 + (*s - '0'); power *= 10; s++; } }
    return sign * val / power;
}
int remove(const char *pathname) { return -1; }
int rename(const char *oldpath, const char *newpath) { return -1; }

/* ---------- printf/fprintf/snprintf (to UDP log) ---------- */
static void log_str(const char *s) {
    void *sendto = SYM(G, D, LIBKERNEL_HANDLE, "sendto");
    if (sendto && log_fd >= 0) {
        size_t len = strlen(s);
        NC(G, sendto, (u64)log_fd, (u64)s, (u64)len, 0, (u64)log_sa, 16);
    }
}
int printf(const char *fmt, ...) { log_str(fmt); return 0; }
int fprintf(MY_FILE *stream, const char *fmt, ...) { log_str(fmt); return 0; }
int snprintf(char *buf, size_t size, const char *fmt, ...) {
    if (size > 0) { size_t len = strlen(fmt); if (len >= size) len = size-1;
        memcpy(buf, fmt, len); buf[len] = 0; } return 0;
}
MY_FILE *stderr = (MY_FILE*)0;

/* fortified variants */
int __printf_chk(int flag, const char *fmt, ...) { log_str(fmt); return 0; }
int __fprintf_chk(MY_FILE *stream, int flag, const char *fmt, ...) { log_str(fmt); return 0; }
int __snprintf_chk(char *buf, size_t maxlen, int flag, size_t os, const char *fmt, ...) {
    if (maxlen > 0) { size_t len = strlen(fmt); if (len >= maxlen) len = maxlen-1;
        memcpy(buf, fmt, len); buf[len] = 0; } return 0;
}
void *__memset_chk(void *s, int c, size_t n, size_t os) { return memset(s, c, n); }
void *__memcpy_chk(void *dest, const void *src, size_t n, size_t os) { return memcpy(dest, src, n); }
int __vsnprintf_chk(char *buf, size_t maxlen, int flag, size_t os, const char *fmt, va_list ap) {
    if (maxlen > 0) { size_t len = strlen(fmt); if (len >= maxlen) len = maxlen-1;
        memcpy(buf, fmt, len); buf[len] = 0; } return 0;
}
int __isoc99_sscanf(const char *str, const char *format, ...) { return 0; }

/* ctype stubs */
typedef unsigned long ctype_bits;
ctype_bits *__ctype_b_loc(void) { static ctype_bits dummy[384]; return dummy; }
int *__ctype_toupper_loc(void) {
    static int table[384];
    if (!table[0]) for (int i=0; i<384; i++) table[i] = (i>='a' && i<='z') ? i-32 : i;
    return table;
}

int *__errno_location(void) { static int e = 0; return &e; }

/* ---------- memory ---------- */
void *malloc(size_t size) { return my_malloc((u32)size); }
void *calloc(size_t nmemb, size_t size) {
    u32 total = (u32)(nmemb * size); void *p = my_malloc(total);
    if (p) memset(p, 0, total); return p;
}
void free(void *ptr) { /* unused */ }

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

/* ---------- file I/O ---------- */
MY_FILE *fopen(const char *path, const char *mode) {
    (void)mode; void *kopen = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelOpen");
    if (!kopen) return NULL; s32 fd = (s32)NC(G, kopen, (u64)path, 0,0,0,0,0);
    if (fd < 0) return NULL;
    for (int i=0; i<16; i++) {
        if (fd_table[i] == -1) { fd_table[i] = fd; return (MY_FILE*)(u64)(i+1); }
    }
    void *kclose = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelClose");
    if (kclose) NC(G, kclose, (u64)fd, 0,0,0,0,0);
    return NULL;
}
int fclose(MY_FILE *stream) {
    if (!stream) return -1; int idx = (int)((u64)stream - 1);
    if (idx<0 || idx>=16 || fd_table[idx]==-1) return -1;
    void *kclose = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelClose");
    if (kclose) NC(G, kclose, (u64)fd_table[idx], 0,0,0,0,0);
    fd_table[idx] = -1; return 0;
}
size_t fread(void *ptr, size_t size, size_t nmemb, MY_FILE *stream) {
    if (!stream) return 0; int idx = (int)((u64)stream - 1);
    if (idx<0 || idx>=16 || fd_table[idx]==-1) return 0;
    void *kread = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelRead");
    if (!kread) return 0; s64 total = (s64)(size * nmemb);
    s32 n = (s32)NC(G, kread, (u64)fd_table[idx], (u64)ptr, (u64)total, 0,0,0);
    return (n>0) ? (size_t)(n / size) : 0;
}
int fseek(MY_FILE *stream, s64 offset, int whence) {
    if (!stream) return -1; int idx = (int)((u64)stream - 1);
    if (idx<0 || idx>=16 || fd_table[idx]==-1) return -1;
    void *klseek = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelLseek");
    if (!klseek) return -1;
    NC(G, klseek, (u64)fd_table[idx], (u64)offset, (u64)whence, 0,0,0);
    return 0;
}
long ftell(MY_FILE *stream) {
    if (!stream) return -1; int idx = (int)((u64)stream - 1);
    if (idx<0 || idx>=16 || fd_table[idx]==-1) return -1;
    void *klseek = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelLseek");
    if (!klseek) return -1;
    s64 pos = (s64)NC(G, klseek, (u64)fd_table[idx], 0, 1, 0,0,0);
    return (long)pos;
}
size_t fwrite(const void *ptr, size_t size, size_t nmemb, MY_FILE *stream) {
    if (!stream) return 0; int idx = (int)((u64)stream - 1);
    if (idx<0 || idx>=16 || fd_table[idx]==-1) return 0;
    void *kwrite = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelWrite");
    if (!kwrite) return 0; s64 total = (s64)(size * nmemb);
    s32 n = (s32)NC(G, kwrite, (u64)fd_table[idx], (u64)ptr, (u64)total, 0,0,0);
    return (n>0) ? (size_t)(n / size) : 0;
}

/* ============== Platform callbacks ============== */
void I_Error(const char *msg) { log_str(msg); while(1){} }
void I_Quit(void) { }
void I_Init(void) { }
void I_Shutdown(void) { }
void I_PrintStr(const char *str) { log_str(str); }

/* time */
int I_GetTime(void) { return 0; }
int I_GetTimeMS(void) { return 0; }
void I_Sleep(int ms) { }

/* frame */
void I_StartTic(void) { }
void I_StartFrame(void) { }

/* video */
void I_UpdateNoBlit(void) { }
void I_ReadScreen(void *dest) { }
void I_Endoom(void) { }
void I_WaitVBL(int count) { }

/* input */
void I_BindVideoVariables(void) { }
void I_BindJoystickVariables(void) { }
void I_BindSoundVariables(void) { }
void I_SetGrabMouseCallback(void (*func)(void)) { }
void I_EnableLoadingDisk(void) { }
void I_Tactile(int on, int off, int total) { }

/* graphics/sound init */
void I_GraphicsCheckCommandLine(void) { }
void I_PrintBanner(void) { }
void I_DisplayFPSDots(void) { }
void I_PrintDivider(void) { }
void I_PrintStartupBanner(void) { }
int  I_CheckIsScreensaver(void) { return 0; }
void I_InitTimer(void) { }
void I_InitJoystick(void) { }
void I_InitMusic(void) { }

/* stats */
void StatDump(void) { }
void StatCopy(FILE *dest) { }

/* misc */
int I_ConsoleStdout(void) { return 1; }
int I_GetMemoryValue(unsigned int offset, void *address, unsigned int size) { return 0; }
void I_AtExit(void (*func)(void), const char *name) { }
void exit(int status) { while(1){} }

/* file system */
int mkdir(const char *pathname, unsigned int mode) { return -1; }

/* zone heap */
void *I_ZoneBase(int *size) {
    static u8 *heap = NULL;
    static int heap_sz = 0;
    if (!heap) { heap_sz = 16*1024*1024; heap = (u8*)my_malloc(heap_sz); }
    *size = heap_sz; return heap;
}
void I_BeginRead(void) { }
void I_EndRead(void) { }

/* sound (extra) */
void I_UpdateSound(void) { }
void I_PrecacheSounds(void) { }

/* WAD checksum */
int W_Checksum(void *data, int len) { return 0; }
