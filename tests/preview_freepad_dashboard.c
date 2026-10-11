/* Disposable host-only preview. All console APIs mocked by the HTTP test. */
#define main dashboard_test_main
#include "test_freepad_dashboard.c"
#undef main

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    mkdir("/data",0700); mkdir(FREEPAD_STATE_DIR,0700);
    if (!freepad_profiles_init(FREEPAD_PROFILES_PATH)) return 1;
    mock_pad.status = VP_READY; mock_pad.user_id = 0x1234;
    mock_pad.lx = mock_pad.ly = mock_pad.rx = mock_pad.ry = 128;
    if (!freepad_web_init(18196)) return 1;
    for (int i = 0; i < 600 && !stopped; i++) sleep(1);
    freepad_web_cleanup();
    return 0;
}
