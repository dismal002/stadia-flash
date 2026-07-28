#ifndef SDP_H
#define SDP_H

#include <stdint.h>
#include <stddef.h>

#define SDP_VENDOR_ID 8137 // 0x1FC9
#define SDP_PRODUCT_ID 309 // 0x0135

typedef struct hid_device_ sdp_device_t; // hid_device*

sdp_device_t* sdp_open(const char* path, int timeout_ms);
void sdp_close(sdp_device_t* dev);
int sdp_upload_and_run(sdp_device_t* dev, const uint8_t* image, size_t length, void (*progress)(const char*, float, int*), int* last_pct);

#endif // SDP_H
