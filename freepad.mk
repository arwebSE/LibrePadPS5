include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
.DEFAULT_GOAL := dist/LibrePadPS5.elf

ELF := dist/LibrePadPS5.elf
BUILD := build/freepad
CFLAGS := -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -O2 -ffunction-sections -fdata-sections -Isrc -Ivendor/anypad -Ibuild/generated
LDFLAGS += -Wl,--gc-sections
LDLIBS += -lScePad -lSceUserService -lSceSystemService -lSceAppInstUtil -ldl

SRCS := src/core/time_utils.c src/core/logging.c src/input/controller_profiles.c src/input/usb_device.c \
        src/input/protocol_decoder.c src/bluetooth/controller_host.c \
        src/ps5/system_bridge.c src/ps5/virtual_controller.c src/dashboard/http_server.c src/app/main.c \
        vendor/anypad/util.c vendor/anypad/crc32.c vendor/anypad/smp_crypto.c \
        vendor/anypad/profiles.c vendor/anypad/generic.c vendor/anypad/host.c vendor/anypad/le.c \
        vendor/anypad/hci_usb.c vendor/anypad/usb_desc.c vendor/anypad/evstream.c
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
HEADERS := $(wildcard src/core/*.h src/input/*.h src/ps5/*.h src/dashboard/*.h src/bluetooth/*.h vendor/anypad/*.h)

build/generated/dashboard/assets.h: web/index.html web/dashboard.css web/dashboard.js tools/embed_dashboard.py
	python3 tools/embed_dashboard.py

$(BUILD)/src/dashboard/http_server.o: build/generated/dashboard/assets.h

$(ELF): $(OBJS)
	@mkdir -p dist
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "Built $@"

$(BUILD)/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rf $(BUILD)
