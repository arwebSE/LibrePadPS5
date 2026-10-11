#ifndef LIBREPAD_BLUETOOTH_HOST_H
#define LIBREPAD_BLUETOOTH_HOST_H
#include <stddef.h>
/* Commands are queued by HTTP, executed only on the main input thread. */
int bluetooth_pair_address(const char *address);
int bluetooth_disable(void);
int bluetooth_reconnect(void);
int bluetooth_forget(void);
int bluetooth_enabled(void);
void bluetooth_status(char *out, size_t size);
void bluetooth_poll(void);
void bluetooth_cleanup(void);
#endif
