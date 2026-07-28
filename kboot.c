#include "kboot.h"
#include <hidapi/hidapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"

#define CMD_FLASH_ERASE_REGION 2
#define CMD_READ_MEMORY 3
#define CMD_WRITE_MEMORY 4
#define CMD_FILL_MEMORY 5
#define CMD_RESET 11
#define CMD_CONFIGURE_MEMORY 17

#define RESP_GENERIC 0xA0
#define RESP_READ_MEMORY 0xA3

#define REPORT_CMD 1
#define REPORT_DATA_OUT 2
#define REPORT_STATUS_IN 3
#define REPORT_DATA_IN 4

#define ERASE_CHUNK 16384

#define REG_CHIP_ID 1074627168
#define REG_FLEXSPI_BASE 1076527104
#define REG_FLEXSPI_JEDEC_ID 1076527360
#define REG_GPR16 1074757680
#define REG_GPR17 1074757684
#define REG_GPR18 1074757688

#define FCB_STAGING_ADDR 8192
#define GIGA_FLASH_MARKER 3221225990
#define FLEXSPI_APP_BASE 1610616832

struct kboot_device_internal {
    hid_device* handle;
    int cmd_report_size;
    int data_report_size;
    int timeout_ms;
};

static double monotonic_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

kboot_device_t* kboot_open(const char* path, int timeout_ms) {
    hid_device* handle;
    if (path) handle = hid_open_path(path);
    else handle = hid_open(KBOOT_VENDOR_ID, KBOOT_PRODUCT_ID, NULL);
    if (!handle) return NULL;
    
    struct kboot_device_internal* dev = malloc(sizeof(struct kboot_device_internal));
    dev->handle = handle;
    
    int sizes[256];
    get_hid_report_sizes(handle, sizes, 256);
    dev->cmd_report_size = sizes[REPORT_CMD] > 0 ? sizes[REPORT_CMD] : 32;
    dev->data_report_size = sizes[REPORT_DATA_OUT] > 0 ? sizes[REPORT_DATA_OUT] : 1024;
    
    dev->timeout_ms = timeout_ms;
    return (kboot_device_t*)dev;
}

void kboot_close(kboot_device_t* dev) {
    if (!dev) return;
    struct kboot_device_internal* d = (struct kboot_device_internal*)dev;
    hid_close(d->handle);
    free(d);
}

static void kboot_frame(const uint8_t* payload, size_t payload_len, int report_len, uint8_t* out) {
    memset(out, 0, report_len);
    out[0] = 0;
    out[1] = payload_len & 0xFF;
    out[2] = (payload_len >> 8) & 0xFF;
    memcpy(out + 3, payload, payload_len);
}

static int kboot_send_command(struct kboot_device_internal* dev, uint8_t tag, uint8_t flags, const uint32_t* params, int num_params) {
    uint8_t body[128];
    body[0] = tag;
    body[1] = flags;
    body[2] = 0;
    body[3] = num_params;
    for (int i = 0; i < num_params; i++) {
        body[4 + i * 4] = params[i] & 0xFF;
        body[5 + i * 4] = (params[i] >> 8) & 0xFF;
        body[6 + i * 4] = (params[i] >> 16) & 0xFF;
        body[7 + i * 4] = (params[i] >> 24) & 0xFF;
    }
    uint8_t frame[512];
    kboot_frame(body, 4 + num_params * 4, dev->cmd_report_size, frame);
    uint8_t buf[513];
    buf[0] = REPORT_CMD;
    memcpy(buf + 1, frame, dev->cmd_report_size);
    hid_write(dev->handle, buf, dev->cmd_report_size + 1);
    return 0;
}

