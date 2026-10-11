include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
ELF := dist/LibrePadPS5-Xbox-Experimental.elf
BUILD := build/xbox
ROOT := experimental/xbox
CFLAGS := -std=c11 -Wall -Wextra -O2 -MMD -MP -DFREEPAD_ONLY -DLIBREPAD_XBOX_EXPERIMENT -Isrc -ffunction-sections -fdata-sections
CXXFLAGS := -std=c++11 -Wall -Wextra -O2 -MMD -MP -DFREEPAD_ONLY -DFIRMWARE=\"/data/pdp-pad/xbox/firmware.bin\" -Isrc -ffunction-sections -fdata-sections
CSRC := src/util.c src/log.c src/usb_controllers.c src/freepad_profiles.c src/shellui_inject.c src/ps5_vpad.c src/web_freepad.c src/main_freepad.c
MODULESRC := $(ROOT)/port/backend.cpp $(ROOT)/port/usb.cpp $(ROOT)/port/controller.cpp $(ROOT)/port/log.cpp $(ROOT)/vendor/xow/dongle/mt76.cpp $(ROOT)/vendor/xow/dongle/dongle.cpp $(ROOT)/vendor/xow/controller/gip.cpp
CXXSRC := $(MODULESRC) $(ROOT)/port/standalone.cpp
OBJS := $(addprefix $(BUILD)/,$(CSRC:.c=.o) $(CXXSRC:.cpp=.o))
$(ELF): $(OBJS)
	@mkdir -p dist
	$(CXX) -Wl,--gc-sections -o $@ $^ -lScePad -lSceUserService -lSceSystemService -lSceAppInstUtil -ldl
dist/liblibrepad_xbox.a: $(addprefix $(BUILD)/,$(MODULESRC:.cpp=.o))
	@mkdir -p dist
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
