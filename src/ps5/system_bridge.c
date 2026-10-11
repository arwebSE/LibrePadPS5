#include "ps5/system_bridge.h"
#include "core/logging.h"
#include <stdint.h>
#include <unistd.h>
#ifdef __PROSPERO__
#include <ps5/kernel.h>
static uint64_t g_orig_authid = 0;
static uint8_t  g_orig_caps[16] = {0};
static int      g_has_orig = 0;
static int      g_is_elevated = 0;

#endif

int elevate_privileges(void)
{
#ifdef __PROSPERO__
    if (g_is_elevated) return 1;

    pid_t me = getpid();
    static const uint8_t root_caps[16] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    if (!g_has_orig) {
        g_orig_authid = kernel_get_ucred_authid(me);
        if (g_orig_authid && kernel_get_ucred_caps(me, g_orig_caps) == 0) {
            g_has_orig = 1;
        }
    }

    /* System service credentials for local MBus access */
    int set_auth_res = kernel_set_ucred_authid(me, 0x4800000000010003ULL);
    int set_caps_res = kernel_set_ucred_caps(me, root_caps);

    if (set_auth_res == 0 && set_caps_res == 0) {
        g_is_elevated = 1;
        return 1;
    }

    /* Under kstuff 1.13, process may already hold root credentials */
    if (getuid() == 0 || geteuid() == 0) {
        g_is_elevated = 1;
        return 1;
    }

    log_line("system: kernel_set_ucred failed and process is not root");
    return 0;
#else
    return 1;
#endif
}

void restore_privileges(void)
{
#ifdef __PROSPERO__
    if (g_has_orig && g_is_elevated) {
        pid_t me = getpid();
        kernel_set_ucred_authid(me, g_orig_authid);
        kernel_set_ucred_caps(me, g_orig_caps);
        g_is_elevated = 0;
    }
#endif
}
