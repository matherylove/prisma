/* renderer.cpp - contexto WGL clasico + pipeline GLSL 1.10.
 *
 * Deliberadamente usa el perfil de compatibilidad y modo inmediato
 * (glBegin/glEnd): es lo unico que funciona igual en un driver de 2005
 * y en uno de 2026 sin escribir dos rutas distintas.
 */
#include "renderer.h"
#include "glloader.h"
#include "common.h"
#include <mmsystem.h>

#ifndef WS_EX_NOACTIVATE
#define WS_EX_NOACTIVATE 0x08000000L
#endif

static const char* RENDER_CLASS = "GLSLPaperRenderWnd";

static HWND      g_wnd  = NULL;
static HWND      g_host = NULL;
static HDC       g_dc   = NULL;
static HGLRC     g_rc   = NULL;
static GLhandle  g_prog = 0;
static GLuint    g_tex  = 0;
static int       g_texW = 0, g_texH = 0;

static int   g_width = 0, g_height = 0;
static int   g_scale = 1;
static int   g_fpsCap = 30;
static int   g_fpsShown = 0;

static DWORD g_startMs = 0;
static DWORD g_lastMs  = 0;
static DWORD g_fpsAccMs = 0;
static int   g_fpsAccFrames = 0;
static int   g_frame = 0;

/* localizaciones de uniforms */
static GLint u_res, u_time, u_delta, u_frame, u_mouse, u_globalTime;

/* ------------------------------------------------------------------ */
/* Fuentes fijas                                                       */
/* ------------------------------------------------------------------ */
static const char* VS_SRC =
    "#version 110\n"
    "void main() {\n"
    "    gl_TexCoord[0] = gl_MultiTexCoord0;\n"
    "    gl_Position = gl_Vertex;\n"
    "}\n";

static const char* FS_PREFIX =
    "#version 110\n"
    "uniform vec3  iResolution;\n"
    "uniform float iTime;\n"
    "uniform float iGlobalTime;\n"
    "uniform float iTimeDelta;\n"
    "uniform int   iFrame;\n"
    "uniform vec4  iMouse;\n"
    "#line 1\n";

static const char* FS_SUFFIX =
    "\nvoid main() {\n"
    "    vec4 c = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "    mainImage(c, gl_FragCoord.xy);\n"
    "    gl_FragColor = c;\n"
    "}\n";

/* ------------------------------------------------------------------ */
static LRESULT CALLBACK RenderWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
        case WM_ERASEBKGND:
            return 1;
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_CLOSE:
            return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

/* ------------------------------------------------------------------ */
static void AppendLog(char* dst, int cap, const char* text)
{
    int used, room, i = 0;
    if (!dst || cap <= 1 || !text) return;
    used = StrLen(dst);
    room = cap - used - 1;
    if (room <= 0) return;
    while (text[i] && i < room) { dst[used + i] = text[i]; i++; }
    dst[used + i] = 0;
}

static GLhandle CompileOne(GLenum type, const char* src, char* log, int logSize)
{
    GLhandle sh;
    GLint    ok = 0;

    sh = gl.CreateShader(type);
    if (!sh) return 0;

    gl.ShaderSource(sh, 1, &src, NULL);
    gl.CompileShader(sh);
    gl.GetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (!ok) {
        char buf[4096];
        buf[0] = 0;
        gl.GetShaderInfoLog(sh, sizeof(buf) - 1, NULL, buf);
        AppendLog(log, logSize, buf);
        gl.DeleteShader(sh);
        return 0;
    }
    return sh;
}

/* ¿El usuario definio su propia funcion main()?
 *
 * Un simple strstr("void main") NO sirve: "void mainImage(" tambien
 * contiene esa subcadena. Hay que saltar comentarios y exigir que
 * "main" sea un identificador completo seguido de '('.
 */
