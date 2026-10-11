#ifndef USB_HOTPLUG_H
#define USB_HOTPLUG_H

int usb_hotplug_init(void);
void usb_hotplug_poll(long now);
void usb_hotplug_cleanup(void);
int usb_hotplug_librepad_connected(void);

#endif /* USB_HOTPLUG_H */
