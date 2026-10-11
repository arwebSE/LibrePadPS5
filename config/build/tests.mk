TEST_BUILD := build/tests
.PHONY: payload-test bluetooth-test xbox-test dashboard-test

BT_CORE := vendor/anypad/util.c vendor/anypad/log.c vendor/anypad/crc32.c vendor/anypad/smp_crypto.c vendor/anypad/profiles.c vendor/anypad/generic.c vendor/anypad/host.c vendor/anypad/le.c
BT_FLAGS := -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -Ivendor/anypad
.PHONY: bluetooth-test
bluetooth-test:
	@mkdir -p $(TEST_BUILD)
	clang $(BT_FLAGS) $(BT_CORE) tests/bluetooth/sim.c tests/bluetooth/test_host.c -o $(TEST_BUILD)/test_bt_host
	./$(TEST_BUILD)/test_bt_host 2>$(TEST_BUILD)/bt-host.log
	clang $(BT_FLAGS) $(BT_CORE) tests/bluetooth/sim_le.c tests/bluetooth/test_le.c -o $(TEST_BUILD)/test_bt_le
	./$(TEST_BUILD)/test_bt_le 2>$(TEST_BUILD)/bt-le.log
	clang $(BT_FLAGS) $(BT_CORE) tests/bluetooth/test_profiles.c -o $(TEST_BUILD)/test_bt_profiles
	./$(TEST_BUILD)/test_bt_profiles 2>$(TEST_BUILD)/bt-profiles.log
	clang $(BT_FLAGS) vendor/anypad/smp_crypto.c tests/bluetooth/test_crypto.c -o $(TEST_BUILD)/test_bt_crypto
	./$(TEST_BUILD)/test_bt_crypto
	clang $(BT_FLAGS) vendor/anypad/usb_desc.c vendor/anypad/evstream.c tests/bluetooth/test_usb.c -o $(TEST_BUILD)/test_bt_usb
	./$(TEST_BUILD)/test_bt_usb
	clang $(BT_FLAGS) $(BT_CORE) tests/bluetooth/test_fuzz.c -o $(TEST_BUILD)/test_bt_fuzz
	./$(TEST_BUILD)/test_bt_fuzz 2>$(TEST_BUILD)/bt-fuzz.log
	clang $(BT_FLAGS) -Isrc -pthread $(BT_CORE) tests/bluetooth/sim.c tests/test_bluetooth_adapter.c src/input/controller_profiles.c src/input/protocol_decoder.c -o $(TEST_BUILD)/test_bt_adapter
	./$(TEST_BUILD)/test_bt_adapter 2>$(TEST_BUILD)/bt-adapter.log

payload-test: check
	python3 tools/embed_dashboard.py
	@mkdir -p $(TEST_BUILD)
	clang++ -std=c++11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_controller_router.cpp -o $(TEST_BUILD)/test_controller_router
	./$(TEST_BUILD)/test_controller_router
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_usb_reports.c src/input/protocol_decoder.c -o $(TEST_BUILD)/test_usb_reports
	./$(TEST_BUILD)/test_usb_reports
	clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Isrc tests/test_protocols.c src/input/protocol_decoder.c -o $(TEST_BUILD)/test_protocols
	./$(TEST_BUILD)/test_protocols
	clang -std=c11 -Wall -Wextra -Werror -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections -fsanitize=address,undefined -g -Isrc -Ivendor/anypad -Itests/stubs tests/test_usb_transport.c -o $(TEST_BUILD)/test_usb_transport
	./$(TEST_BUILD)/test_usb_transport
	clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Isrc -Itests/stubs tests/test_startup.c -o $(TEST_BUILD)/test_startup
	./$(TEST_BUILD)/test_startup
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_controller_profiles.c src/input/controller_profiles.c src/input/protocol_decoder.c -o $(TEST_BUILD)/test_controller_profiles
	./$(TEST_BUILD)/test_controller_profiles
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc -Ibuild/generated tests/test_dashboard.c tests/mock_bluetooth.c src/input/controller_profiles.c src/input/protocol_decoder.c -o $(TEST_BUILD)/test_dashboard
	./$(TEST_BUILD)/test_dashboard
	clang -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -g -Isrc tests/test_lifecycle.c src/core/time_utils.c -ldl -o $(TEST_BUILD)/test_lifecycle
	./$(TEST_BUILD)/test_lifecycle

xbox-test:
	python3 tests/xbox/run.py --sanitize

dashboard-test:
	node --check web/dashboard.js
	node tests/test_dashboard_tabs.js
	node tests/test_dashboard_support.js
	node tests/test_dashboard_xbox.js
