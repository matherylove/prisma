/* main.cpp - punto de entrada y bucle principal.
 *
 * El bucle es de tipo juego (PeekMessage + render) porque necesitamos
 * dibujar continuamente aunque no lleguen mensajes.
 */
#include "common.h"
#include "renderer.h"
#include <mmsystem.h>

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int cmdShow)
{
    MSG   msg;
    HWND  main;
    HACCEL accel;
    DWORD lastStatus;

    (void)prev; (void)cmdLine;

    /* timeGetTime con resolucion de 1 ms es estable en toda la linea
       9x -> 11, al contrario que QueryPerformanceCounter en chipsets
       de la epoca de Windows 98. */
    timeBeginPeriod(1);

    if (!EditorCreate(inst, cmdShow)) {
        MessageBoxA(NULL, "No se pudo crear la ventana principal.",
                    "GLSLPaper", MB_OK | MB_ICONERROR);
        timeEndPeriod(1);
        return 1;
    }

    main  = EditorGetHwnd();
    accel = EditorGetAccel();
    lastStatus = timeGetTime();

    for (;;) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            if (!accel || !TranslateAcceleratorA(main, accel, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }

        RendererTick();

        if (timeGetTime() - lastStatus >= 1000) {
            lastStatus = timeGetTime();
            EditorRefreshStatus();
        }

        if (!RendererIsRunning()) {
            /* nada que dibujar: no quemamos CPU */
            WaitMessage();
        }
    }

done:
    RendererStop();
    timeEndPeriod(1);
    return 0;
}
