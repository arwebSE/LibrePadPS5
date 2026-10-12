#ifndef LIBREPAD_STORAGE_H
#define LIBREPAD_STORAGE_H

/* Prepare/migrate storage and lock both old and current namespaces. Returns
 * a handle to release with librepad_storage_close(), or -1 with errno set. */
int librepad_storage_open(void);
void librepad_storage_close(int fd);

/* The operation/path that failed during startup, available before logging. */
const char *librepad_storage_error_stage(void);

#endif
