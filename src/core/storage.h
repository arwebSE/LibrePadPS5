#ifndef LIBREPAD_STORAGE_H
#define LIBREPAD_STORAGE_H

/* Prepare/migrate storage and acquire the shared instance lock. Returns an
 * owned descriptor to close on shutdown, or -1 with errno set. */
int librepad_storage_open(void);

#endif
