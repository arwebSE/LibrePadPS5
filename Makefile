# LibrePadPS5: configurable USB and opt-in Bluetooth gamepads for PS5.

PS5_HOST ?=
PS5_PORT ?= 9021

BUILD := build

.PHONY: all freepad send clean test freepad-test host-test

all: freepad

freepad:
ifndef PS5_PAYLOAD_SDK
	$(error PS5_PAYLOAD_SDK is undefined. Please export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk)
endif
	$(MAKE) -f freepad.mk

send: freepad
ifeq ($(strip $(PS5_HOST)),)
	$(error Set PS5_HOST to your console's address)
endif
	@echo "[*] Sending payload to PS5 $(PS5_HOST):$(PS5_PORT)..."
	python3 -c "import socket; f=open('dist/LibrePadPS5.elf','rb').read(); s=socket.create_connection(('$(PS5_HOST)', $(PS5_PORT)), timeout=5); s.sendall(f); s.close(); print('Payload sent')"

test: freepad-test bluetooth-test

BT_CORE := vendor/anypad/util.c vendor/anypad/log.c vendor/anypad/crc32.c vendor/anypad/smp_crypto.c vendor/anypad/profiles.c vendor/anypad/generic.c vendor/anypad/host.c vendor/anypad/le.c
BT_FLAGS := -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -Ivendor/anypad
.PHONY: bluetooth-test
bluetooth-test:
	@mkdir -p $(BUILD)
	clang $(BT_FLAGS) $(BT_CORE) tests/bluetooth/sim.c tests/bluetooth/test_host.c -o $(BUILD)/test_bt_host
	./$(BUILD)/test_bt_host 2>$(BUILD)/bt-host.log
	clang $(BT_FLAGS) $(BT_CORE) tests/bluetooth/sim_le.c tests/bluetooth/test_le.c -o $(BUILD)/test_bt_le
	./$(BUILD)/test_bt_le 2>$(BUILD)/bt-le.log
	clang $(BT_FLAGS) $(BT_CORE) tests/bluetooth/test_profiles.c -o $(BUILD)/test_bt_profiles
	./$(BUILD)/test_bt_profiles 2>$(BUILD)/bt-profiles.log
	clang $(BT_FLAGS) vendor/anypad/smp_crypto.c tests/bluetooth/test_crypto.c -o $(BUILD)/test_bt_crypto
	./$(BUILD)/test_bt_crypto
	clang $(BT_FLAGS) vendor/anypad/usb_desc.c vendor/anypad/evstream.c tests/bluetooth/test_usb.c -o $(BUILD)/test_bt_usb
	./$(BUILD)/test_bt_usb
	clang $(BT_FLAGS) $(BT_CORE) tests/bluetooth/test_fuzz.c -o $(BUILD)/test_bt_fuzz
	./$(BUILD)/test_bt_fuzz 2>$(BUILD)/bt-fuzz.log
	clang $(BT_FLAGS) -Isrc -pthread $(BT_CORE) tests/bluetooth/sim.c tests/test_bluetooth_adapter.c src/input/controller_profiles.c src/input/protocol_decoder.c -o $(BUILD)/test_bt_adapter
	./$(BUILD)/test_bt_adapter 2>$(BUILD)/bt-adapter.log

freepad-test:
	python3 tools/check_style.py
	python3 tools/embed_dashboard.py
	@mkdir -p $(BUILD)
	clang++ -std=c++11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_controller_router.cpp -o $(BUILD)/test_controller_router
	./$(BUILD)/test_controller_router
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad.c src/input/protocol_decoder.c -o $(BUILD)/test_freepad
	./$(BUILD)/test_freepad
	clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Isrc tests/test_protocols.c src/input/protocol_decoder.c -o $(BUILD)/test_protocols
	./$(BUILD)/test_protocols
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad_profiles.c src/input/controller_profiles.c src/input/protocol_decoder.c -o $(BUILD)/test_freepad_profiles
	./$(BUILD)/test_freepad_profiles
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc -Ibuild/generated tests/test_freepad_dashboard.c tests/mock_bluetooth.c src/input/controller_profiles.c src/input/protocol_decoder.c -o $(BUILD)/test_freepad_dashboard
	./$(BUILD)/test_freepad_dashboard
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad_lifecycle.c src/core/time_utils.c -ldl -o $(BUILD)/test_freepad_lifecycle
	./$(BUILD)/test_freepad_lifecycle

# Compatibility alias for the supported host regression suites.
host-test: freepad-test

clean:
	rm -rf $(BUILD)
