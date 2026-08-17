#ifndef GLSLPAPER_RENDERER_H
#define GLSLPAPER_RENDERER_H

#include <windows.h>

BOOL  RendererStart(char* err, int errSize);
void  RendererStop(void);
BOOL  RendererIsRunning(void);
HWND  RendererGetWindow(void);
void  RendererReanchor(void);

/* Compila y activa el shader. Devuelve FALSE y rellena 'log' si falla;
   en ese caso el shader anterior sigue en marcha. */
BOOL  RendererApplyShader(const char* userSrc, char* log, int logSize);

void  RendererTick(void);          /* llamar desde el bucle principal */
void  RendererResetTime(void);

void  RendererSetScale(int denom); /* 1, 2, 3, 4 */
int   RendererGetScale(void);
void  RendererSetFpsCap(int fps);  /* 0 = sin limite */
int   RendererGetFpsCap(void);
int   RendererGetFps(void);        /* fps medidos, redondeados */

#endif
