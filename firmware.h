#ifndef FIRMWARE_H
#define FIRMWARE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define IVT_MAGIC_LEN 4
extern const uint8_t IVT_MAGIC[IVT_MAGIC_LEN];

#define BUILD_INFO_HEADER_MAGIC 1953699234
#define BUILD_INFO_FOOTER_MAGIC 1200016776
#define BUILD_INFO_SIZE 256

typedef struct {
    const char* name;
    uint32_t offset;
    uint32_t size;
    int id;
} partition_t;

typedef struct {
    bool has_ivt;
    uint32_t ivt_offset;
    uint32_t fcb_offset;
    uint32_t version;
    uint32_t build_number;
    uint32_t reset_handler;
    const partition_t* partition;
} build_info_t;

int parse_build_info(const uint8_t* data, size_t length, build_info_t* out_info);

#endif // FIRMWARE_H
