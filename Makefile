# FreePad USB — wired Switch controllers for PS5.
# The ps5 target retains the upstream OmniPad build for development only.

PS5_HOST ?=
PS5_PORT ?= 9021

BUILD := build

.PHONY: all freepad ps5 send clean test freepad-test host-test

all: freepad

freepad:
ifndef PS5_PAYLOAD_SDK
	$(error PS5_PAYLOAD_SDK is undefined. Please export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk)
endif
	$(MAKE) -f freepad.mk

ps5:
ifndef PS5_PAYLOAD_SDK
	$(error PS5_PAYLOAD_SDK is undefined. Please export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk)
endif
	$(MAKE) -f ps5.mk

send: freepad
ifeq ($(strip $(PS5_HOST)),)
	$(error Set PS5_HOST to your console's address)
endif
	@echo "[*] Sending payload to PS5 $(PS5_HOST):$(PS5_PORT)..."
	python3 -c "import socket; f=open('dist/FreePad-USB.elf','rb').read(); s=socket.create_connection(('$(PS5_HOST)', $(PS5_PORT)), timeout=5); s.sendall(f); s.close(); print('Payload sent')"

test: freepad-test

freepad-test:
	@mkdir -p $(BUILD)
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad.c src/usb_controllers.c -o $(BUILD)/test_freepad
	./$(BUILD)/test_freepad
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad_profiles.c src/freepad_profiles.c src/usb_controllers.c -o $(BUILD)/test_freepad_profiles
	./$(BUILD)/test_freepad_profiles
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad_dashboard.c src/freepad_profiles.c src/usb_controllers.c -o $(BUILD)/test_freepad_dashboard
	./$(BUILD)/test_freepad_dashboard
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad_lifecycle.c src/util.c -ldl -o $(BUILD)/test_freepad_lifecycle
	./$(BUILD)/test_freepad_lifecycle

host-test:
	@mkdir -p $(BUILD)
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_parsers.c -o $(BUILD)/test_parsers
	./$(BUILD)/test_parsers
	clang -O2 -Isrc tests/test_vpad_abi.c -o $(BUILD)/test_vpad_abi
	./$(BUILD)/test_vpad_abi
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_flow_sim.c -o $(BUILD)/test_flow_sim
	./$(BUILD)/test_flow_sim
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_normalization.c -o $(BUILD)/test_normalization
	./$(BUILD)/test_normalization
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_lifecycle_logout.c -o $(BUILD)/test_lifecycle_logout
	./$(BUILD)/test_lifecycle_logout
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_fuzz_usb.c -o $(BUILD)/test_fuzz_usb
	./$(BUILD)/test_fuzz_usb
	clang -O2 -Isrc -pthread src/util.c src/log.c tests/test_hotplug_thrashing.c -o $(BUILD)/test_hotplug_thrashing
	./$(BUILD)/test_hotplug_thrashing
	clang -O2 -Isrc src/util.c src/log.c tests/test_web_concurrency.c -o $(BUILD)/test_web_concurrency
	./$(BUILD)/test_web_concurrency
	clang -O2 -Isrc src/usb_controllers.c src/profiles.c src/util.c src/log.c tests/test_dongle_cable_switch.c -o $(BUILD)/test_dongle_cable_switch
	./$(BUILD)/test_dongle_cable_switch

clean:
	rm -rf $(BUILD)
