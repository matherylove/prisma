/* editor.cpp - la parte "bloc de notas".
 *
 * Se usa el control EDIT estandar a proposito: es identico desde
 * Windows 95 hasta Windows 11. RichEdit cambia de DLL segun version
 * (riched32 / riched20 / msftedit) y no vale la pena todavia.
 */
#include "common.h"
#include "renderer.h"
#include "glloader.h"
#include <commdlg.h>

#define IDC_CODE   1000
#define IDC_LOG    1001

#define IDM_NEW     100
#define IDM_OPEN    101
#define IDM_SAVE    102
#define IDM_SAVEAS  103
#define IDM_EXIT    104
#define IDM_APPLY   200
#define IDM_STOP    201
#define IDM_RESET   202
#define IDM_SCALE1  300
#define IDM_SCALE2  301
#define IDM_SCALE3  302
#define IDM_SCALE4  303
#define IDM_FPS30   400
#define IDM_FPS60   401
#define IDM_FPSMAX  402
#define IDM_ABOUT   500
#define IDM_DIAG    501
#define IDM_REANCH  502
#define IDM_ANCH0   510
#define IDM_ANCH1   511
#define IDM_ANCH2   512
#define IDM_ANCH3   513
#define IDM_ANCH4   514
#define IDM_ANCH5   515

#define LOG_HEIGHT  150

static const char* MAIN_CLASS = "GLSLPaperMainWnd";

static HWND   g_main = NULL;
static HWND   g_code = NULL;
static HWND   g_log  = NULL;
static HFONT  g_font = NULL;
static HACCEL g_accel = NULL;
static char   g_path[MAX_PATH];

static const char* DEFAULT_SHADER =
    "// GLSLPaper - estilo Shadertoy, perfil GLSL 1.10\r\n"
    "// Uniforms: iResolution, iTime, iTimeDelta, iFrame, iMouse\r\n"
    "// Reglas del perfil 110:\r\n"
    "//   texture2D() en vez de texture(), sin in/out, floats con punto\r\n"
    "\r\n"
    "void mainImage(out vec4 fragColor, in vec2 fragCoord)\r\n"
    "{\r\n"
    "    vec2 uv = fragCoord / iResolution.xy;\r\n"
    "    vec2 p  = (uv - 0.5) * vec2(iResolution.x / iResolution.y, 1.0);\r\n"
    "\r\n"
    "    float t = iTime;\r\n"
    "    float v = sin(p.x * 8.0 + t) + sin(p.y * 8.0 - t * 0.7);\r\n"
    "    v += sin(length(p) * 12.0 - t * 2.0);\r\n"
    "\r\n"
    "    vec3 col = 0.5 + 0.5 * cos(vec3(0.0, 2.0, 4.0) + v * 1.5 + t * 0.2);\r\n"
    "    fragColor = vec4(col, 1.0);\r\n"
    "}\r\n";

/* ------------------------------------------------------------------ */
static void LogSet(const char* text)
{
    SetWindowTextA(g_log, text ? text : "");
}

static void UpdateTitle(void)
{
    char buf[MAX_PATH + 96];
    const char* name = g_path[0] ? g_path : "(sin titulo)";
    if (RendererIsRunning()) {
        wsprintfA(buf, "GLSLPaper - %s  [activo  %d fps  escala 1/%d]",
                  name, RendererGetFps(), RendererGetScale());
    } else {
        wsprintfA(buf, "GLSLPaper - %s  [detenido]", name);
    }
    SetWindowTextA(g_main, buf);
}

static void SyncMenu(void)
{
    HMENU m = GetMenu(g_main);
    int s = RendererGetScale();
    int f = RendererGetFpsCap();
    CheckMenuRadioItem(m, IDM_SCALE1, IDM_SCALE4,
                       IDM_SCALE1 + (s == 1 ? 0 : s == 2 ? 1 : s == 3 ? 2 : 3),
                       MF_BYCOMMAND);
    CheckMenuRadioItem(m, IDM_FPS30, IDM_FPSMAX,
                       f == 30 ? IDM_FPS30 : f == 60 ? IDM_FPS60 : IDM_FPSMAX,
                       MF_BYCOMMAND);
    CheckMenuRadioItem(m, IDM_ANCH0, IDM_ANCH5,
                       IDM_ANCH0 + DesktopGetAnchorMode(), MF_BYCOMMAND);
}

static char* GetCodeText(void)
{
    int   len = GetWindowTextLengthA(g_code);
    char* buf = (char*)MemAlloc(len + 2);
    if (!buf) return NULL;
    GetWindowTextA(g_code, buf, len + 1);
    return buf;
}

