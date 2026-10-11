#ifndef LIBREPAD_XBOX_H
#define LIBREPAD_XBOX_H
#ifdef __cplusplus
extern "C" {
#endif
/* Optional native backend; the shared controller router reserves its own slot.
 * start is nonblocking; poll runs on the application's controller loop.
 * stop joins the radio worker and releases its pad before application shutdown.
 * Model 1697 / 045e:02e6 is verified on PS5 12.60. */
int librepad_xbox_start(void);
void librepad_xbox_poll(long now);
int librepad_xbox_is_connected(void);
/* Queue a restart after dashboard installation. Executed by poll, never HTTP. */
void librepad_xbox_reload(void);
void librepad_xbox_stop(void);
#ifdef __cplusplus
}
#endif
#endif
