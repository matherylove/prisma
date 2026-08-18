/* common.h - utilidades compartidas, sin CRT pesado.
 * Todo lo que se usa aqui existe desde Windows 95/98 en adelante.
 */
#ifndef GLSLPAPER_COMMON_H
#define GLSLPAPER_COMMON_H

#include <windows.h>

/* ------------------------------------------------------------------ */
/* Memoria y strings (evitamos la CRT para facilitar el paso a         */
/* -nostdlib mas adelante; ver README).                                */
/* ------------------------------------------------------------------ */
void* MemAlloc(int bytes);
void  MemFree(void* p);
int   StrLen(const char* s);
void  StrCopy(char* dst, const char* src);
void  StrCat(char* dst, const char* src);
int   StrFind(const char* hay, const char* needle);  /* -1 si no esta */
char* StrDup(const char* s);
void  StrAppendSafe(char* dst, int cap, const char* src);

/* ------------------------------------------------------------------ */
/* Ficheros (CreateFileA/ReadFile, nada de fopen)                      */
/* ------------------------------------------------------------------ */
char* FileReadAll(const char* path);                 /* MemFree() luego */
BOOL  FileWriteAll(const char* path, const char* data);

/* ------------------------------------------------------------------ */
/* Deteccion de generacion de shell                                    */
/* ------------------------------------------------------------------ */
enum ShellGen {
    SHELL_9X      = 0,   /* 95 / 98 / 98SE / ME            */
    SHELL_NT_XP   = 1,   /* NT4 / 2000 / XP / 2003         */
    SHELL_VISTA_7 = 2,   /* Vista / 7                      */
    SHELL_WIN8    = 3    /* 8 / 8.1 / 10 / 11              */
};
int  OsShellGeneration(void);
const char* OsShellName(void);

/* ------------------------------------------------------------------ */
/* Anclaje al escritorio (desktop.cpp)                                 */
/* ------------------------------------------------------------------ */
enum AnchorMode {
    ANCHOR_AUTO            = 0,
    ANCHOR_WORKERW_SIBLING = 1,  /* WorkerW de nivel superior, hermano de DefView */
    ANCHOR_WORKERW_CHILD   = 2,  /* WorkerW hija de Progman (Windows 11) */
    ANCHOR_UNDER_DEFVIEW   = 3,  /* hija de Progman, justo debajo de los iconos */
    ANCHOR_PROGMAN_BOTTOM  = 4,  /* hija de Progman, al fondo del todo */
    ANCHOR_NONE            = 5   /* sin padre: tapa iconos, solo emergencia */
};
HWND DesktopResolveHost(int mode);
HWND DesktopGetInsertAfter(void);
int  DesktopEffectiveMode(void);
void DesktopPlace(HWND hwnd, HWND host, HWND insertAfter);
void DesktopGetVirtualRect(RECT* out);
void DesktopSetAnchorMode(int mode);
int  DesktopGetAnchorMode(void);
const char* DesktopAnchorModeName(int mode);
void DesktopDiagnostics(char* out, int cap, HWND renderWnd);

/* ------------------------------------------------------------------ */
/* Editor (editor.cpp)                                                 */
/* ------------------------------------------------------------------ */
BOOL EditorCreate(HINSTANCE inst, int cmdShow);
HWND EditorGetHwnd(void);
HACCEL EditorGetAccel(void);
void EditorRefreshStatus(void);

#endif /* GLSLPAPER_COMMON_H */