static uint8_t* kboot_wait_result(struct kboot_device_internal* dev, int timeout_s, size_t* out_len) {
    double deadline = (timeout_s < 0) ? -1 : monotonic_time() + timeout_s;
    while (1) {
        if (deadline > 0 && monotonic_time() > deadline) return NULL;
        uint8_t raw[1028];
        int res = hid_read_timeout(dev->handle, raw, dev->data_report_size + 4, dev->timeout_ms);
        if (res <= 0) continue;
        uint8_t report_id = raw[0];
        uint16_t length = raw[2] | (raw[3] << 8);
        if (report_id == REPORT_STATUS_IN) {
            // tag is unused, removing `uint8_t tag = raw[4];`
            uint8_t flags = raw[5];
            uint8_t count = raw[7];
            uint32_t status = 0;
            if (count > 0) status = raw[8] | (raw[9] << 8) | (raw[10] << 16) | (raw[11] << 24);
            if (status != 0) return NULL;
            if (flags & 1) continue;
            if (out_len) *out_len = 0;
            return NULL; // Success but no data
        } else if (report_id == REPORT_DATA_IN) {
            uint8_t* data = malloc(length);
            memcpy(data, raw + 4, length);
            if (out_len) *out_len = length;
            return data;
        }
    }
}

static uint32_t kboot_read_u32(struct kboot_device_internal* dev, uint32_t address) {
    uint32_t params[3] = {address, 4, 0};
    kboot_send_command(dev, CMD_READ_MEMORY, 0, params, 3);
    size_t len = 0;
    uint8_t* data = kboot_wait_result(dev, -1, &len);
    if (!data || len != 4) { if(data) free(data); return 0; }
    uint32_t val = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    free(data);
    return val;
}

static void kboot_write_u32(struct kboot_device_internal* dev, uint32_t address, uint32_t value) {
    uint32_t params[3] = {address, 4, value};
    kboot_send_command(dev, CMD_FILL_MEMORY, 0, params, 3);
    kboot_wait_result(dev, -1, NULL);
}

static void kboot_write_memory(struct kboot_device_internal* dev, uint32_t address, const uint8_t* data, size_t length, void (*progress)(const char*, float, int*), int* last_pct) {
    uint32_t params[3] = {address, (uint32_t)length, 0};
    kboot_send_command(dev, CMD_WRITE_MEMORY, 1, params, 3);
    kboot_wait_result(dev, -1, NULL); // initial ack
    size_t chunk_size = dev->data_report_size - 3;
    if (chunk_size > 512) chunk_size = 512;
    size_t sent = 0;
    while (sent < length) {
        size_t end = sent + chunk_size;
        if (end > length) end = length;
        size_t len = end - sent;
        uint8_t frame[512];
        kboot_frame(data + sent, len, dev->data_report_size, frame);
        uint8_t buf[513];
        buf[0] = REPORT_DATA_OUT;
        memcpy(buf + 1, frame, dev->data_report_size);
        hid_write(dev->handle, buf, dev->data_report_size + 1);
        sent = end;
        if (progress) progress("Flashing", (float)sent / length * 100.0f, last_pct);
    }
    kboot_wait_result(dev, -1, NULL); // final ack
}

static void kboot_erase_region(struct kboot_device_internal* dev, uint32_t address, size_t length, void (*progress)(const char*, float, int*), int* last_pct) {
    size_t remaining = length;
    uint32_t addr = address;
    while (remaining > 0) {
        size_t step = (remaining > ERASE_CHUNK) ? ERASE_CHUNK : remaining;
        uint32_t params[3] = {addr, (uint32_t)step, 0};
        kboot_send_command(dev, CMD_FLASH_ERASE_REGION, 0, params, 3);
        kboot_wait_result(dev, -1, NULL);
        addr += step;
        remaining -= step;
        if (progress) progress("Erasing", (float)(length - remaining) / length * 100.0f, last_pct);
    }
}

static void kboot_configure_memory(struct kboot_device_internal* dev) {
    uint32_t params[2] = {9, FCB_STAGING_ADDR};
    kboot_send_command(dev, CMD_CONFIGURE_MEMORY, 0, params, 2);
    kboot_wait_result(dev, -1, NULL);
}

