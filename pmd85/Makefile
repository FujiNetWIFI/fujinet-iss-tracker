CC=zcc
TARGET_EXEC ?= iss

TARGET=+pmd85
#CFLAGS=--max-allocs-per-node200000 -SO3

LIB_FUJINET := -l../_libs/4.7.4-pmd85/fujinet-pmd85-4.7.4.lib
LIB_FUJINET_INC := -I../_libs/4.7.4-pmd85/

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src

.PHONY: clean all

ROM_FILE = $(BUILD_DIR)/$(TARGET_EXEC).rmm

all: $(ROM_FILE)

$(BUILD_DIR)/$(TARGET_EXEC):
	$(MKDIR_P) $(dir $@)
	${CC} ${TARGET} -m ${CFLAGS} ${LIB_FUJINET_INC} ${LIB_FUJINET} @src/iss.lst -o $@ -create-app
#	${CC} ${TARGET} -m -v --list ${CFLAGS} ${LIB_FUJINET_INC} ${LIB_FUJINET} @src/iss.lst -o $@ -create-app

$(ROM_FILE): $(BUILD_DIR)/$(TARGET_EXEC)
	echo "creating $(ROM_FILE) from program $(TARGET_EXEC)" ; \
	if [ -f "$(ROM_FILE)" ] ; then \
		rm "$(ROM_FILE)" ; \
	fi ; \
	(/bin/printf '\xcd\x00\x8c\x00\x00\xff\x47\x00\x00'; \
	dd if=$(BUILD_DIR)/$(TARGET_EXEC) count=4 bs=1 status=none; \
	dd if=$(BUILD_DIR)/$(TARGET_EXEC) skip=13 bs=1 status=none) > $(ROM_FILE)

clean:
	$(RM) -r $(BUILD_DIR)

MKDIR_P ?= mkdir -p