static BOOL HasOwnMain(const char* s)
{
    int i = 0;
    while (s[i]) {
        /* comentario de linea */
        if (s[i] == '/' && s[i + 1] == '/') {
            while (s[i] && s[i] != '\n') i++;
            continue;
        }
        /* comentario de bloque */
        if (s[i] == '/' && s[i + 1] == '*') {
            i += 2;
            while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) i++;
            if (s[i]) i += 2;
            continue;
        }
        if (s[i] == 'm' && s[i+1] == 'a' && s[i+2] == 'i' && s[i+3] == 'n') {
            char prev = (i > 0) ? s[i - 1] : ' ';
            BOOL boundary = !((prev >= 'a' && prev <= 'z') ||
                              (prev >= 'A' && prev <= 'Z') ||
                              (prev >= '0' && prev <= '9') || prev == '_');
            int j = i + 4;
            while (s[j] == ' ' || s[j] == '\t') j++;
            /* "mainImage(" falla aqui: tras "main" viene 'I', no '(' */
            if (boundary && s[j] == '(') return TRUE;
            i += 4;
            continue;
        }
        i++;
    }
    return FALSE;
}

static char* BuildFragmentSource(const char* userSrc)
{
    /* Si el usuario ya escribio su propio main(), no envolvemos.
       Si no, asumimos estilo Shadertoy con mainImage(). */
    BOOL hasMain = HasOwnMain(userSrc);
    int  len = StrLen(FS_PREFIX) + StrLen(userSrc) +
               (hasMain ? 2 : StrLen(FS_SUFFIX)) + 8;
    char* out = (char*)MemAlloc(len);
    if (!out) return NULL;

    StrCopy(out, FS_PREFIX);
    StrCat(out, userSrc);
    if (!hasMain) StrCat(out, FS_SUFFIX);
    return out;
}

BOOL RendererApplyShader(const char* userSrc, char* log, int logSize)
{
    char*    fsrc;
    GLhandle vs, fs, prog;
    GLint    ok = 0;

    if (log && logSize > 0) log[0] = 0;
    if (!g_rc) {
        AppendLog(log, logSize, "El renderer no esta iniciado.\r\n");
        return FALSE;
    }
    wglMakeCurrent(g_dc, g_rc);

    fsrc = BuildFragmentSource(userSrc);
    if (!fsrc) return FALSE;

    fs = CompileOne(GL_FRAGMENT_SHADER, fsrc, log, logSize);
    MemFree(fsrc);
    if (!fs) return FALSE;

    vs = CompileOne(GL_VERTEX_SHADER, VS_SRC, log, logSize);
    if (!vs) { gl.DeleteShader(fs); return FALSE; }

    prog = gl.CreateProgram();
    gl.AttachShader(prog, vs);
    gl.AttachShader(prog, fs);
    gl.LinkProgram(prog);
    gl.GetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok) {
        char buf[4096];
        buf[0] = 0;
        gl.GetProgramInfoLog(prog, sizeof(buf) - 1, NULL, buf);
        AppendLog(log, logSize, buf);
        AppendLog(log, logSize,
            "\r\n[enlazado fallido] Recuerda: define mainImage(out vec4, in vec2)\r\n"
            "y deja que GLSLPaper anada el main(), o escribe tu propio main().\r\n");
        gl.DeleteProgram(prog);
        gl.DeleteShader(vs);
        gl.DeleteShader(fs);
        return FALSE;
    }

    /* En la ruta ARB los shaders se liberan con el mismo DeleteObject;
       el programa mantiene una referencia hasta el unlink. */
    gl.DeleteShader(vs);
    gl.DeleteShader(fs);

    if (g_prog) gl.DeleteProgram(g_prog);
    g_prog = prog;

    gl.UseProgram(g_prog);
    u_res        = gl.GetUniformLocation(g_prog, "iResolution");
    u_time       = gl.GetUniformLocation(g_prog, "iTime");
    u_globalTime = gl.GetUniformLocation(g_prog, "iGlobalTime");
    u_delta      = gl.GetUniformLocation(g_prog, "iTimeDelta");
    u_frame      = gl.GetUniformLocation(g_prog, "iFrame");
    u_mouse      = gl.GetUniformLocation(g_prog, "iMouse");

    AppendLog(log, logSize, "Shader compilado y enlazado correctamente.\r\n");
    return TRUE;
}

