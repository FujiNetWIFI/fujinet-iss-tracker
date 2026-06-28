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

DIST_DIR := ./dist

SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

.PHONY: all disk release clean

all: disk

release:
	mkdir -p $(DIST_DIR)
	$(MAKE) iss.bin
	$(MAKE) COCO3=1 iss3.bin
	$(CC) -oloader.bin loader.c
	cp iss.bin iss3.bin loader.bin $(DIST_DIR)/

# loader.bin detects the machine and launches iss3.bin (CoCo 3) or iss.bin (CoCo 1/2).
disk: release
	$(RM) $(DIST_DIR)/iss.dsk
	decb dskini $(DIST_DIR)/iss.dsk
	printf '10 RUNM"LOADER"\n' > autoexec.bas
	decb copy -0 -a -l -r autoexec.bas $(DIST_DIR)/iss.dsk,AUTOEXEC.BAS
	writecocofile $(DIST_DIR)/iss.dsk $(DIST_DIR)/loader.bin
	writecocofile $(DIST_DIR)/iss.dsk $(DIST_DIR)/iss.bin
	writecocofile $(DIST_DIR)/iss.dsk $(DIST_DIR)/iss3.bin

$(TARGET_EXEC).bin: $(OBJS)
	$(CC) -o$@ $(LDFLAGS_EXTRA) $(OBJS)

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -r iss iss3 iss.bin iss3.bin loader.bin autoexec.bas ./build $(DIST_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
