#include "firmware.h"
#include <string.h>

const uint8_t IVT_MAGIC[IVT_MAGIC_LEN] = {0xD1, 0x00, 0x20, 0x41};

static const partition_t _PARTITIONS[] = {
    {"Application A", 1610874880, 8126464, 1},
    {"Application B", 1619263488, 8126464, 2},
    {"Bootloader A",  1619001344, 131072,  3},
    {"Bootloader B",  1619132416, 131072,  4},
};
static const int _NUM_PARTITIONS = sizeof(_PARTITIONS) / sizeof(_PARTITIONS[0]);

static uint32_t read_u32_le(const uint8_t* buf) {
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static const partition_t* find_partition(uint32_t reset_handler_addr) {
    for (int i = 0; i < _NUM_PARTITIONS; i++) {
        uint32_t lo = _PARTITIONS[i].offset;
        uint32_t hi = _PARTITIONS[i].offset + _PARTITIONS[i].size;
        if (reset_handler_addr >= lo && reset_handler_addr <= hi) {
            return &_PARTITIONS[i];
        }
    }
    return NULL;
}

int parse_build_info(const uint8_t* data, size_t length, build_info_t* out_info) {
    if (length < 4) return -1; // File too small
    
    bool has_ivt = (memcmp(data, IVT_MAGIC, IVT_MAGIC_LEN) == 0);
    uint32_t build_info_offset = 1024 + (has_ivt ? 4096 : 0);
    
    if (length < build_info_offset + BUILD_INFO_SIZE) return -2; // Truncated
    
    const uint8_t* build = data + build_info_offset;
    uint32_t header = read_u32_le(build + 0);
    uint32_t size = read_u32_le(build + 8);
    uint32_t version = read_u32_le(build + 16);
    uint32_t build_number = read_u32_le(build + 20);
    uint32_t footer = read_u32_le(build + 252);
    
    if (header != BUILD_INFO_HEADER_MAGIC) return -3;
    if (footer != BUILD_INFO_FOOTER_MAGIC) return -4;
    if (size != BUILD_INFO_SIZE) return -5;
    
    uint32_t reset_handler_offset = 4 + (has_ivt ? 4096 : 0);
    if (length < reset_handler_offset + 4) return -6;
    
    uint32_t reset_handler = read_u32_le(data + reset_handler_offset);
    const partition_t* part = find_partition(reset_handler);
    if (!part) return -7;
    
    out_info->has_ivt = has_ivt;
    out_info->ivt_offset = 0;
    out_info->fcb_offset = 4096;
    out_info->version = version;
    out_info->build_number = build_number;
    out_info->reset_handler = reset_handler;
    out_info->partition = part;
    
    return 0;
}
