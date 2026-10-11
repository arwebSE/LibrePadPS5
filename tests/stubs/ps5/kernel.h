/* Startup regression only: no SDK or console kernel access. */
#ifndef TEST_KERNEL_H
#define TEST_KERNEL_H
#include <stdint.h>
uint32_t kernel_get_fw_version(void);
#endif