/* ------------------------------------------------------------------ */
static void EnsureTexture(int w, int h)
{
    if (g_tex && g_texW == w && g_texH == h) return;

    if (!g_tex) glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    /* NPOT esta garantizado: si hay GLSL, hay OpenGL 2.0. */
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, w, h, 0);
    g_texW = w;
    g_texH = h;
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void DrawQuad(void)
{
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();
}

/* ¿Hay una app a pantalla completa delante? Entonces no gastamos GPU. */
static BOOL ShouldPause(void)
{
    HWND fg = GetForegroundWindow();
    RECT r, scr;
    char cls[64];

    if (!fg) return FALSE;

    /* El propio escritorio ocupa toda la pantalla. Sin esta excepcion,
       en cuanto el usuario hace clic en el escritorio creeriamos que hay
       una aplicacion a pantalla completa delante y congelariamos la
       animacion justo cuando mas se ve. */
    cls[0] = 0;
    GetClassNameA(fg, cls, sizeof(cls));
    if (StrFind(cls, "Progman") == 0 || StrFind(cls, "WorkerW") == 0 ||
        StrFind(cls, "Shell_TrayWnd") == 0)
        return FALSE;

    if (!GetWindowRect(fg, &r)) return FALSE;

    scr.left = 0; scr.top = 0;
    scr.right  = GetSystemMetrics(SM_CXSCREEN);
    scr.bottom = GetSystemMetrics(SM_CYSCREEN);

    return (r.left <= scr.left && r.top <= scr.top &&
            r.right >= scr.right && r.bottom >= scr.bottom);
}

void RendererTick(void)
{
    DWORD now, elapsed;
    float t, dt;
    int   rw, rh;
    POINT pt;

    if (!g_rc || !g_prog) return;

    now = timeGetTime();

    if (g_fpsCap > 0) {
        DWORD minMs = (DWORD)(1000 / g_fpsCap);
        if (now - g_lastMs < minMs) {
            Sleep(1);
            return;
        }
    }

    if (ShouldPause()) { Sleep(50); return; }

    elapsed = now - g_lastMs;
    dt = (float)elapsed / 1000.0f;
    g_lastMs = now;
    t  = (float)(now - g_startMs) / 1000.0f;

    /* contador de fps */
    g_fpsAccMs += elapsed;
    g_fpsAccFrames++;
    if (g_fpsAccMs >= 1000) {
        g_fpsShown = (int)((g_fpsAccFrames * 1000) / (int)g_fpsAccMs);
        g_fpsAccMs = 0;
        g_fpsAccFrames = 0;
    }

    wglMakeCurrent(g_dc, g_rc);

    rw = g_width  / g_scale; if (rw < 1) rw = 1;
    rh = g_height / g_scale; if (rh < 1) rh = 1;

    GetCursorPos(&pt);

    /* --- pasada del shader, a resolucion reducida si toca --- */
    glDisable(GL_TEXTURE_2D);
    glViewport(0, 0, rw, rh);
    gl.UseProgram(g_prog);

    if (u_res  >= 0) gl.Uniform3f(u_res, (float)rw, (float)rh, 1.0f);
    if (u_time >= 0) gl.Uniform1f(u_time, t);
    if (u_globalTime >= 0) gl.Uniform1f(u_globalTime, t);
    if (u_delta >= 0) gl.Uniform1f(u_delta, dt);
    if (u_frame >= 0) gl.Uniform1i(u_frame, g_frame);
    if (u_mouse >= 0) gl.Uniform4f(u_mouse, (float)pt.x,
                                   (float)(g_height - pt.y), 0.0f, 0.0f);
    DrawQuad();

    /* --- reescalado al tamano real --- */
    if (g_scale > 1) {
        gl.UseProgram(0);
        EnsureTexture(rw, rh);
        glBindTexture(GL_TEXTURE_2D, g_tex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, rw, rh);
        glViewport(0, 0, g_width, g_height);
        glEnable(GL_TEXTURE_2D);
        glColor3f(1.0f, 1.0f, 1.0f);
        DrawQuad();
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    SwapBuffers(g_dc);
    g_frame++;
}

/* ------------------------------------------------------------------ */
BOOL RendererStart(char* err, int errSize)
{
    WNDCLASSA wc;
    PIXELFORMATDESCRIPTOR pfd;
    RECT r;
    int  pf;

    if (g_wnd) return TRUE;
    if (err && errSize > 0) err[0] = 0;

    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = RenderWndProc;
    wc.hInstance     = GetModuleHandleA(NULL);
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = RENDER_CLASS;
    RegisterClassA(&wc);

    DesktopGetVirtualRect(&r);
    g_width  = r.right - r.left;
    g_height = r.bottom - r.top;

    /* El padre y el estilo WS_CHILD se fijan AQUI, en la creacion.
       WS_CHILD no se puede anadir despues con SetWindowLong, y un
       SetParent sobre una ventana WS_POPUP solo cambia el propietario:
       la ventana sigue siendo de nivel superior y al mandarla al fondo
       del Z-order desaparece detras del escritorio. */
    g_host = DesktopResolveHost(DesktopEffectiveMode());

    g_wnd = CreateWindowExA(
        WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        RENDER_CLASS, "GLSLPaper",
        g_host ? (WS_CHILD | WS_VISIBLE) : (WS_POPUP | WS_VISIBLE),
        g_host ? 0 : r.left, g_host ? 0 : r.top, g_width, g_height,
        g_host, NULL, wc.hInstance, NULL);

    if (!g_wnd) {
        if (err) StrCopy(err, "No se pudo crear la ventana de render.");
        return FALSE;
    }

    DesktopPlace(g_wnd, g_host);

    g_dc = GetDC(g_wnd);

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    pf = ChoosePixelFormat(g_dc, &pfd);
    if (!pf || !SetPixelFormat(g_dc, pf, &pfd)) {
        if (err) StrCopy(err, "No hay un formato de pixel OpenGL valido.");
        RendererStop();
        return FALSE;
    }

    g_rc = wglCreateContext(g_dc);
    if (!g_rc || !wglMakeCurrent(g_dc, g_rc)) {
        if (err) StrCopy(err, "No se pudo crear el contexto OpenGL.");
        RendererStop();
        return FALSE;
    }

    if (!GlLoadShaderApi(err, errSize)) {
        RendererStop();
        return FALSE;
    }

    if (gl.SwapInterval) gl.SwapInterval(0); /* el cap de fps es nuestro */

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);

    RendererResetTime();
    return TRUE;
}

