/* Standalone Xbox test payload only. Keep slot zero exclusive to its radio.
 * The optional module archive excludes this adapter and all Bluetooth symbols. */
#include "bluetooth/controller_host.h"
#include <stdio.h>
int bluetooth_enabled(void) { return 0; }
int bluetooth_pair_address(const char *address) { (void)address; return 0; }
int bluetooth_disable(void) { return 1; }
int bluetooth_forget(void) { return 0; }
int bluetooth_reconnect(void) { return 0; }
void bluetooth_status(char *out, size_t size) {
    snprintf(out, size, "{\"enabled\":false,\"connected\":false,\"pairing\":0,\"message\":\"Xbox Wireless experimental payload\"}");
}
void bluetooth_poll(void) {}
void bluetooth_cleanup(void) {}
