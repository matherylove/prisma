/* glloader.h - carga en runtime de las funciones GLSL.
 *
 * Intenta primero los nombres core de OpenGL 2.0 y, si no estan,
 * cae a las extensiones ARB (GL_ARB_shader_objects). Los handles ARB
 * (GLhandleARB) son unsigned int en Windows, igual que los GLuint del
 * core, por eso podemos unificar ambas rutas con un solo tipo.
 */
#ifndef GLSLPAPER_GLLOADER_H
#define GLSLPAPER_GLLOADER_H

#include <windows.h>
#include <GL/gl.h>

typedef unsigned int GLhandle;

/* Enums que no estan en el gl.h de la epoca */
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER    0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER      0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS     0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS        0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH    0x8B84
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE      0x812F
#endif

typedef GLhandle (APIENTRY *PFN_CreateShader)(GLenum);
typedef void     (APIENTRY *PFN_ShaderSource)(GLhandle, GLsizei, const char**, const GLint*);
typedef void     (APIENTRY *PFN_CompileShader)(GLhandle);
typedef GLhandle (APIENTRY *PFN_CreateProgram)(void);
typedef void     (APIENTRY *PFN_AttachShader)(GLhandle, GLhandle);
typedef void     (APIENTRY *PFN_LinkProgram)(GLhandle);
typedef void     (APIENTRY *PFN_UseProgram)(GLhandle);
typedef void     (APIENTRY *PFN_GetObjectiv)(GLhandle, GLenum, GLint*);
typedef void     (APIENTRY *PFN_GetInfoLog)(GLhandle, GLsizei, GLsizei*, char*);
typedef void     (APIENTRY *PFN_DeleteObject)(GLhandle);
typedef GLint    (APIENTRY *PFN_GetUniformLocation)(GLhandle, const char*);
typedef void     (APIENTRY *PFN_Uniform1f)(GLint, GLfloat);
typedef void     (APIENTRY *PFN_Uniform1i)(GLint, GLint);
typedef void     (APIENTRY *PFN_Uniform2f)(GLint, GLfloat, GLfloat);
typedef void     (APIENTRY *PFN_Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void     (APIENTRY *PFN_Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef BOOL     (APIENTRY *PFN_SwapInterval)(int);

struct GlFns {
    PFN_CreateShader       CreateShader;
    PFN_ShaderSource       ShaderSource;
    PFN_CompileShader      CompileShader;
    PFN_CreateProgram      CreateProgram;
    PFN_AttachShader       AttachShader;
    PFN_LinkProgram        LinkProgram;
    PFN_UseProgram         UseProgram;
    PFN_GetObjectiv        GetShaderiv;
    PFN_GetObjectiv        GetProgramiv;
    PFN_GetInfoLog         GetShaderInfoLog;
    PFN_GetInfoLog         GetProgramInfoLog;
    PFN_DeleteObject       DeleteShader;
    PFN_DeleteObject       DeleteProgram;
    PFN_GetUniformLocation GetUniformLocation;
    PFN_Uniform1f          Uniform1f;
    PFN_Uniform1i          Uniform1i;
    PFN_Uniform2f          Uniform2f;
    PFN_Uniform3f          Uniform3f;
    PFN_Uniform4f          Uniform4f;
    PFN_SwapInterval       SwapInterval;
    BOOL                   usedArbPath;
};

extern GlFns gl;

/* Requiere un contexto activo (wglMakeCurrent) antes de llamarse. */
BOOL GlLoadShaderApi(char* errOut, int errSize);

#endif /* GLSLPAPER_GLLOADER_H */
