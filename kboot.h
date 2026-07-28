#ifndef KBOOT_H
#define KBOOT_H

#include <stdint.h>
#include <stddef.h>
#include "firmware.h"

#define KBOOT_VENDOR_ID 5538 // 0x15A2
#define KBOOT_PRODUCT_ID 115 // 0x0073

typedef struct hid_device_ kboot_device_t; // hid_device*

kboot_device_t* kboot_open(const char* path, int timeout_ms);
void kboot_close(kboot_device_t* dev);
int kboot_flash_firmware(kboot_device_t* dev, const uint8_t* image, size_t image_size, const build_info_t* build_info, const uint8_t* vendor_probe, size_t vendor_probe_size, const uint8_t* winbond_fcb, size_t winbond_fcb_size, void (*progress)(const char*, float, int*), int* last_pct);

#endif // KBOOT_H
