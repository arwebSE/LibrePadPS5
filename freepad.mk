include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
.DEFAULT_GOAL := dist/FreePadUSB.elf

ELF := dist/FreePadUSB.elf
BUILD := build/freepad
CFLAGS := -std=c11 -Wall -Wextra -O2 -ffunction-sections -fdata-sections -Isrc -Ibuild/generated
LDFLAGS += -Wl,--gc-sections
LDLIBS += -lScePad -lSceUserService -lSceSystemService -lSceAppInstUtil -ldl

SRCS := src/core/time_utils.c src/core/logging.c src/input/controller_profiles.c src/input/usb_device.c \
        src/ps5/system_bridge.c src/ps5/virtual_controller.c src/dashboard/http_server.c src/app/main.c
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
HEADERS := $(wildcard src/core/*.h src/input/*.h src/ps5/*.h src/dashboard/*.h)

build/generated/dashboard/assets.h: web/index.html web/dashboard.css web/dashboard.js tools/embed_dashboard.py
	python3 tools/embed_dashboard.py

$(BUILD)/dashboard/http_server.o: build/generated/dashboard/assets.h

$(ELF): $(OBJS)
	@mkdir -p dist
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "Built $@"

$(BUILD)/%.o: src/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rf $(BUILD)
