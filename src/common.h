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
HWND DesktopFindWallpaperHost(void);
void DesktopAnchor(HWND hwnd);
void DesktopGetVirtualRect(RECT* out);

/* ------------------------------------------------------------------ */
/* Editor (editor.cpp)                                                 */
/* ------------------------------------------------------------------ */
BOOL EditorCreate(HINSTANCE inst, int cmdShow);
HWND EditorGetHwnd(void);
HACCEL EditorGetAccel(void);
void EditorRefreshStatus(void);

#endif /* GLSLPAPER_COMMON_H */
