#ifndef LIBREPAD_TEST_USB_ENDIAN_H
#define LIBREPAD_TEST_USB_ENDIAN_H
#define USETW(field,value) do { (field)[0]=(value)&255; (field)[1]=((value)>>8)&255; } while (0)
#define UGETW(field) ((unsigned)(field)[0] | ((unsigned)(field)[1]<<8))
#endif