void RendererStop(void)
{
    if (g_rc) {
        wglMakeCurrent(g_dc, g_rc);
        if (g_prog) { gl.DeleteProgram(g_prog); g_prog = 0; }
        if (g_tex)  { glDeleteTextures(1, &g_tex); g_tex = 0; }
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_rc);
        g_rc = NULL;
    }
    if (g_dc)  { ReleaseDC(g_wnd, g_dc); g_dc = NULL; }
    if (g_wnd) { DestroyWindow(g_wnd); g_wnd = NULL; }
    g_texW = g_texH = 0;
    g_host = NULL;
}

BOOL RendererIsRunning(void) { return (g_rc != NULL && g_prog != 0); }

HWND RendererGetWindow(void) { return g_wnd; }

void RendererReanchor(void)
{
    /* No se puede reanclar en caliente: cambiar de padre exige recrear
       la ventana con WS_CHILD. El editor para y vuelve a aplicar. */
    if (g_wnd) DesktopPlace(g_wnd, g_host);
}

void RendererResetTime(void)
{
    g_startMs = timeGetTime();
    g_lastMs  = g_startMs;
    g_frame   = 0;
}

void RendererSetScale(int denom)
{
    if (denom < 1) denom = 1;
    if (denom > 8) denom = 8;
    g_scale = denom;
}
int  RendererGetScale(void)  { return g_scale; }
void RendererSetFpsCap(int f){ g_fpsCap = (f < 0) ? 0 : f; }
int  RendererGetFpsCap(void) { return g_fpsCap; }
int  RendererGetFps(void)    { return g_fpsShown; }
