# Makefile mit Map-Erzeugung + Analyse-Targets
CC := arm-none-eabi-gcc
LD := arm-none-eabi-ld
OBJCOPY := arm-none-eabi-objcopy
STRIP := arm-none-eabi-strip
NM := arm-none-eabi-nm
READELF := arm-none-eabi-readelf
SIZE := arm-none-eabi-size

TARGET := main
RELOC_OUT := $(TARGET).bin        # deine bisherige relocatable Ausgabe (wie im Repo)
ELF := $(TARGET).elf              # voll gelinktes ELF (für Map/Analyse)
MAP := $(TARGET).map
FLATBIN := $(TARGET)-flat.bin     # optional: reines binary aus ELF (für Tests)
RELOC_GC := $(TARGET)-reloc.o     # reloc object mit garbage-collection (falls loader ok)

# Quellen
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

# Flags
# Kompilier-Flags (für einzelne .c -> .o)
CFLAGS_COMPILE := -mlong-calls -fno-exceptions -fno-builtin-memmove -march=armv7-m -Wall
# Size-optimierung empfohlen für Analyse/Release:
CFLAGS_COMPILE += -Os -ffunction-sections -fdata-sections

# Flags für relocatable single-line build (dein altes Verhalten)
CFLAGS_RELOC := -r -mlong-calls -fno-exceptions -fno-builtin-memmove -march=armv7-m -Os -Wall -Wl,-T,tinyelf.ld

# Linker flags für ELF (erzeugen Map, gc-sections, Memory-usage)
LDFLAGS := -lgcc
LDFLDFLAGS_MAP := -Wl,-Map=$(MAP),--gc-sections,--print-memory-usage

.PHONY: all map reloc reloc-gc clean check-undefs check-size check-top

# DEFAULT: wie im Repo, relocatable single-file (kompatibel mit Gopher workflow)
all: $(RELOC_OUT)

# Erzeuge relocatable kombinierte Datei (wie vorher)
# Achtung: bei diesem single-line -r-Aufruf funktioniert --gc-sections nicht; für GC siehe reloc-gc target
$(RELOC_OUT): $(SRCS)
	$(CC) $(CFLAGS_RELOC) $(SRCS) -o $@ $(LDFLAGS)
	$(STRIP) $@ -d -R.comment -R.ARM.attributes

# ------------------------
# FULL ELF + MAP target
# Erzeugt: out.elf + out.map + optional flat binary
# ------------------------
#$(ELF): $(OBJS)
#	$(CC) $(OBJS)  -nostdlib -nostartfiles -nodefaultlibs  $(LDFLDFLAGS_MAP) -o $(ELF) $(LDFLAGS)
#	$(STRIP) $(ELF) --strip-unneeded
# Link to ELF with map, no startfiles/libc, keep symbols (no strip)
$(ELF): $(OBJS)
	$(CC) $(OBJS) -nostdlib -nostartfiles -nodefaultlibs -Wl,-Map=$(MAP),--gc-sections,--print-memory-usage -o $(ELF) -lgcc
	@echo "WICHTIG: ELF ungestript (für Analyse). main.map erzeugt."


# flat binary aus ELF (optional)
$(FLATBIN): $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(FLATBIN)

# Shortcut target: map (build ELF+Map + flat binary)
map: $(ELF) $(FLATBIN)
	@echo "ELF + MAP + flat binary erstellt: $(ELF), $(MAP), $(FLATBIN)"

# ------------------------
# Produce relocatable object with garbage collection
# (erzeugt reloc object, entfernt nicht referenzierte sections)
# Manche Loader akzeptieren das; teste ob Gopher damit arbeitet.
# ------------------------
reloc-gc: $(OBJS)
	$(LD) -r --gc-sections -o $(RELOC_GC) $(OBJS)
	$(STRIP) $(RELOC_GC) --strip-unneeded
	@echo "Relocatable object with GC: $(RELOC_GC)"

# ------------------------
# Build rules to compile each .c -> .o (needed for ELF/map and reloc-gc)
# ------------------------
#%.o: %.c
#	$(CC) $(CFLAGS_COMPILE) -c $< -o $@
# build per-file objects (needed for gc-sections)
%.o: %.c
	$(CC) -c $(CFLAGS_COMPILE) $< -o $@


# ------------------------
# Analyse / helper targets
# ------------------------
# Liste undefinierter Symbole (nur U)
check-undefs: all
	@echo "Undefined symbols in $(RELOC_OUT):"
	@$(NM) $(RELOC_OUT) | grep " U " || true

# Größe & Sektionen des ELF (nutze map target vorher)
check-size: $(ELF)
	@echo "size (ELF):" ; $(SIZE) $(ELF) || true
	@echo "sections:" ; $(READELF) -S $(ELF) || true

# Top N größte Symbole in ELF (nützlich um Platzfresser zu finden)
# Ausgabe: size symbol
check-top: $(ELF)
	@echo "Top symbols (by size):"
	@$(READELF) -sW $(ELF) | awk 'NR>3 { if($3>0) print $3" "$8 }' | sort -nr | head -n 40

clean:
	rm -f $(RELOC_OUT) $(ELF) $(MAP) $(FLATBIN) $(RELOC_GC) $(OBJS) *.lst

