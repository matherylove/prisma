#include "glloader.h"
#include "common.h"

GlFns gl;

static PROC GetAny(const char* a, const char* b)
{
    PROC p = wglGetProcAddress(a);
    if (!p && b) p = wglGetProcAddress(b);
    return p;
}

BOOL GlLoadShaderApi(char* errOut, int errSize)
{
    PROC probe;

    ZeroMemory(&gl, sizeof(gl));

    /* ¿Ruta core de OpenGL 2.0 disponible? */
    probe = wglGetProcAddress("glCreateShader");
    gl.usedArbPath = (probe == NULL);

    gl.CreateShader  = (PFN_CreateShader) GetAny("glCreateShader",  "glCreateShaderObjectARB");
    gl.ShaderSource  = (PFN_ShaderSource) GetAny("glShaderSource",  "glShaderSourceARB");
    gl.CompileShader = (PFN_CompileShader)GetAny("glCompileShader", "glCompileShaderARB");
    gl.CreateProgram = (PFN_CreateProgram)GetAny("glCreateProgram", "glCreateProgramObjectARB");
    gl.AttachShader  = (PFN_AttachShader) GetAny("glAttachShader",  "glAttachObjectARB");
    gl.LinkProgram   = (PFN_LinkProgram)  GetAny("glLinkProgram",   "glLinkProgramARB");
    gl.UseProgram    = (PFN_UseProgram)   GetAny("glUseProgram",    "glUseProgramObjectARB");

    gl.GetUniformLocation = (PFN_GetUniformLocation)
        GetAny("glGetUniformLocation", "glGetUniformLocationARB");

    gl.Uniform1f = (PFN_Uniform1f)GetAny("glUniform1f", "glUniform1fARB");
    gl.Uniform1i = (PFN_Uniform1i)GetAny("glUniform1i", "glUniform1iARB");
    gl.Uniform3f = (PFN_Uniform3f)GetAny("glUniform3f", "glUniform3fARB");
    gl.Uniform4f = (PFN_Uniform4f)GetAny("glUniform4f", "glUniform4fARB");

    if (gl.usedArbPath) {
        /* ARB usa una sola funcion de consulta y una sola de borrado */
        PFN_GetObjectiv  q = (PFN_GetObjectiv) wglGetProcAddress("glGetObjectParameterivARB");
        PFN_GetInfoLog   l = (PFN_GetInfoLog)  wglGetProcAddress("glGetInfoLogARB");
        PFN_DeleteObject d = (PFN_DeleteObject)wglGetProcAddress("glDeleteObjectARB");
        gl.GetShaderiv = q;  gl.GetProgramiv = q;
        gl.GetShaderInfoLog = l; gl.GetProgramInfoLog = l;
        gl.DeleteShader = d; gl.DeleteProgram = d;
    } else {
        gl.GetShaderiv        = (PFN_GetObjectiv) wglGetProcAddress("glGetShaderiv");
        gl.GetProgramiv       = (PFN_GetObjectiv) wglGetProcAddress("glGetProgramiv");
        gl.GetShaderInfoLog   = (PFN_GetInfoLog)  wglGetProcAddress("glGetShaderInfoLog");
        gl.GetProgramInfoLog  = (PFN_GetInfoLog)  wglGetProcAddress("glGetProgramInfoLog");
        gl.DeleteShader       = (PFN_DeleteObject)wglGetProcAddress("glDeleteShader");
        gl.DeleteProgram      = (PFN_DeleteObject)wglGetProcAddress("glDeleteProgram");
    }

    /* Opcional: control de vsync */
    gl.SwapInterval = (PFN_SwapInterval)wglGetProcAddress("wglSwapIntervalEXT");

    if (!gl.CreateShader || !gl.ShaderSource || !gl.CompileShader ||
        !gl.CreateProgram || !gl.AttachShader || !gl.LinkProgram ||
        !gl.UseProgram || !gl.GetShaderiv || !gl.GetShaderInfoLog ||
        !gl.GetUniformLocation) {
        if (errOut && errSize > 0) {
            const char* v = (const char*)glGetString(GL_VERSION);
            const char* r = (const char*)glGetString(GL_RENDERER);
            wsprintfA(errOut,
                "Esta GPU/driver no expone GLSL.\r\n"
                "GL_VERSION : %s\r\n"
                "GL_RENDERER: %s\r\n\r\n"
                "Se necesita OpenGL 2.0 o GL_ARB_shader_objects.\r\n"
                "Minimo probado: GeForce 6xxx con ForceWare 81.98.",
                v ? v : "(desconocido)", r ? r : "(desconocido)");
        }
        return FALSE;
    }
    return TRUE;
}
