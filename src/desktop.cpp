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

static HWND g_workerSibling;   /* WorkerW de nivel superior, hermano de DefView */
static HWND g_defViewHost;     /* la ventana que contiene SHELLDLL_DefView */
static HWND g_progmanWorker;   /* WorkerW HIJA de Progman (Windows 11) */
static HWND g_defView;         /* el propio SHELLDLL_DefView */
static HWND g_insertAfter;     /* posicion Z resuelta junto con el host */

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
    g_progmanWorker = NULL;
    g_defView       = NULL;

    if (OsShellGeneration() >= SHELL_WIN8) {
        PokeExplorer(progman);
        /* Explorer crea la capa de forma asincrona; sin esta pausa la
           buscamos antes de que exista. */
        Sleep(120);
    }

    EnumWindows(EnumProcScan, 0);

    /* En Windows 11 la WorkerW de fondo puede ser HIJA de Progman, y
       entonces EnumWindows (que solo ve ventanas de nivel superior) no
       la encuentra nunca. */
    if (progman) {
        HWND w = FindWindowExA(progman, NULL, "WorkerW", NULL);
        while (w) {
            if (IsUsableHost(w)) { g_progmanWorker = w; break; }
            w = FindWindowExA(progman, w, "WorkerW", NULL);
        }
        g_defView = FindWindowExA(progman, NULL, "SHELLDLL_DefView", NULL);
    }
}

/* ------------------------------------------------------------------ */
int DesktopEffectiveMode(void)
{
    if (g_anchorMode != ANCHOR_AUTO) return g_anchorMode;
    return (OsShellGeneration() >= SHELL_WIN8)
         ? ANCHOR_WORKERW_SIBLING : ANCHOR_UNDER_DEFVIEW;
}

HWND DesktopGetInsertAfter(void) { return g_insertAfter; }

HWND DesktopResolveHost(int mode)
{
    HWND progman;

    g_insertAfter = HWND_BOTTOM;

    if (mode == ANCHOR_AUTO) mode = DesktopEffectiveMode();
    if (mode == ANCHOR_NONE) return NULL;

    progman = FindWindowA("Progman", NULL);
    ScanDesktop();

    switch (mode) {
    case ANCHOR_WORKERW_SIBLING:
        if (IsUsableHost(g_workerSibling)) return g_workerSibling;
        break;
    case ANCHOR_WORKERW_CHILD:
        if (IsUsableHost(g_progmanWorker)) return g_progmanWorker;
        break;
    case ANCHOR_UNDER_DEFVIEW:
        if (IsUsableHost(progman)) {
            /* Justo por debajo de los iconos, pero por ENCIMA de
               cualquier WorkerW hija que este pintando el fondo.
               Con HWND_BOTTOM quedariamos debajo de ella y por eso
               no se veia nada. */
            if (g_defView) g_insertAfter = g_defView;
            return progman;
        }
        break;
    case ANCHOR_PROGMAN_BOTTOM:
        if (IsUsableHost(progman)) return progman;
        break;
    }

    /* Degradacion ordenada */
    if (IsUsableHost(g_workerSibling)) return g_workerSibling;
    if (IsUsableHost(g_progmanWorker)) return g_progmanWorker;
    if (IsUsableHost(progman)) {
        if (g_defView) g_insertAfter = g_defView;
        return progman;
    }
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
void DesktopPlace(HWND hwnd, HWND host, HWND insertAfter)
{
    RECT r;
    DesktopGetVirtualRect(&r);

    if (host) {
        if (!insertAfter) insertAfter = HWND_BOTTOM;
        SetWindowPos(hwnd, insertAfter, 0, 0,
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
        case ANCHOR_WORKERW_SIBLING: return "WorkerW de nivel superior";
        case ANCHOR_WORKERW_CHILD:   return "WorkerW hija de Progman";
        case ANCHOR_UNDER_DEFVIEW:   return "Progman, justo debajo de DefView";
        case ANCHOR_PROGMAN_BOTTOM:  return "Progman, al fondo del todo";
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

    StrAppendSafe(out, cap, "\r\nHijos directos de Progman (orden Z):\r\n");
    {
        HWND pm = FindWindowA("Progman", NULL);
        HWND c  = pm ? FindWindowExA(pm, NULL, NULL, NULL) : NULL;
        char cls[64], tmp[256];
        RECT cr;
        int  n = 0;
        while (c && n < 24) {
            cls[0] = 0;
            GetClassNameA(c, cls, sizeof(cls));
            cr.right = cr.bottom = 0;
            GetClientRect(c, &cr);
            wsprintfA(tmp, "  0x%08X %-18s %4dx%-4d vis=%d",
                      (DWORD)(UINT_PTR)c, cls, (int)cr.right, (int)cr.bottom,
                      IsWindowVisible(c) ? 1 : 0);
            StrAppendSafe(out, cap, tmp);
            StrAppendSafe(out, cap, "\r\n");
            c = FindWindowExA(pm, c, NULL, NULL);
            n++;
        }
        if (n == 0) StrAppendSafe(out, cap, "  (ninguno)\r\n");
    }

    StrAppendSafe(out, cap, "\r\n");
    Line(out, cap, "WorkerW nivel sup. : 0x%08X",
         (DWORD)(UINT_PTR)g_workerSibling, 0, 0, 0);
    Line(out, cap, "WorkerW de Progman : 0x%08X",
         (DWORD)(UINT_PTR)g_progmanWorker, 0, 0, 0);
    Line(out, cap, "SHELLDLL_DefView   : 0x%08X",
         (DWORD)(UINT_PTR)g_defView, 0, 0, 0);

    host = DesktopResolveHost(DesktopGetAnchorMode());
    Line(out, cap, "Host elegido       : 0x%08X",
         (DWORD)(UINT_PTR)host, 0, 0, 0);
    Line(out, cap, "Insertar tras      : 0x%08X",
         (DWORD)(UINT_PTR)DesktopGetInsertAfter(), 0, 0, 0);

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
        "\r\nSi la ventana esta bien colocada y aun asi no se ve, algo opaco\r\n"
        "la tapa: mira que hijos de Progman quedan por encima.\r\n");
}
