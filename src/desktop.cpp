/* desktop.cpp - localiza la ventana del escritorio que hara de padre.
 *
 * REGLA CLAVE: una ventana solo puede ser WS_CHILD si se crea asi, con
 * el padre pasado a CreateWindowEx. SetWindowLong NO permite anadir ni
 * quitar WS_CHILD despues (asi lo documenta Microsoft). Por eso aqui
 * solo *resolvemos* el host; quien crea la ventana es renderer.cpp.
 *
 * Estructura tipica del escritorio:
 *
 *   Windows 9x..7      Progman
 *                        └ SHELLDLL_DefView  (iconos)
 *
 *   Windows 8..11      WorkerW  (A)
 *                        └ SHELLDLL_DefView  (iconos)
 *                      WorkerW  (B)  <- hermano siguiente, capa de fondo
 *                      Progman
 *
 * Que la capa buena sea A o B varia entre builds de Windows 11, de ahi
 * el conmutador de modos.
 */
#include "common.h"

static int g_anchorMode = ANCHOR_AUTO;

static HWND g_workerSibling;   /* B: hermano siguiente al de DefView */
static HWND g_defViewHost;     /* A: la que contiene SHELLDLL_DefView */

/* ------------------------------------------------------------------ */
static BOOL IsUsableHost(HWND h)
{
    RECT rc;
    if (!h) return FALSE;
    if (!GetClientRect(h, &rc)) return FALSE;
    /* Hay WorkerW de 0x0 (barra de tareas, etc). Un padre vacio recorta
       nuestra ventana a nada. */
    return (rc.right > 16 && rc.bottom > 16);
}

