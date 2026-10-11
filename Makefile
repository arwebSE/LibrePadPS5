# FreePadUSB — configurable wired USB controllers for PS5.

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
	python3 -c "import socket; f=open('dist/FreePadUSB.elf','rb').read(); s=socket.create_connection(('$(PS5_HOST)', $(PS5_PORT)), timeout=5); s.sendall(f); s.close(); print('Payload sent')"

test: freepad-test

freepad-test:
	python3 tools/embed_dashboard.py
	@mkdir -p $(BUILD)
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad.c src/legacy/report_decoder.c -o $(BUILD)/test_freepad
	./$(BUILD)/test_freepad
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad_profiles.c src/input/controller_profiles.c src/legacy/report_decoder.c -o $(BUILD)/test_freepad_profiles
	./$(BUILD)/test_freepad_profiles
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc -Ibuild/generated tests/test_freepad_dashboard.c src/input/controller_profiles.c src/legacy/report_decoder.c -o $(BUILD)/test_freepad_dashboard
	./$(BUILD)/test_freepad_dashboard
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_freepad_lifecycle.c src/core/time_utils.c -ldl -o $(BUILD)/test_freepad_lifecycle
	./$(BUILD)/test_freepad_lifecycle

# Compatibility alias for the supported host regression suites.
host-test: freepad-test

clean:
	rm -rf $(BUILD)
