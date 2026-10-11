include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
.DEFAULT_GOAL := dist/LibrePadPS5.elf

ELF := dist/LibrePadPS5.elf
WITH_XBOX ?= 1
BUILD := build/librepad-xbox$(WITH_XBOX)
CFLAGS := -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -O2 -ffunction-sections -fdata-sections -Isrc -Ivendor/anypad -Ibuild/generated
LDFLAGS += -Wl,--gc-sections
LDLIBS += -lScePad -lSceUserService -lSceSystemService -lSceAppInstUtil -ldl

SRCS := src/core/time_utils.c src/core/storage.c src/core/logging.c src/input/controller_profiles.c src/input/usb_device.c src/input/controller_router.c \
        src/input/protocol_decoder.c src/bluetooth/controller_host.c \
        src/ps5/system_bridge.c src/ps5/virtual_controller.c src/dashboard/http_server.c src/app/main.c \
        vendor/anypad/util.c vendor/anypad/crc32.c vendor/anypad/smp_crypto.c \
        vendor/anypad/profiles.c vendor/anypad/generic.c vendor/anypad/host.c vendor/anypad/le.c \
        vendor/anypad/hci_usb.c vendor/anypad/usb_desc.c vendor/anypad/evstream.c
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
HEADERS := $(wildcard src/core/*.h src/input/*.h src/ps5/*.h src/dashboard/*.h src/bluetooth/*.h src/xbox/*.h vendor/anypad/*.h)

build/generated/dashboard/assets.h: web/index.html web/dashboard.css web/dashboard.js web/favicon.svg tools/embed_dashboard.py
	python3 tools/embed_dashboard.py

$(BUILD)/src/dashboard/http_server.o: build/generated/dashboard/assets.h

LINKER := $(CC)
ifeq ($(WITH_XBOX),1)
CFLAGS += -DLIBREPAD_WITH_XBOX
LINKER := $(CXX)
XBOX_ARCHIVE := dist/liblibrepad_xbox.a
.PHONY: xbox-module
xbox-module:
	$(MAKE) -f config/build/xbox.mk xbox-module
$(XBOX_ARCHIVE): xbox-module
endif

$(ELF): $(OBJS) $(XBOX_ARCHIVE)
	@mkdir -p dist
	$(LINKER) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "Built $@"

$(BUILD)/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Relink when switching WITH_XBOX, even if both object variants already exist.
.PHONY: clean $(ELF)
clean:
	rm -rf $(BUILD)
