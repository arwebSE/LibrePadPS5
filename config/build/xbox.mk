include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
ELF := dist/LibrePadPS5-Xbox.elf
.DEFAULT_GOAL := $(ELF)
BUILD := build/xbox
CFLAGS := -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -O2 -MMD -MP -DLIBREPAD_XBOX_STANDALONE -Isrc -Ibuild/generated -ffunction-sections -fdata-sections
CXXFLAGS := -std=c++11 -Wall -Wextra -O2 -MMD -MP -include core/paths.h -DFIRMWARE=LIBREPAD_XBOX_FIRMWARE_PATH -Isrc -Ivendor -Ibuild/generated -ffunction-sections -fdata-sections
CSRC := src/core/time_utils.c src/core/storage.c src/core/logging.c src/input/protocol_decoder.c src/input/controller_profiles.c src/input/controller_router.c src/ps5/system_bridge.c src/ps5/virtual_controller.c src/dashboard/http_server.c src/app/main.c src/xbox/standalone_bluetooth.c
MODULESRC := src/xbox/backend.cpp src/xbox/usb.cpp src/xbox/controller.cpp src/xbox/firmware.cpp src/xbox/reconnect.cpp src/xbox/log.cpp vendor/xow/dongle/mt76.cpp vendor/xow/dongle/dongle.cpp vendor/xow/controller/gip.cpp
CXXSRC := $(MODULESRC) src/xbox/standalone.cpp
OBJS := $(addprefix $(BUILD)/,$(CSRC:.c=.o) $(CXXSRC:.cpp=.o))
build/generated/dashboard/assets.h: web/dist/index.html web/dist/dashboard.css web/dist/dashboard.js web/dist/favicon.svg web/dist/source-manifest.json $(shell find web/src web/public web/scripts -type f) web/index.html web/package.json web/package-lock.json web/vite.config.js tools/embed_dashboard.py
	python3 tools/embed_dashboard.py
$(BUILD)/src/dashboard/http_server.o: build/generated/dashboard/assets.h
build/generated/xbox/firmware_blob.h: vendor/xbox/FW_ACC_00U.bin tools/embed_xbox_firmware.py
	python3 tools/embed_xbox_firmware.py
$(BUILD)/src/xbox/firmware.o: build/generated/xbox/firmware_blob.h
$(ELF): $(OBJS)
	@mkdir -p dist
	$(CXX) -Wl,--gc-sections -o $@ $^ -lScePad -lSceUserService -lSceSystemService -lSceAppInstUtil -ldl
dist/liblibrepad_xbox.a: $(addprefix $(BUILD)/,$(MODULESRC:.cpp=.o))
	@mkdir -p dist
	@rm -f $@
	$(AR) rcs $@ $^
.PHONY: xbox-module
xbox-module: dist/liblibrepad_xbox.a
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
-include $(OBJS:.o=.d)