/* ------------------------------------------------------------------ */
static DWORD OfnSize(void)
{
    /* Windows 9x/NT4 rechazan la estructura ampliada de Windows 2000+. */
    if (OsShellGeneration() == SHELL_9X) {
#ifdef OPENFILENAME_SIZE_VERSION_400A
        return OPENFILENAME_SIZE_VERSION_400A;
#else
        return 76;
#endif
    }
    return (DWORD)sizeof(OPENFILENAMEA);
}

static BOOL AskPath(BOOL saving)
{
    OPENFILENAMEA ofn;
    char file[MAX_PATH];

    file[0] = 0;
    if (g_path[0]) StrCopy(file, g_path);

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = OfnSize();
    ofn.hwndOwner   = g_main;
    ofn.lpstrFilter = "Shaders GLSL (*.frag;*.glsl)\0*.frag;*.glsl\0Todos (*.*)\0*.*\0";
    ofn.lpstrFile   = file;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrDefExt = "frag";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
                (saving ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    if (saving ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn)) {
        StrCopy(g_path, file);
        return TRUE;
    }
    return FALSE;
}

static void DoSave(BOOL forceAsk)
{
    char* text;
    if (forceAsk || !g_path[0]) {
        if (!AskPath(TRUE)) return;
    }
    text = GetCodeText();
    if (!text) return;

    if (FileWriteAll(g_path, text)) {
        LogSet("Guardado.\r\n");
    } else {
        LogSet("ERROR: no se pudo escribir el fichero.\r\n");
    }
    MemFree(text);
    UpdateTitle();
}

static void DoOpen(void)
{
    char* text;
    if (!AskPath(FALSE)) return;
    text = FileReadAll(g_path);
    if (!text) {
        LogSet("ERROR: no se pudo leer el fichero.\r\n");
        return;
    }
    SetWindowTextA(g_code, text);
    MemFree(text);
    LogSet("Fichero cargado. Pulsa F5 para aplicar.\r\n");
    UpdateTitle();
}

static void DoApply(void)
{
    char  err[2048];
    char  log[8192];
    char* text;

    if (!RendererIsRunning()) {
        err[0] = 0;
        if (!RendererStart(err, sizeof(err))) {
            LogSet(err[0] ? err : "No se pudo iniciar el renderer.\r\n");
            return;
        }
    }

    text = GetCodeText();
    if (!text) return;

    log[0] = 0;
    RendererApplyShader(text, log, sizeof(log));
    MemFree(text);

    LogSet(log);
    SyncMenu();
    UpdateTitle();
}

static void DoStop(void)
{
    RendererStop();
    LogSet("Renderer detenido. El fondo vuelve al del sistema.\r\n");
    UpdateTitle();
}

static void DoAbout(void)
{
    char buf[1024];
    wsprintfA(buf,
        "GLSLPaper 0.1\r\n\r\n"
        "Fondos de pantalla animados con GLSL.\r\n"
        "Shell detectado: %s\r\n"
        "Ruta GLSL: %s\r\n\r\n"
        "Perfil objetivo: GLSL 1.10 (OpenGL 2.0).\r\n"
        "Minimo probado: GeForce 6xxx + ForceWare 81.98 sobre 98SE.",
        OsShellName(),
        RendererIsRunning()
            ? (gl.usedArbPath ? "extensiones ARB" : "OpenGL 2.0 core")
            : "(aun no iniciado)");
    MessageBoxA(g_main, buf, "Acerca de GLSLPaper", MB_OK | MB_ICONINFORMATION);
}

static void DoDiagnostics(void)
{
    char buf[4096];
    DesktopDiagnostics(buf, sizeof(buf), RendererGetWindow());
    LogSet(buf);
}

/* mode < 0 significa "conserva el modo actual, solo recoloca".
 *
 * Cambiar de modo obliga a recrear la ventana: el padre y WS_CHILD solo
 * se pueden fijar en CreateWindowEx. Por eso paramos y reaplicamos. */
static void DoReanchor(int mode)
{
    BOOL wasRunning = RendererIsRunning();

    if (mode >= 0 && mode != DesktopGetAnchorMode()) {
        DesktopSetAnchorMode(mode);
        if (wasRunning) {
            RendererStop();
            DoApply();          /* recrea la ventana con el nuevo padre */
        }
    } else if (wasRunning) {
        RendererReanchor();
    }

    if (!RendererIsRunning()) {
        LogSet("Modo cambiado. Pulsa F5 para aplicar.\r\n");
    } else {
        char buf[4096];
        DesktopDiagnostics(buf, sizeof(buf), RendererGetWindow());
        LogSet(buf);
    }
    SyncMenu();
}

