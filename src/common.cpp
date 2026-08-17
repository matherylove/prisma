#include "common.h"

/* ------------------------------------------------------------------ */
void* MemAlloc(int bytes)
{
    if (bytes <= 0) bytes = 1;
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)bytes);
}

void MemFree(void* p)
{
    if (p) HeapFree(GetProcessHeap(), 0, p);
}

int StrLen(const char* s)
{
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

void StrCopy(char* dst, const char* src)
{
    while ((*dst++ = *src++) != 0) {}
}

void StrCat(char* dst, const char* src)
{
    while (*dst) dst++;
    while ((*dst++ = *src++) != 0) {}
}

int StrFind(const char* hay, const char* needle)
{
    int i, j;
    if (!hay || !needle || !needle[0]) return -1;
    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j]; j++) {
            if (hay[i + j] != needle[j]) break;
        }
        if (!needle[j]) return i;
    }
    return -1;
}

char* StrDup(const char* s)
{
    int   n = StrLen(s);
    char* p = (char*)MemAlloc(n + 1);
    if (p) StrCopy(p, s ? s : "");
    return p;
}

void StrAppendSafe(char* dst, int cap, const char* src)
{
    int used, room, i = 0;
    if (!dst || !src || cap <= 1) return;
    used = StrLen(dst);
    room = cap - used - 1;
    if (room <= 0) return;
    while (src[i] && i < room) { dst[used + i] = src[i]; i++; }
    dst[used + i] = 0;
}

/* ------------------------------------------------------------------ */
char* FileReadAll(const char* path)
{
    HANDLE h;
    DWORD  size, got = 0;
    char*  buf;

    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;

    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size > (16 * 1024 * 1024)) {
        CloseHandle(h);
        return NULL;
    }

    buf = (char*)MemAlloc((int)size + 1);
    if (!buf) { CloseHandle(h); return NULL; }

    if (!ReadFile(h, buf, size, &got, NULL)) {
        MemFree(buf);
        CloseHandle(h);
        return NULL;
    }
    buf[got] = 0;
    CloseHandle(h);
    return buf;
}

BOOL FileWriteAll(const char* path, const char* data)
{
    HANDLE h;
    DWORD  written = 0;
    DWORD  len = (DWORD)StrLen(data);

    h = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;

    WriteFile(h, data, len, &written, NULL);
    CloseHandle(h);
    return (written == len);
}

/* ------------------------------------------------------------------ */
/* GetVersionExA sin manifest devuelve 6.2 en Windows 10/11, que es    */
/* justo lo que necesitamos: solo distinguimos "8 o superior".         */
/* ------------------------------------------------------------------ */
static int g_shellGen = -1;

int OsShellGeneration(void)
{
    OSVERSIONINFOA vi;

    if (g_shellGen >= 0) return g_shellGen;

    ZeroMemory(&vi, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    GetVersionExA(&vi);

    if (vi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        g_shellGen = SHELL_9X;
    } else if (vi.dwMajorVersion < 6) {
        g_shellGen = SHELL_NT_XP;
    } else if (vi.dwMajorVersion == 6 && vi.dwMinorVersion < 2) {
        g_shellGen = SHELL_VISTA_7;
    } else {
        g_shellGen = SHELL_WIN8;
    }
    return g_shellGen;
}

const char* OsShellName(void)
{
    switch (OsShellGeneration()) {
        case SHELL_9X:      return "Windows 9x/ME";
        case SHELL_NT_XP:   return "Windows NT/2000/XP";
        case SHELL_VISTA_7: return "Windows Vista/7";
        default:            return "Windows 8 o superior";
    }
}