static void kboot_reg_rmw_or_set(struct kboot_device_internal* dev, uint32_t offset, uint32_t value, bool read_modify_write) {
    uint32_t addr = REG_FLEXSPI_BASE + offset;
    if (read_modify_write) {
        uint32_t current = kboot_read_u32(dev, addr);
        uint32_t new_value = current | value;
        if (new_value != current) kboot_write_u32(dev, addr, new_value);
    } else {
        kboot_write_u32(dev, addr, value);
    }
}

int kboot_flash_firmware(kboot_device_t* pdev, const uint8_t* image, size_t image_size, const build_info_t* build_info, const uint8_t* vendor_probe, size_t vendor_probe_size, const uint8_t* winbond_fcb, size_t winbond_fcb_size, void (*progress)(const char*, float, int*), int* last_pct) {
    struct kboot_device_internal* dev = (struct kboot_device_internal*)pdev;
    
    printf("Detecting MCU type...\n");
    uint32_t chip_id = kboot_read_u32(dev, REG_CHIP_ID);
    printf("MCU: 0x%08X\n", chip_id);
    
    printf("Detecting Flash type...\n");
    kboot_write_memory(dev, FCB_STAGING_ADDR, vendor_probe, vendor_probe_size, NULL, NULL);
    kboot_configure_memory(dev);
    kboot_reg_rmw_or_set(dev, 128, 2147483648, true);
    kboot_reg_rmw_or_set(dev, 20, 30, true);
    kboot_reg_rmw_or_set(dev, 160, 0, false);
    kboot_reg_rmw_or_set(dev, 184, 1, false);
    kboot_reg_rmw_or_set(dev, 188, 1, false);
    kboot_reg_rmw_or_set(dev, 164, 2, false);
    kboot_reg_rmw_or_set(dev, 176, 1, false);
    uint32_t value = 0;
    while (value == 0) value = kboot_read_u32(dev, REG_FLEXSPI_JEDEC_ID);
    printf("Flash: 0x%04X\n", value);
    
    printf("Setting up flash\n");
    if (value == 6088) { // Giga-16m
        kboot_write_u32(dev, FCB_STAGING_ADDR, GIGA_FLASH_MARKER);
    } else if (value == 6127) { // Winbond-16m
        if (winbond_fcb) kboot_write_memory(dev, FCB_STAGING_ADDR, winbond_fcb, winbond_fcb_size, NULL, NULL);
    }
    kboot_configure_memory(dev);
    
    printf("Clearing GPR flags\n");
    kboot_write_u32(dev, REG_GPR16, 0);
    kboot_write_u32(dev, REG_GPR17, 0);
    kboot_write_u32(dev, REG_GPR18, 0);
    
    if (build_info->has_ivt) {
        printf("Extracting IVT and flashing\n");
        size_t ivt_len = build_info->fcb_offset;
        kboot_erase_region(dev, FLEXSPI_APP_BASE, ivt_len, progress, last_pct);
        kboot_write_memory(dev, FLEXSPI_APP_BASE, image + build_info->ivt_offset, ivt_len, progress, last_pct);
    }
    
    const partition_t* partition = build_info->partition;
    printf("Flashing to %s at 0x%X\n", partition->name, partition->offset);
    kboot_erase_region(dev, partition->offset, image_size, progress, last_pct);
    kboot_write_memory(dev, partition->offset, image, image_size, progress, last_pct);
    
    if (partition->id == 1) kboot_write_u32(dev, REG_GPR18, 1);
    else if (partition->id == 2) kboot_write_u32(dev, REG_GPR18, 2);
    
    printf("Resetting device\n");
    kboot_send_command(dev, CMD_RESET, 0, NULL, 0);
    kboot_wait_result(dev, 2, NULL);
    printf("Device reset complete\n");
    return 0;
}
