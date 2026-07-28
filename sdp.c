#include "sdp.h"
#include <hidapi/hidapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

#define CMD_WRITE_FILE 1028
#define CMD_JUMP_ADDRESS 2827
#define LOAD_ADDRESS 536870912
#define JUMP_ADDRESS 536871936

#define REPORT_CMD 1
#define REPORT_DATA 2
#define REPORT_STATUS 4

#define CMD_REPORT_PAYLOAD_SIZE 16
#define STATUS_REPORT_PAYLOAD_SIZE 4

struct sdp_device_internal {
    hid_device* handle;
    int data_report_size;
    int timeout_ms;
};

sdp_device_t* sdp_open(const char* path, int timeout_ms) {
    hid_device* handle;
    if (path) handle = hid_open_path(path);
    else handle = hid_open(SDP_VENDOR_ID, SDP_PRODUCT_ID, NULL);
    if (!handle) return NULL;
    
    struct sdp_device_internal* dev = malloc(sizeof(struct sdp_device_internal));
    dev->handle = handle;
    
    int sizes[256];
    get_hid_report_sizes(handle, sizes, 256);
    dev->data_report_size = sizes[REPORT_DATA] > 0 ? sizes[REPORT_DATA] : 1024;
    
    dev->timeout_ms = timeout_ms;
    return (sdp_device_t*)dev;
}

void sdp_close(sdp_device_t* dev) {
    if (!dev) return;
    struct sdp_device_internal* d = (struct sdp_device_internal*)dev;
    hid_close(d->handle);
    free(d);
}

static void sdp_send_command(struct sdp_device_internal* dev, uint16_t tag, uint32_t address, uint8_t fmt, uint32_t count, uint32_t data) {
    uint8_t buf[CMD_REPORT_PAYLOAD_SIZE + 1];
    memset(buf, 0, sizeof(buf));
    buf[0] = REPORT_CMD;
    buf[1] = (tag >> 8) & 0xFF; buf[2] = tag & 0xFF;
    buf[3] = (address >> 24) & 0xFF; buf[4] = (address >> 16) & 0xFF; buf[5] = (address >> 8) & 0xFF; buf[6] = address & 0xFF;
    buf[7] = fmt;
    buf[8] = (count >> 24) & 0xFF; buf[9] = (count >> 16) & 0xFF; buf[10] = (count >> 8) & 0xFF; buf[11] = count & 0xFF;
    buf[12] = (data >> 24) & 0xFF; buf[13] = (data >> 16) & 0xFF; buf[14] = (data >> 8) & 0xFF; buf[15] = data & 0xFF;
    buf[16] = 0;
    hid_write(dev->handle, buf, sizeof(buf));
}

static int sdp_read_status(struct sdp_device_internal* dev, uint32_t* out_status) {
    uint8_t buf[STATUS_REPORT_PAYLOAD_SIZE + 4];
    memset(buf, 0, sizeof(buf));
    int res = hid_read_timeout(dev->handle, buf, sizeof(buf), dev->timeout_ms);
    if (res < STATUS_REPORT_PAYLOAD_SIZE + 1) return -1;
    *out_status = ((uint32_t)buf[1] << 24) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 8) | (uint32_t)buf[4];
    return 0;
}

int sdp_upload_and_run(sdp_device_t* dev, const uint8_t* image, size_t length, void (*progress)(const char*, float, int*), int* last_pct) {
    struct sdp_device_internal* d = (struct sdp_device_internal*)dev;
    sdp_send_command(d, CMD_WRITE_FILE, LOAD_ADDRESS, 0, length, 0);
    size_t sent = 0;
    uint8_t* buf = malloc(d->data_report_size + 1);
    while (sent < length) {
        size_t end = sent + d->data_report_size;
        if (end > length) end = length;
        size_t chunk_len = end - sent;
        
        memset(buf, 0, d->data_report_size + 1);
        buf[0] = REPORT_DATA;
        memcpy(buf + 1, image + sent, chunk_len);
        hid_write(d->handle, buf, d->data_report_size + 1);
        
        sent = end;
        if (progress) progress("Uploading", (float)sent / length * 100.0f, last_pct);
    }
    free(buf);
    uint32_t status = 0;
    if (sdp_read_status(d, &status) < 0) return -1;
    sdp_send_command(d, CMD_JUMP_ADDRESS, JUMP_ADDRESS, 0, 0, 0);
    return status;
}