/* ------------------------------------------------------------------ */
static HMENU BuildMenu(void)
{
    HMENU bar  = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU sh   = CreatePopupMenu();
    HMENU view = CreatePopupMenu();
    HMENU desk = CreatePopupMenu();
    HMENU help = CreatePopupMenu();

    AppendMenuA(file, MF_STRING, IDM_NEW,    "&Nuevo\tCtrl+N");
    AppendMenuA(file, MF_STRING, IDM_OPEN,   "&Abrir...\tCtrl+O");
    AppendMenuA(file, MF_STRING, IDM_SAVE,   "&Guardar\tCtrl+S");
    AppendMenuA(file, MF_STRING, IDM_SAVEAS, "Guardar &como...");
    AppendMenuA(file, MF_SEPARATOR, 0, NULL);
    AppendMenuA(file, MF_STRING, IDM_EXIT,   "&Salir");

    AppendMenuA(sh, MF_STRING, IDM_APPLY, "&Aplicar\tF5");
    AppendMenuA(sh, MF_STRING, IDM_STOP,  "&Detener");
    AppendMenuA(sh, MF_STRING, IDM_RESET, "&Reiniciar tiempo\tF6");

    AppendMenuA(view, MF_STRING, IDM_SCALE1, "Escala 1/&1 (nativa)");
    AppendMenuA(view, MF_STRING, IDM_SCALE2, "Escala 1/&2");
    AppendMenuA(view, MF_STRING, IDM_SCALE3, "Escala 1/&3");
    AppendMenuA(view, MF_STRING, IDM_SCALE4, "Escala 1/&4");
    AppendMenuA(view, MF_SEPARATOR, 0, NULL);
    AppendMenuA(view, MF_STRING, IDM_FPS30,  "Limite 30 fps");
    AppendMenuA(view, MF_STRING, IDM_FPS60,  "Limite 60 fps");
    AppendMenuA(view, MF_STRING, IDM_FPSMAX, "Sin limite");

    AppendMenuA(desk, MF_STRING, IDM_REANCH, "&Reanclar al escritorio\tF7");
    AppendMenuA(desk, MF_STRING, IDM_DIAG,   "&Diagnostico\tF8");
    AppendMenuA(desk, MF_SEPARATOR, 0, NULL);
    AppendMenuA(desk, MF_STRING, IDM_ANCH0, "Modo &auto");
    AppendMenuA(desk, MF_STRING, IDM_ANCH1, "Modo &1: WorkerW de nivel superior");
    AppendMenuA(desk, MF_STRING, IDM_ANCH2, "Modo &2: WorkerW hija de Progman");
    AppendMenuA(desk, MF_STRING, IDM_ANCH3, "Modo &3: Progman, debajo de DefView");
    AppendMenuA(desk, MF_STRING, IDM_ANCH4, "Modo &4: Progman, al fondo del todo");
    AppendMenuA(desk, MF_STRING, IDM_ANCH5, "Modo &5: sin padre (tapa iconos)");

    AppendMenuA(help, MF_STRING, IDM_ABOUT, "&Acerca de...");

    AppendMenuA(bar, MF_POPUP, (UINT_PTR)file, "&Archivo");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)sh,   "&Shader");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)view, "&Ver");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)desk, "&Escritorio");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)help, "A&yuda");
    return bar;
}

static void Layout(HWND h)
{
    RECT rc;
    int  logH = LOG_HEIGHT;
    if (!g_code || !g_log) return;
    GetClientRect(h, &rc);
    if (rc.bottom < logH * 2) logH = rc.bottom / 3;

    MoveWindow(g_code, 0, 0, rc.right, rc.bottom - logH, TRUE);
    MoveWindow(g_log,  0, rc.bottom - logH, rc.right, logH, TRUE);
}

