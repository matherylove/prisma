# GLSLPaper - build con MinGW-w64 (32 bits)
#
#   make            compila build/glslpaper.exe
#   make clean      limpia
#   make imports    lista las DLL/simbolos importados (para revisar 98SE)

CXX     ?= g++
OBJDUMP ?= objdump
BUILD   := build
TARGET  := $(BUILD)/glslpaper.exe

SRC := src/main.cpp src/common.cpp src/desktop.cpp \
       src/editor.cpp src/glloader.cpp src/renderer.cpp
OBJ := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRC))

# WINVER 0x0410 = Windows 98. Sin esto los headers habilitan APIs
# que no existen en el kernel32 de 9x y el .exe ni siquiera arranca.
DEFS := -DWINVER=0x0410 -D_WIN32_WINNT=0x0410 -DWIN32_LEAN_AND_MEAN

CXXFLAGS := -m32 -Os -std=gnu++98 \
            -fno-exceptions -fno-rtti -fno-strict-aliasing \
            -march=pentium2 -mtune=generic \
            -Wall -Wno-unused-parameter $(DEFS)

# major/minor os-version y subsystem-version a 4.0 marcan el PE como
# "ejecutable de Windows 95/98" en la cabecera.
LDFLAGS := -m32 -mwindows -s \
           -static -static-libgcc -static-libstdc++ \
           -Wl,--major-subsystem-version,4 -Wl,--minor-subsystem-version,0 \
           -Wl,--major-os-version,4 -Wl,--minor-os-version,0

LIBS := -lopengl32 -lgdi32 -lcomdlg32 -lwinmm -luser32 -lkernel32

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS) $(LIBS)

imports: $(TARGET)
	$(OBJDUMP) -p $(TARGET) | grep -A400 "DLL Name"

clean:
	rm -rf $(BUILD)

.PHONY: all clean imports
