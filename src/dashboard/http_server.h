#ifndef LIBREPAD_HTTP_SERVER_H
#define LIBREPAD_HTTP_SERVER_H

int librepad_web_init(int port);
void librepad_web_cleanup(void);
void librepad_request_stop(void);

#endif