static LRESULT CALLBACK MainWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE: {
        LOGFONTA lf;
        ZeroMemory(&lf, sizeof(lf));
        lf.lfHeight = -13;
        lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
        StrCopy(lf.lfFaceName, "Courier New");
        g_font = CreateFontIndirectA(&lf);

        g_code = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            0, 0, 10, 10, h, (HMENU)IDC_CODE, GetModuleHandleA(NULL), NULL);

        g_log = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, 0, 10, 10, h, (HMENU)IDC_LOG, GetModuleHandleA(NULL), NULL);

        SendMessageA(g_code, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageA(g_log,  WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageA(g_code, EM_LIMITTEXT, 1024 * 512, 0);
        SetWindowTextA(g_code, DEFAULT_SHADER);
        LogSet("Listo. F5 para aplicar el shader al escritorio.\r\n");
        return 0;
    }

    case WM_SIZE:
        Layout(h);
        return 0;

    case WM_SETFOCUS:
        SetFocus(g_code);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDM_NEW:
            g_path[0] = 0;
            SetWindowTextA(g_code, DEFAULT_SHADER);
            UpdateTitle();
            return 0;
        case IDM_OPEN:   DoOpen();       return 0;
        case IDM_SAVE:   DoSave(FALSE);  return 0;
        case IDM_SAVEAS: DoSave(TRUE);   return 0;
        case IDM_EXIT:   PostMessageA(h, WM_CLOSE, 0, 0); return 0;
        case IDM_APPLY:  DoApply();      return 0;
        case IDM_STOP:   DoStop();       return 0;
        case IDM_RESET:  RendererResetTime(); return 0;
        case IDM_SCALE1: RendererSetScale(1); SyncMenu(); UpdateTitle(); return 0;
        case IDM_SCALE2: RendererSetScale(2); SyncMenu(); UpdateTitle(); return 0;
        case IDM_SCALE3: RendererSetScale(3); SyncMenu(); UpdateTitle(); return 0;
        case IDM_SCALE4: RendererSetScale(4); SyncMenu(); UpdateTitle(); return 0;
        case IDM_FPS30:  RendererSetFpsCap(30); SyncMenu(); return 0;
        case IDM_FPS60:  RendererSetFpsCap(60); SyncMenu(); return 0;
        case IDM_FPSMAX: RendererSetFpsCap(0);  SyncMenu(); return 0;
        case IDM_ABOUT:  DoAbout();      return 0;
        case IDM_DIAG:   DoDiagnostics(); return 0;
        case IDM_REANCH: DoReanchor(-1);  return 0;
        case IDM_ANCH0:  DoReanchor(ANCHOR_AUTO);          return 0;
        case IDM_ANCH1:  DoReanchor(ANCHOR_WORKERW_SIBLING); return 0;
        case IDM_ANCH2:  DoReanchor(ANCHOR_WORKERW_CHILD);   return 0;
        case IDM_ANCH3:  DoReanchor(ANCHOR_UNDER_DEFVIEW);   return 0;
        case IDM_ANCH4:  DoReanchor(ANCHOR_PROGMAN_BOTTOM);  return 0;
        case IDM_ANCH5:  DoReanchor(ANCHOR_NONE);            return 0;
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(h);
        return 0;

    case WM_DESTROY:
        RendererStop();
        if (g_font) DeleteObject(g_font);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

/* ------------------------------------------------------------------ */
BOOL EditorCreate(HINSTANCE inst, int cmdShow)
{
    WNDCLASSA wc;
    ACCEL     acc[7];

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = MAIN_CLASS;
    if (!RegisterClassA(&wc)) return FALSE;

    g_path[0] = 0;
    g_main = CreateWindowExA(0, MAIN_CLASS, "GLSLPaper",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 720, 620,
        NULL, BuildMenu(), inst, NULL);
    if (!g_main) return FALSE;

    acc[0].fVirt = FVIRTKEY;              acc[0].key = VK_F5; acc[0].cmd = IDM_APPLY;
    acc[1].fVirt = FVIRTKEY;              acc[1].key = VK_F6; acc[1].cmd = IDM_RESET;
    acc[2].fVirt = FVIRTKEY | FCONTROL;   acc[2].key = 'S';   acc[2].cmd = IDM_SAVE;
    acc[3].fVirt = FVIRTKEY | FCONTROL;   acc[3].key = 'O';   acc[3].cmd = IDM_OPEN;
    acc[4].fVirt = FVIRTKEY | FCONTROL;   acc[4].key = 'N';   acc[4].cmd = IDM_NEW;
    acc[5].fVirt = FVIRTKEY;              acc[5].key = VK_F7; acc[5].cmd = IDM_REANCH;
    acc[6].fVirt = FVIRTKEY;              acc[6].key = VK_F8; acc[6].cmd = IDM_DIAG;
    g_accel = CreateAcceleratorTableA(acc, 7);

    SyncMenu();
    UpdateTitle();
    ShowWindow(g_main, cmdShow);
    UpdateWindow(g_main);
    return TRUE;
}

HWND   EditorGetHwnd(void)  { return g_main; }
HACCEL EditorGetAccel(void) { return g_accel; }

void EditorRefreshStatus(void) { UpdateTitle(); }
