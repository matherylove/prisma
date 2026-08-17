/* desktop.cpp - engancha la ventana de render por debajo de los iconos.
 *
 * 9x / NT / 2000 / XP / Vista / 7 : SetParent a "Progman", al fondo del
 *   Z-order para quedar detras de SHELLDLL_DefView (los iconos).
 *
 * 8 / 8.1 / 10 / 11 : hay que provocar que Explorer cree una ventana
 *   "WorkerW" con el mensaje no documentado 0x052C, y colgarse de la
 *   WorkerW que queda como hermana siguiente de la que contiene
 *   SHELLDLL_DefView.
 *
 * Como el comportamiento de Windows 11 varia entre builds, el modo de
 * anclaje es conmutable en caliente desde el menu Escritorio.
 */
#include "common.h"

static int g_anchorMode = ANCHOR_AUTO;

/* ------------------------------------------------------------------ */
/* Busqueda del host                                                   */
/* ------------------------------------------------------------------ */
static HWND g_worker;
static HWND g_workerFromProgman;

static BOOL IsUsableHost(HWND h)
{
    RECT rc;
    if (!h) return FALSE;
    if (!GetClientRect(h, &rc)) return FALSE;
    /* Hay WorkerW de 0x0 (barra de tareas, etc). Un padre vacio recorta
       nuestra ventana a nada y por eso "no pasa nada" en pantalla. */
    return (rc.right > 16 && rc.bottom > 16);
}

static BOOL CALLBACK EnumProcFindWorker(HWND top, LPARAM lp)
{
    HWND progman = (HWND)lp;
    HWND defView = FindWindowExA(top, NULL, "SHELLDLL_DefView", NULL);

    if (defView != NULL) {
        HWND worker = FindWindowExA(NULL, top, "WorkerW", NULL);
        if (IsUsableHost(worker)) {
            if (top == progman) g_workerFromProgman = worker;
            g_worker = worker;
        }
    }
    return TRUE;  /* no paramos: preferimos la coincidencia de Progman */
}

static void PokeExplorer(HWND progman)
{
    DWORD_PTR result = 0;
    if (!progman) return;
    /* Variante Windows 11 primero, luego la clasica de 8/10. */
    SendMessageTimeoutA(progman, 0x052C, 0x0000000D, 0x00000001,
                        SMTO_NORMAL, 1000, &result);
    SendMessageTimeoutA(progman, 0x052C, 0x0000000D, 0x00000000,
                        SMTO_NORMAL, 1000, &result);
    SendMessageTimeoutA(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
}

HWND DesktopFindWallpaperHost(void)
{
    HWND progman = FindWindowA("Progman", NULL);

    if (OsShellGeneration() < SHELL_WIN8) return progman;

    PokeExplorer(progman);

    g_worker = NULL;
    g_workerFromProgman = NULL;
    EnumWindows(EnumProcFindWorker, (LPARAM)progman);

    if (g_workerFromProgman) return g_workerFromProgman;
    if (g_worker)            return g_worker;
    return progman;
}

/* ------------------------------------------------------------------ */
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
    out->left = x; out->top = y;
    out->right = x + w; out->bottom = y + h;
}

/* ------------------------------------------------------------------ */
void DesktopSetAnchorMode(int mode)
{
    if (mode < ANCHOR_AUTO || mode > ANCHOR_NONE) mode = ANCHOR_AUTO;
    g_anchorMode = mode;
}

int DesktopGetAnchorMode(void) { return g_anchorMode; }

const char* DesktopAnchorModeName(int mode)
{
    switch (mode) {
        case ANCHOR_WORKERW_CHILD: return "WorkerW + WS_CHILD";
        case ANCHOR_WORKERW_POPUP: return "WorkerW sin tocar estilo";
        case ANCHOR_PROGMAN:       return "Progman directo";
        case ANCHOR_NONE:          return "sin padre, al fondo";
        default:                   return "auto";
    }
}

