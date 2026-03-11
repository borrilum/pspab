# ------------------------------------------------------------
# Minimal PSPSDK Makefile for BlackSquareTrail
# Build output includes EBOOT.PBP
# ------------------------------------------------------------

TARGET = BlackSquareTrail
OBJS = main.o

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

LIBS = -lpspgu -lpspdisplay -lpspctrl -lpspge -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = BlackSquareTrail

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
