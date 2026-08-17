/* desktop.cpp - engancha la ventana de render por debajo de los iconos.
 *
 * 9x / NT / 2000 / XP / Vista / 7 : SetParent a "Progman", al fondo del
 *   Z-order para quedar detras de SHELLDLL_DefView (los iconos).
 *
 * 8 / 8.1 / 10 / 11 : hay que provocar que Explorer cree una ventana
 *   "WorkerW" con el mensaje no documentado 0x052C, y colgarse de la
 *   WorkerW que queda como hermana siguiente de la que contiene
 *   SHELLDLL_DefView.
 */
#include "common.h"

static HWND g_foundWorker = NULL;

static BOOL CALLBACK EnumProcFindWorker(HWND top, LPARAM /*lp*/)
{
    HWND defView = FindWindowExA(top, NULL, "SHELLDLL_DefView", NULL);
    if (defView != NULL) {
        /* la WorkerW hermana justo despues es la capa de fondo */
        HWND worker = FindWindowExA(NULL, top, "WorkerW", NULL);
        if (worker != NULL) {
            g_foundWorker = worker;
            return FALSE; /* parar */
        }
    }
    return TRUE;
}

HWND DesktopFindWallpaperHost(void)
{
    HWND  progman = FindWindowA("Progman", NULL);
    DWORD_PTR result = 0;

    if (OsShellGeneration() < SHELL_WIN8) {
        return progman; /* puede ser NULL si el shell no es Explorer */
    }

    if (progman) {
        /* Provoca la creacion de la capa WorkerW. Dos variantes vistas
           en la practica segun build de Windows 10/11. */
        SendMessageTimeoutA(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
        SendMessageTimeoutA(progman, 0x052C, 0x0000000D, 0x00000001,
                            SMTO_NORMAL, 1000, &result);
    }

    g_foundWorker = NULL;
    EnumWindows(EnumProcFindWorker, 0);

    if (g_foundWorker) return g_foundWorker;
    return progman;
}

void DesktopGetVirtualRect(RECT* out)
{
    /* SM_?VIRTUALSCREEN existe desde Windows 98; en NT4 devuelve 0 y
       caemos al tamano de la pantalla primaria. */
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (w <= 0 || h <= 0) {
        x = 0; y = 0;
        w = GetSystemMetrics(SM_CXSCREEN);
        h = GetSystemMetrics(SM_CYSCREEN);
    }
    out->left   = x;
    out->top    = y;
    out->right  = x + w;
    out->bottom = y + h;
}

void DesktopAnchor(HWND hwnd)
{
    HWND host = DesktopFindWallpaperHost();
    RECT r;

    DesktopGetVirtualRect(&r);

    if (host) {
        SetParent(hwnd, host);
        /* Coordenadas relativas al cliente del host: normalmente el
           origen del escritorio virtual cae en (0,0). */
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0,
                     r.right - r.left, r.bottom - r.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else {
        /* Fallback universal: ventana suelta al fondo del Z-order. */
        SetWindowPos(hwnd, HWND_BOTTOM, r.left, r.top,
                     r.right - r.left, r.bottom - r.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}
