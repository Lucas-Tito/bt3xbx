# Build do Xbox 360. Roda dentro da imagem do XDK, a partir da raiz do repositório:
#   port/x360/tools/build.sh            (Release)
#   port/x360/tools/build.sh CONFIG=Debug
# Saída: build/x360/<CONFIG>/default.xex (+ .map para resolver endereço de crash).

CONFIG ?= Release
include port/x360/package/titulo.env

WINE := wine
OUT := build/x360/$(CONFIG)
OBJ := $(OUT)/obj

XBIN := $(XEDK)/bin/win32
CL := "$(XBIN)/cl.exe"
LINK := "$(XBIN)/link.exe"
IMAGEXEX := "$(XBIN)/imagexex.exe"
INCLUDE_XDK := $(XEDK)/include/xbox;$(XEDK)/include/crt

# O programa da F0 (sonda): medições no console antes do port de verdade.
SRCS_CPP := port/x360/src/f0/sonda.cpp
SRCS_C := port/x360/src/diario.c port/x360/src/ambiente.c port/x360/src/f0/sonda_float.c port/src/softfloat_ps2.c
OBJS := $(SRCS_CPP:%.cpp=$(OBJ)/%.obj) $(SRCS_C:%.c=$(OBJ)/%.obj)

COMUM := -c -Zi -nologo -D _XBOX -Gm- -GS- -Zc:wchar_t -Zc:forScope -openmp- -Fd"$(@:.obj=.pdb)" \
         -Iport/x360/src -Iport/x360/include -Iinclude -Iport/src
# Sem -fp:fast: ele funde multiplicação e soma (fmadds) e muda o resultado. O jogo precisa de uma
# operação por vez, como no PS2.
FP := -fp:precise -fp:except-
CXXFLAGS := $(COMUM) $(FP) -W3 -EHsc -GR- -TP
CFLAGS := $(COMUM) $(FP) -W3 -TC -FImsvc_compat.h

ifeq ($(CONFIG),Release)
  CXXFLAGS += -Ox -Oi -D NDEBUG -MT -Gy -GF
  CFLAGS += -Ox -Oi -D NDEBUG -MT -Gy -GF
  LIBS := d3d9.lib xgraphics.lib xboxkrnl.lib xapilib.lib
else ifeq ($(CONFIG),Debug)
  # Sem otimização, mas com as libs de varejo: as de depuração (d3d9d...) exigem o xbdm, que o
  # console não tem, e o .xex nem carregaria.
  CXXFLAGS += -Od -MT
  CFLAGS += -Od -MT
  LIBS := d3d9.lib xgraphics.lib xboxkrnl.lib xapilib.lib
else
  $(error CONFIG desconhecida: $(CONFIG))
endif

LDFLAGS := -NOLOGO -DEBUG -PDB:"$(OUT)/default.pdb" -STACK:"262144","262144" -RELEASE -XEX:NO \
           -MAP:"$(OUT)/default.map" -OPT:REF

all: $(OUT)/default.xex

$(OUT)/default.xex: $(OUT)/default.exe build/x360/spa.bin port/x360/package/xex.xml port/x360/package/titulo.env
	@echo "XEX $@"
	@$(WINE) $(IMAGEXEX) -nologo -config:port/x360/package/xex.xml -titleid:$(TITLE_ID) \
	    -section:$(subst 0x,,$(TITLE_ID))=build/x360/spa.bin -out:"$@" "$<"

# spa.bin (nome e ícone que o Aurora/FreeStyle mostram) é gerado no host pelo build.sh: a imagem
# não tem Python.

$(OUT)/default.exe: $(OBJS)
	@echo "LINK $@"
	@LIB="$(XEDK)/lib/xbox" $(WINE) $(LINK) $(LDFLAGS) -OUT:"$@" $^ $(LIBS)

# Dependência grossa de propósito: todo .obj depende de todos os headers nossos e deste Makefile
# (no CollectionUI, header mudado sem recompilar gerou binário misto).
HEADERS := $(shell find port/x360 -name '*.h') port/src/softfloat_ps2_inl.h $(wildcard include/port/*.h)

$(OBJ)/%.obj: %.cpp $(HEADERS) port/x360/Makefile
	@mkdir -p $(@D)
	@echo "CC  $<"
	@INCLUDE="$(INCLUDE_XDK)" $(WINE) $(CL) $(CXXFLAGS) -Fo"$@" $<

$(OBJ)/%.obj: %.c $(HEADERS) port/x360/Makefile
	@mkdir -p $(@D)
	@echo "CC  $<"
	@INCLUDE="$(INCLUDE_XDK)" $(WINE) $(CL) $(CFLAGS) -Fo"$@" $<

clean:
	rm -rf $(OUT)

.PHONY: all clean
