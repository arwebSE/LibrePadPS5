#include "bluetooth/controller_host.h"
#include <stdio.h>
int bluetooth_enabled(void) { return 0; }
int bluetooth_pair_address(const char *address) { (void)address; return 0; }
int bluetooth_disable(void) { return 1; }
int bluetooth_forget(void) { return 1; }
int bluetooth_reconnect(void) { return 0; }
void bluetooth_status(char *out, size_t size) {
    snprintf(out,size,"{\"enabled\":false,\"connected\":false,\"pairing\":0,\"message\":\"Host-only preview\"}");
}