void DesktopAnchor(HWND hwnd)
{
    HWND host = NULL;
    RECT r;
    int  mode = g_anchorMode;

    DesktopGetVirtualRect(&r);

    if (mode == ANCHOR_AUTO) {
        mode = (OsShellGeneration() >= SHELL_WIN8)
             ? ANCHOR_WORKERW_CHILD : ANCHOR_PROGMAN;
    }

    if (mode == ANCHOR_WORKERW_CHILD || mode == ANCHOR_WORKERW_POPUP) {
        host = DesktopFindWallpaperHost();
        if (!IsUsableHost(host)) host = FindWindowA("Progman", NULL);
    } else if (mode == ANCHOR_PROGMAN) {
        HWND progman = FindWindowA("Progman", NULL);
        if (OsShellGeneration() >= SHELL_WIN8) PokeExplorer(progman);
        host = progman;
    }

    if (host && IsUsableHost(host)) {
        SetParent(hwnd, host);

        if (mode == ANCHOR_WORKERW_CHILD) {
            /* Estrictamente correcto segun la documentacion: una ventana
               con padre debe ser WS_CHILD. En la practica algunas builds
               funcionan sin esto y otras no; de ahi el conmutador. */
            SetWindowLongA(hwnd, GWL_STYLE, (LONG)(WS_CHILD | WS_VISIBLE));
        }

        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0,
                     r.right - r.left, r.bottom - r.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    } else {
        /* Fallback universal: ventana suelta al fondo del Z-order. */
        SetParent(hwnd, NULL);
        SetWindowLongA(hwnd, GWL_STYLE, (LONG)(WS_POPUP | WS_VISIBLE));
        SetWindowPos(hwnd, HWND_BOTTOM, r.left, r.top,
                     r.right - r.left, r.bottom - r.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    }

    ShowWindow(hwnd, SW_SHOWNA);
    UpdateWindow(hwnd);
}

/* ------------------------------------------------------------------ */
/* Diagnostico                                                         */
/* ------------------------------------------------------------------ */
static int g_workerCount;

static BOOL CALLBACK EnumProcCount(HWND top, LPARAM lp)
{
    char cls[64];
    (void)lp;
    cls[0] = 0;
    GetClassNameA(top, cls, sizeof(cls));
    if (StrFind(cls, "WorkerW") == 0) g_workerCount++;
    return TRUE;
}

static void Line(char* out, int cap, const char* fmt,
                 DWORD a, DWORD b, DWORD c, DWORD d)
{
    char tmp[512];
    wsprintfA(tmp, fmt, a, b, c, d);
    StrAppendSafe(out, cap, tmp);
    StrAppendSafe(out, cap, "\r\n");
}

void DesktopDiagnostics(char* out, int cap, HWND renderWnd)
{
    HWND progman, host, parent;
    RECT rc;
    char cls[64];

    if (!out || cap <= 0) return;
    out[0] = 0;

    StrAppendSafe(out, cap, "=== DIAGNOSTICO DE ANCLAJE ===\r\n");
    StrAppendSafe(out, cap, "Shell: ");
    StrAppendSafe(out, cap, OsShellName());
    StrAppendSafe(out, cap, "\r\nModo: ");
    StrAppendSafe(out, cap, DesktopAnchorModeName(g_anchorMode));
    StrAppendSafe(out, cap, "\r\n\r\n");

    progman = FindWindowA("Progman", NULL);
    Line(out, cap, "Progman            : 0x%08X", (DWORD)(UINT_PTR)progman, 0, 0, 0);
    if (progman && GetClientRect(progman, &rc))
        Line(out, cap, "  cliente          : %d x %d",
             (DWORD)rc.right, (DWORD)rc.bottom, 0, 0);
    Line(out, cap, "  DefView hijo     : 0x%08X",
         (DWORD)(UINT_PTR)FindWindowExA(progman, NULL, "SHELLDLL_DefView", NULL), 0, 0, 0);

    g_workerCount = 0;
    EnumWindows(EnumProcCount, 0);
    Line(out, cap, "WorkerW de nivel 1 : %d", (DWORD)g_workerCount, 0, 0, 0);

    host = DesktopFindWallpaperHost();
    Line(out, cap, "Host elegido       : 0x%08X", (DWORD)(UINT_PTR)host, 0, 0, 0);
    if (host) {
        cls[0] = 0;
        GetClassNameA(host, cls, sizeof(cls));
        StrAppendSafe(out, cap, "  clase            : ");
        StrAppendSafe(out, cap, cls);
        StrAppendSafe(out, cap, "\r\n");
        if (GetClientRect(host, &rc))
            Line(out, cap, "  cliente          : %d x %d",
                 (DWORD)rc.right, (DWORD)rc.bottom, 0, 0);
        StrAppendSafe(out, cap, IsUsableHost(host)
            ? "  estado           : usable\r\n"
            : "  estado           : VACIO - este modo no sirve\r\n");
    }

    StrAppendSafe(out, cap, "\r\n");
    Line(out, cap, "Ventana render     : 0x%08X", (DWORD)(UINT_PTR)renderWnd, 0, 0, 0);
    if (renderWnd) {
        parent = GetParent(renderWnd);
        Line(out, cap, "  padre real       : 0x%08X", (DWORD)(UINT_PTR)parent, 0, 0, 0);
        Line(out, cap, "  estilo           : 0x%08X  WS_CHILD=%d WS_VISIBLE=%d",
             (DWORD)GetWindowLongA(renderWnd, GWL_STYLE),
             (DWORD)((GetWindowLongA(renderWnd, GWL_STYLE) & WS_CHILD) ? 1 : 0),
             (DWORD)((GetWindowLongA(renderWnd, GWL_STYLE) & WS_VISIBLE) ? 1 : 0), 0);
        Line(out, cap, "  IsWindowVisible  : %d",
             (DWORD)(IsWindowVisible(renderWnd) ? 1 : 0), 0, 0, 0);
        if (GetWindowRect(renderWnd, &rc))
            Line(out, cap, "  rect pantalla    : %d,%d  %d x %d",
                 (DWORD)rc.left, (DWORD)rc.top,
                 (DWORD)(rc.right - rc.left), (DWORD)(rc.bottom - rc.top));
    }
    StrAppendSafe(out, cap,
        "\r\nSi el host esta VACIO o el rect es 0x0, el modo actual no "
        "sirve en\r\nesta build. Prueba otro en el menu Escritorio.\r\n");
}
