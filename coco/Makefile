SRC_DIRS ?= ./src

CC=cmoc

ifeq ($(COCO3),1)
TARGET_EXEC := iss3
CFLAGS += -DCOCO3
BUILD_DIR := ./build/coco3
# CoCo 3 loads via RUNM in compat mode, so the program must stay under $8000
# (BASIC/Disk ROM region). $8000-$FFFF is reused as the hi-res video buffer.
LDFLAGS_EXTRA := --limit=8000
else
TARGET_EXEC := iss
BUILD_DIR := ./build/coco12
endif

DIST_DISK := iss.dsk

SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

.PHONY: all dist clean

# Default: build both versions and the auto-detecting boot disk in one pass.
# loader.bin detects the machine and launches iss3.bin (CoCo 3) or iss.bin (CoCo 1/2).
all: dist

dist: clean
	$(MAKE) iss.bin
	$(MAKE) COCO3=1 iss3.bin
	$(CC) -oloader.bin loader.c
	$(RM) $(DIST_DISK)
	decb dskini $(DIST_DISK)
	printf '10 RUNM"LOADER"\n' > autoexec.bas
	decb copy -0 -a -l -r autoexec.bas $(DIST_DISK),AUTOEXEC.BAS
	writecocofile $(DIST_DISK) loader.bin
	writecocofile $(DIST_DISK) iss.bin
	writecocofile $(DIST_DISK) iss3.bin
	cp $(DIST_DISK) /tnfs/coco/

$(TARGET_EXEC).bin: $(OBJS)
	$(CC) -o$@ $(LDFLAGS_EXTRA) $(OBJS)

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -r iss iss3 iss.bin iss3.bin iss.dsk iss3.dsk loader.bin autoexec.bas ./build

-include $(DEPS)

MKDIR_P ?= mkdir -p