static void PokeExplorer(HWND progman)
{
    DWORD_PTR result = 0;
    if (!progman) return;
    /* Mensaje no documentado que hace a Explorer crear la capa WorkerW.
       Variante de Windows 11 primero, luego las clasicas de 8/10. */
    SendMessageTimeoutA(progman, 0x052C, 0x0000000D, 0x00000001,
                        SMTO_NORMAL, 1000, &result);
    SendMessageTimeoutA(progman, 0x052C, 0x0000000D, 0x00000000,
                        SMTO_NORMAL, 1000, &result);
    SendMessageTimeoutA(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
}

static BOOL CALLBACK EnumProcScan(HWND top, LPARAM lp)
{
    HWND defView = FindWindowExA(top, NULL, "SHELLDLL_DefView", NULL);
    (void)lp;

    if (defView != NULL) {
        HWND sibling = FindWindowExA(NULL, top, "WorkerW", NULL);
        if (IsUsableHost(top))     g_defViewHost   = top;
        if (IsUsableHost(sibling)) g_workerSibling = sibling;
    }
    return TRUE;  /* recorremos todo, no paramos en la primera */
}

static void ScanDesktop(void)
{
    HWND progman = FindWindowA("Progman", NULL);
    g_workerSibling = NULL;
    g_defViewHost   = NULL;
    if (OsShellGeneration() >= SHELL_WIN8) PokeExplorer(progman);
    EnumWindows(EnumProcScan, 0);
}

/* ------------------------------------------------------------------ */
int DesktopEffectiveMode(void)
{
    if (g_anchorMode != ANCHOR_AUTO) return g_anchorMode;
    return (OsShellGeneration() >= SHELL_WIN8)
         ? ANCHOR_WORKERW_SIBLING : ANCHOR_PROGMAN;
}

HWND DesktopResolveHost(int mode)
{
    HWND progman;

    if (mode == ANCHOR_AUTO) mode = DesktopEffectiveMode();
    if (mode == ANCHOR_NONE) return NULL;

    progman = FindWindowA("Progman", NULL);

    if (mode == ANCHOR_PROGMAN) {
        if (OsShellGeneration() >= SHELL_WIN8) PokeExplorer(progman);
        return IsUsableHost(progman) ? progman : NULL;
    }

    ScanDesktop();

    if (mode == ANCHOR_DEFVIEW_HOST && IsUsableHost(g_defViewHost))
        return g_defViewHost;
    if (mode == ANCHOR_WORKERW_SIBLING && IsUsableHost(g_workerSibling))
        return g_workerSibling;

    /* Degradacion ordenada: hermano -> host de DefView -> Progman */
    if (IsUsableHost(g_workerSibling)) return g_workerSibling;
    if (IsUsableHost(g_defViewHost))   return g_defViewHost;
    if (IsUsableHost(progman))         return progman;
    return NULL;
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
    out->left = x; out->top = y;
    out->right = x + w; out->bottom = y + h;
}

/* Coloca la ventana ya creada: al fondo del Z-order, sin activarla.
   Si tiene padre, las coordenadas son relativas a su area cliente. */
void DesktopPlace(HWND hwnd, HWND host)
{
    RECT r;
    DesktopGetVirtualRect(&r);

    if (host) {
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0,
                     r.right - r.left, r.bottom - r.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else {
        SetWindowPos(hwnd, HWND_BOTTOM, r.left, r.top,
                     r.right - r.left, r.bottom - r.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    UpdateWindow(hwnd);
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
        case ANCHOR_WORKERW_SIBLING: return "WorkerW hermano de DefView";
        case ANCHOR_DEFVIEW_HOST:    return "ventana que contiene DefView";
        case ANCHOR_PROGMAN:         return "Progman";
        case ANCHOR_NONE:            return "sin padre (tapa iconos)";
        default:                     return "auto";
    }
}

/* ------------------------------------------------------------------ */
/* Diagnostico                                                         */
/* ------------------------------------------------------------------ */
static char* g_treeBuf;
static int   g_treeCap;

static BOOL CALLBACK EnumProcTree(HWND top, LPARAM lp)
{
    char cls[64], tmp[256];
    RECT rc;
    HWND dv;
    (void)lp;

    cls[0] = 0;
    GetClassNameA(top, cls, sizeof(cls));
    if (StrFind(cls, "WorkerW") != 0 && StrFind(cls, "Progman") != 0)
        return TRUE;

    rc.right = rc.bottom = 0;
    GetClientRect(top, &rc);
    dv = FindWindowExA(top, NULL, "SHELLDLL_DefView", NULL);

    wsprintfA(tmp, "  0x%08X %-8s %4dx%-4d DefView=%s vis=%d",
              (DWORD)(UINT_PTR)top, cls, (int)rc.right, (int)rc.bottom,
              dv ? "SI" : "no", IsWindowVisible(top) ? 1 : 0);
    StrAppendSafe(g_treeBuf, g_treeCap, tmp);
    StrAppendSafe(g_treeBuf, g_treeCap, "\r\n");
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
    HWND host, parent;
    RECT rc;
    LONG style;

    if (!out || cap <= 0) return;
    out[0] = 0;

    StrAppendSafe(out, cap, "=== DIAGNOSTICO DE ANCLAJE ===\r\nShell: ");
    StrAppendSafe(out, cap, OsShellName());
    StrAppendSafe(out, cap, "\r\nModo: ");
    StrAppendSafe(out, cap, DesktopAnchorModeName(DesktopEffectiveMode()));
    StrAppendSafe(out, cap, "\r\n\r\nVentanas del escritorio (orden Z):\r\n");

    g_treeBuf = out; g_treeCap = cap;
    EnumWindows(EnumProcTree, 0);
    g_treeBuf = NULL;

    ScanDesktop();
    StrAppendSafe(out, cap, "\r\n");
    Line(out, cap, "WorkerW hermano    : 0x%08X",
         (DWORD)(UINT_PTR)g_workerSibling, 0, 0, 0);
    Line(out, cap, "Host de DefView    : 0x%08X",
         (DWORD)(UINT_PTR)g_defViewHost, 0, 0, 0);

    host = DesktopResolveHost(ANCHOR_AUTO);
    Line(out, cap, "Host elegido       : 0x%08X",
         (DWORD)(UINT_PTR)host, 0, 0, 0);

    StrAppendSafe(out, cap, "\r\n");
    Line(out, cap, "Ventana render     : 0x%08X",
         (DWORD)(UINT_PTR)renderWnd, 0, 0, 0);
    if (renderWnd) {
        parent = GetParent(renderWnd);
        style  = GetWindowLongA(renderWnd, GWL_STYLE);
        Line(out, cap, "  padre real       : 0x%08X",
             (DWORD)(UINT_PTR)parent, 0, 0, 0);
        Line(out, cap, "  WS_CHILD=%d  WS_VISIBLE=%d  IsWindowVisible=%d",
             (DWORD)((style & WS_CHILD) ? 1 : 0),
             (DWORD)((style & WS_VISIBLE) ? 1 : 0),
             (DWORD)(IsWindowVisible(renderWnd) ? 1 : 0), 0);
        if (GetWindowRect(renderWnd, &rc))
            Line(out, cap, "  rect pantalla    : %d,%d  %d x %d",
                 (DWORD)rc.left, (DWORD)rc.top,
                 (DWORD)(rc.right - rc.left), (DWORD)(rc.bottom - rc.top));
    }
    StrAppendSafe(out, cap,
        "\r\nSi WS_CHILD=0 con padre distinto de 0, la ventana se creo mal:\r\n"
        "WS_CHILD hay que ponerlo en CreateWindowEx, no despues.\r\n");
}
