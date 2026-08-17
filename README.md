# GLSLPaper

Fondos de pantalla animados escritos en GLSL, al estilo Active Desktop.
Un editor tipo bloc de notas: escribes el shader, guardas, aplicas, y la
animación se dibuja en tiempo real por debajo de los iconos del escritorio.

**Objetivo de compatibilidad:** Windows 98SE hasta Windows 11, sobre
hardware que exponga OpenGL 2.0 / GLSL 1.10. El suelo práctico es una
**GeForce 6xxx con ForceWare 81.98** en 98SE.

---

## Compilar

### Local (MSYS2 en Windows)

```sh
pacman -S make mingw-w64-i686-gcc
make
```

Sale `build/glslpaper.exe`, un único binario sin instalador.

### GitHub Actions

`.github/workflows/build.yml` ya está listo. Compila en `MINGW32`, ejecuta
una auditoría de imports y sube `glslpaper.exe` como artefacto en cada push.

---

## Uso

1. Escribe o abre un shader (hay ejemplos en `shaders/`).
2. **F5** para aplicar. Si hay error de compilación, aparece en el panel
   inferior y el shader anterior sigue corriendo.
3. **Ver → Escala** baja la resolución de render (la palanca que hace
   usable el 90% de los shaders de Shadertoy en hardware modesto).
4. **Ver → Límite de fps** por defecto 30. Es un fondo, no un benchmark.

El render se pausa solo cuando hay una aplicación a pantalla completa
delante.

### Perfil de shader

Se escribe contra **GLSL 1.10**. El programa inyecta el preámbulo:

```glsl
uniform vec3  iResolution;
uniform float iTime;
uniform float iGlobalTime;
uniform float iTimeDelta;
uniform int   iFrame;
uniform vec4  iMouse;
```

Si tu código **no** contiene `void main`, se asume estilo Shadertoy y se
envuelve automáticamente tu `mainImage(out vec4, in vec2)`. Si escribes tu
propio `main()`, se usa tal cual.

Reglas del perfil 110 (lo que rompe al pegar shaders modernos):
`texture2D()` en vez de `texture()`, `varying`/`attribute` en vez de
`in`/`out`, `gl_FragColor` en vez de una salida propia, y **todos los
floats con punto decimal** (`1.0`, no `1`).

---

## Arquitectura

| Fichero | Qué hace |
|---|---|
| `src/common.*` | Utilidades sin CRT, IO de ficheros, detección de versión |
| `src/glloader.*` | Carga GLSL: nombres core de GL 2.0 con *fallback* a ARB |
| `src/desktop.cpp` | Anclaje al escritorio: `Progman` vs `WorkerW` |
| `src/renderer.*` | Contexto WGL, pipeline de shader, quad, reescalado |
| `src/editor.cpp` | Ventana, control `EDIT`, menús, diálogos de fichero |
| `src/main.cpp` | `WinMain` y bucle de render |

Decisiones que parecen raras y son deliberadas:

- **Control `EDIT`, no RichEdit.** `riched32` / `riched20` / `msftedit`
  cambian de DLL entre versiones de Windows; `EDIT` es idéntico desde 95.
- **Modo inmediato (`glBegin`/`glEnd`) y contexto WGL clásico.** Es lo
  único que se comporta igual en un driver de 2005 y en uno de 2026 sin
  escribir dos rutas.
- **`timeGetTime()` con `timeBeginPeriod(1)`**, no `QueryPerformanceCounter`,
  que da saltos hacia adelante en varios chipsets de la era 98SE.
- **Win32 ANSI en todo.** Las funciones `-W` no existen en 9x.
- **NPOT sin comprobar:** si hay GLSL, hay OpenGL 2.0, y NPOT es
  obligatorio en esa versión.

---

## Estado y siguientes pasos

Esto es el esqueleto v0. Funciona, pero falta lo siguiente:

- [ ] **Verificación real en 98SE.** El binario de MinGW-w64 se marca
      como PE 4.0 y evita APIs modernas, pero la CRT de MinGW puede
      arrastrar imports que no existen en el `kernel32` de 98. Ejecuta
      `make imports` y contrasta con un `kernel32.dll` de 98SE real
      (o Dependency Walker) antes de dar por buena la compatibilidad.
      El siguiente paso si algo falla es `-nostdlib` con entry point
      propio: el código ya evita la CRT casi por completo (`HeapAlloc`,
      `CreateFileA`, `wsprintfA`), así que la migración es corta.
- [ ] **Separar el renderer en otro proceso** (`WM_COPYDATA`). Un shader
      que cuelgue el driver no debería llevarse por delante lo que estás
      escribiendo.
- [ ] **Icono en la bandeja** para cerrar el editor sin parar el fondo.
      Ojo: en 98 hay que usar `NOTIFYICONDATA_V1_SIZE`, no `sizeof`.
- [ ] **Multi-monitor con origen negativo.** Ahora se asume que el
      escritorio virtual empieza en (0,0) respecto al host.
- [ ] **Validador de shader** que avise de los patrones donde divergen el
      compilador permisivo de NVIDIA de 2005 (hereda mucho de Cg: acepta
      `float x = 1;`) y los compiladores estrictos de AMD/Intel actuales.
      Regla práctica: desarrolla contra el estricto, verifica en el viejo.
- [ ] Persistencia de ajustes en un `.ini` junto al `.exe` (nada de
      `%APPDATA%`, que en 9x no está garantizado).
- [ ] Resaltado de sintaxis (requiere control propio, no `EDIT`).
