#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <libusb-1.0/libusb.h>
#include <stdint.h>

#define GOOGLE_VENDOR_ID 6353 // 0x18D1
extern const uint16_t GOOGLE_PRODUCT_IDS[2];

libusb_device_handle* find_controller(libusb_context* ctx);
int claim_controller(libusb_device_handle* dev_handle);
int get_version(libusb_device_handle* dev_handle, uint32_t* out_version);
int get_battery(libusb_device_handle* dev_handle, uint16_t* out_battery);
int get_serial_number(libusb_device_handle* dev_handle, char* serial, int max_len);
const char* classify_variant(const char* serial_number);

#endif // CONTROLLER_H
