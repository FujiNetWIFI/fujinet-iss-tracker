# intv/Makefile -- IntyBASIC build for the Intellivision ISS tracker.
#
# Standalone rather than wired into ../makefiles: those drive cc65-family C
# toolchains for the other platforms, and IntyBASIC/as1600 is a different
# toolchain entirely (same arrangement as the other IntyBASIC FujiNet
# clients, e.g. netcat/intv).
#
# Override INTYBASIC/AS1600/LIBDIR on the command line if your tools
# aren't on PATH, e.g.:
#   make INTYBASIC=/path/to/intybasic LIBDIR=/path/to/IntyBASIC/intybasic

INTYBASIC ?= intybasic
AS1600    ?= as1600
LIBDIR    ?= $(HOME)/Workspace/IntyBASIC/intybasic/

PRODUCT = iss
SOURCES = iss.bas fujinet.bas map.bas constants.bas

.PHONY: all clean run regen-map

all: $(PRODUCT).bin $(PRODUCT).rom

$(PRODUCT).asm: $(SOURCES)
	$(INTYBASIC) iss.bas $(PRODUCT).asm $(LIBDIR)

# .bin + .cfg -- for loading from SD via the cartridge menu.
$(PRODUCT).bin: $(PRODUCT).asm
	$(AS1600) -o $(PRODUCT).bin -l $(PRODUCT).lst $(PRODUCT).asm

# .rom -- Intellicart format, for jzIntv and network boot.
$(PRODUCT).rom: $(PRODUCT).asm
	$(AS1600) -o $(PRODUCT).rom $(PRODUCT).asm

run: $(PRODUCT).rom
	./run.sh

# Regenerate map.bas from the PNG (needs python3 + Pillow). map.bas is
# committed, so this is only needed after changing iss-intv.png.
regen-map:
	python3 workfiles/png2map.py iss-intv.png map.bas workfiles/preview.png

clean:
	rm -f $(PRODUCT).asm $(PRODUCT).bin $(PRODUCT).cfg $(PRODUCT).rom $(PRODUCT).lst
