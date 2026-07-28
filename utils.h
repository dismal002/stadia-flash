#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void progress_printer(const char* label, float pct, int* last_pct);

uint8_t* load_asset(const char* assets_dir, const char* name, size_t* out_size);

char* find_hid_path(uint16_t vendor_id, uint16_t product_id);
char* wait_for_hid(uint16_t vendor_id, uint16_t product_id, int timeout_s);

struct hid_device_;
int get_hid_report_sizes(struct hid_device_* handle, int* sizes_out, int max_reports);

#endif // UTILS_H
