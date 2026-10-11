#ifndef FREEPAD_SYSTEM_BRIDGE_H
#define FREEPAD_SYSTEM_BRIDGE_H

/* Local process credentials only; no remote process injection. */
int elevate_privileges(void);
void restore_privileges(void);

#endif
