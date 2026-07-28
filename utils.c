#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <hidapi/hidapi.h>

void progress_printer(const char* label, float pct, int* last_pct) {
    int pct_i = (int)pct;
    if (pct_i != *last_pct) {
        *last_pct = pct_i;
        printf("\r%s: %3d%%", label, pct_i);
        fflush(stdout);
        if (pct_i >= 100) {
            printf("\n");
        }
    }
}

uint8_t* load_asset(const char* dir, const char* name, size_t* out_size) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    if (out_size) *out_size = size;
    return data;
}

char* find_hid_path(uint16_t vendor_id, uint16_t product_id) {
    struct hid_device_info* devs = hid_enumerate(vendor_id, product_id);
    if (!devs) return NULL;
    char* path = strdup(devs->path);
    hid_free_enumeration(devs);
    return path;
}

static double monotonic_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

char* wait_for_hid(uint16_t vendor_id, uint16_t product_id, int timeout_s) {
    double deadline = monotonic_time() + timeout_s;
    while (monotonic_time() < deadline) {
        char* path = find_hid_path(vendor_id, product_id);
        if (path) return path;
        usleep(250000);
    }
    return NULL;
}

int get_hid_report_sizes(struct hid_device_* handle, int* sizes_out, int max_reports) {
    if (!handle || !sizes_out) return -1;
    for (int i = 0; i < max_reports; i++) sizes_out[i] = 0;
    unsigned char desc[4096];
    int res = hid_get_report_descriptor(handle, desc, sizeof(desc));
    if (res < 0) return res;

    int pos = 0;
    int current_report_id = 0;
    int report_size = 0;
    int report_count = 0;

    while (pos < res) {
        unsigned char b = desc[pos++];
        if (b == 0xFE) { // Long item, ignore
            if (pos >= res) break;
            int len = desc[pos++];
            if (pos >= res) break;
            pos += len + 1; // len + bLongItemTag
            continue;
        }
        int bSize = b & 3;
        if (bSize == 3) bSize = 4; // 3 means 4 bytes
        int bType = (b >> 2) & 3;
        int bTag = (b >> 4) & 15;

        if (pos + bSize > res) break;

        uint32_t data = 0;
        for (int i = 0; i < bSize; i++) {
            data |= ((uint32_t)desc[pos + i]) << (8 * i);
        }
        pos += bSize;

        if (bType == 1) { // Global
            if (bTag == 8) { // Report ID
                current_report_id = data;
            } else if (bTag == 3) { // Report Size
                report_size = data;
            } else if (bTag == 7) { // Report Count
                report_count = data;
            }
        } else if (bType == 0) { // Main
            if (bTag == 8 || bTag == 9 || bTag == 11) { // Input, Output, Feature
                if (current_report_id >= 0 && current_report_id < max_reports) {
                    int bytes = (report_size * report_count + 7) / 8;
                    if (bytes > sizes_out[current_report_id]) {
                        sizes_out[current_report_id] = bytes;
                    }
                }
            }
        }
    }
    return 0;
}
