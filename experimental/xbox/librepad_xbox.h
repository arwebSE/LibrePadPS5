#ifndef LIBREPAD_XBOX_H
#define LIBREPAD_XBOX_H
#ifdef __cplusplus
extern "C" {
#endif
/* Optional native backend for virtual slot zero. Use one slot-zero input source.
 * start is nonblocking; poll runs on the application's controller loop.
 * stop joins the radio worker and releases its pad before application shutdown.
 * Model 1697 / 045e:02e6 is the current experimental target. */
int librepad_xbox_start(void);
void librepad_xbox_poll(long now);
int librepad_xbox_is_connected(void);
void librepad_xbox_stop(void);
#ifdef __cplusplus
}
#endif
#endif
