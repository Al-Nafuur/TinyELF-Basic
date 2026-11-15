# TinyELF Basic
#
#   make          builds TinyELF-Basic-v<version>.bin; <version> is read from
#                 the start banner in main.c (mgreet)
#   make check    prints the section sizes and the symbols the ELF imports
#                 from the cartridge firmware (memset and vcsLib only)
#   make clean    removes the binary of the current version
#
# Releases are built with GCC 14.2 (Arm GNU Toolchain 14.2.Rel1), which gives
# a byte-identical copy of the released file. Select another toolchain with
#   make PREFIX=<path>/bin/arm-none-eabi-

PREFIX  ?= arm-none-eabi-
CC      := $(PREFIX)gcc
NM      := $(PREFIX)nm
SIZE    := $(PREFIX)size

VERSION := $(shell sed -n 's/.*"TinyELF Basic v\([0-9.]*\) ".*/\1/p' main.c)
ifeq ($(VERSION),)
$(error Could not read the version from mgreet in main.c)
endif
TARGET  := TinyELF-Basic-v$(VERSION).bin

SRCS    := $(sort $(wildcard *.c))
CFLAGS  := -mlong-calls -fno-exceptions -fno-builtin-memmove -march=armv7-m -Os -Wall

.PHONY: all check clean

all: $(TARGET)

# one relocatable ELF (-r); tinyelf.ld keeps all variables out of .bss
$(TARGET): $(SRCS) $(wildcard *.h) tinyelf.ld
	$(CC) -r $(CFLAGS) -Wl,-T,tinyelf.ld $(SRCS) -o $@ -lgcc

check: $(TARGET)
	$(SIZE) $(TARGET)
	@echo "Imported symbols:"
	@$(NM) $(TARGET) | grep " U "

clean:
	rm -f $(TARGET)
