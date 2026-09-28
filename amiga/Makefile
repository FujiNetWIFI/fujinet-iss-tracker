# amiga/Makefile -- ISS Tracker for the Amiga (FujiNet NIO client).
#
# Standalone rather than wired into ../makefiles: those drive cc65-family
# toolchains, and this port builds with amiga-gcc against fujinet-nio-lib
# (same arrangement as the coco/ and intv/ ports).
#
# Requirements:
#   - amiga-gcc (m68k-amigaos-gcc) on PATH, e.g. /opt/amiga/bin
#   - FUJINET_NIO_LIB pointing at a fujinet-nio-lib checkout, with
#     fujinet-nio-driver alongside it (or set FUJINET_NIO_DRIVER)
#   - xdftool (amitools) or uvx, for `make disk`
#   - python3 + Pillow only for the regen-* targets
#
#   make            # -> build/ISSTracker
#   make disk       # -> dist/ISSTracker.adf
#   make test       # host-side logic tests (gcc)

ifndef FUJINET_NIO_LIB
  ifdef NIO_WORKSPACE
    FUJINET_NIO_LIB := $(NIO_WORKSPACE)/repos/fujinet-nio-lib
  endif
endif
FUJINET_NIO_DRIVER ?= $(FUJINET_NIO_LIB)/../fujinet-nio-driver

CC       := m68k-amigaos-gcc
HOSTCC   ?= gcc
PYTHON   ?= python3

PROGRAM  := ISSTracker
LABEL    := ISSTracker
BUILD    := build
DIST     := dist

NIO_LIB_A := $(FUJINET_NIO_LIB)/build/fujinet-nio-amiga.a

# -mcrt=nix13 keeps the executable Kickstart 1.3 compatible and handles
# Workbench startup. Every OS call used is available in V33 (1.2/1.3).
CFLAGS  := -mcpu=68000 -msoft-float -mcrt=nix13 -Os -Wall -Wextra -std=gnu99 \
           -fomit-frame-pointer -I$(FUJINET_NIO_LIB)/include \
           -I$(FUJINET_NIO_DRIVER)/amiga/include
LDFLAGS := -mcpu=68000 -msoft-float -mcrt=nix13 -s

SRCS := main.c screen.c sprite.c night.c trail.c who.c fetch.c json.c geo.c \
        map_data.c region.c region_data.c config.c home.c sound.c twinkle.c \
        ufo.c ufo_path.c
OBJS := $(SRCS:%.c=$(BUILD)/obj/%.o)

ICONS := icons/ISSTracker.info icons/Disk.info icons/ReadMe.info

.PHONY: all disk test clean regen-map regen-icons regen-regions nio-lib check-env

all: $(BUILD)/$(PROGRAM)

check-env:
	@if [ -z "$(FUJINET_NIO_LIB)" ] || [ ! -f "$(FUJINET_NIO_LIB)/include/fujinet-nio.h" ]; then \
		echo "Set FUJINET_NIO_LIB to a fujinet-nio-lib checkout"; exit 1; fi

# Always ask fujinet-nio-lib's own build whether its archive is current.
nio-lib: check-env
	$(MAKE) -C $(FUJINET_NIO_LIB) amiga

$(NIO_LIB_A): nio-lib

$(BUILD)/obj/%.o: src/%.c | check-env
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD)/$(PROGRAM): $(OBJS) $(NIO_LIB_A)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(NIO_LIB_A)

# Non-bootable OFS disk (reads on a stock Kickstart 1.3 machine) holding
# the program, its icons and a ReadMe. Boot Workbench, then insert it.
XDFTOOL := $(shell command -v xdftool 2>/dev/null || \
             (command -v uvx >/dev/null 2>&1 && echo "uvx --from amitools xdftool"))

disk: $(DIST)/$(PROGRAM).adf

$(DIST)/$(PROGRAM).adf: $(BUILD)/$(PROGRAM) $(ICONS) ReadMe.txt
	@if [ -z "$(XDFTOOL)" ]; then echo "xdftool (amitools) or uvx is required"; exit 1; fi
	@mkdir -p $(DIST)
	rm -f $@
	$(XDFTOOL) $@ create + format $(LABEL) ofs \
		+ write $(BUILD)/$(PROGRAM) $(PROGRAM) \
		+ write icons/ISSTracker.info $(PROGRAM).info \
		+ write icons/Disk.info Disk.info \
		+ write ReadMe.txt ReadMe \
		+ write icons/ReadMe.info ReadMe.info
	$(XDFTOOL) $@ list

test:
	@mkdir -p $(BUILD)/host
	$(HOSTCC) -std=c99 -Wall -Wextra -O2 -o $(BUILD)/host/test_logic \
		tests/test_logic.c src/geo.c src/json.c src/night.c src/region.c \
		src/region_data.c src/config.c src/ufo_path.c
	$(BUILD)/host/test_logic

# Regenerate committed assets (outputs are checked in; a normal build does
# not need Python or Pillow).
regen-map:
	$(PYTHON) tools/png2planar.py --preview $(BUILD)/map-preview.png gfx/map.png gfx/lights.png src/map_data.c

regen-regions:
	$(PYTHON) tools/mkregions.py gfx/regions.png gfx/regions.txt src/region_data.c

regen-icons:
	$(PYTHON) tools/mkinfo.py --preview $(BUILD)/icon-preview.png

clean:
	rm -rf $(BUILD) $(DIST)

-include $(OBJS:.o=.d)
