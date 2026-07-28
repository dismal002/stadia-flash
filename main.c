#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <hidapi/hidapi.h>
#include <libusb-1.0/libusb.h>

#include "utils.h"
#include "controller.h"
#include "sdp.h"
#include "kboot.h"
#include "firmware.h"

int cmd_info() {
    libusb_context* ctx;
    libusb_init(&ctx);
    libusb_device_handle* dev = find_controller(ctx);
    if (!dev) {
        printf("No Stadia controller found in normal USB mode.\n");
        libusb_exit(ctx);
        return 1;
    }
    claim_controller(dev);
    uint32_t version = 0;
    get_version(dev, &version);
    uint16_t battery = 0;
    get_battery(dev, &battery);
    char serial[256] = {0};
    get_serial_number(dev, serial, sizeof(serial));
    printf("Firmware version: %u\nBattery: %u%%\nSerial number: %s\n", version, battery, serial);
    const char* variant = classify_variant(serial);
    if (variant) {
        printf("Hardware variant: %s\n", variant);
    } else {
        printf("Hardware variant: BLOCKED\nThe original tool refuses to flash units with this serial prefix.\n");
    }
    libusb_close(dev);
    libusb_exit(ctx);
    return 0;
}

int cmd_list() {
    hid_init();
    char* sdp_path = find_hid_path(SDP_VENDOR_ID, SDP_PRODUCT_ID);
    printf("Flashloader/SDP mode (0x1FC9:0x0135): %s\n", sdp_path ? "present" : "not found");
    free(sdp_path);
    char* kboot_path = find_hid_path(KBOOT_VENDOR_ID, KBOOT_PRODUCT_ID);
    printf("Kboot/firmware-update mode (0x15A2:0x0073): %s\n", kboot_path ? "present" : "not found");
    free(kboot_path);
    hid_exit();
    
    libusb_context* ctx;
    libusb_init(&ctx);
    libusb_device_handle* dev = find_controller(ctx);
    printf("Normal mode (0x18D1:0x9400/0x946B): %s\n", dev ? "present" : "not found");
    if (dev) libusb_close(dev);
    libusb_exit(ctx);
    return 0;
}

int cmd_flash_loader(const char* assets_dir) {
    hid_init();
    char* path = find_hid_path(SDP_VENDOR_ID, SDP_PRODUCT_ID);
    if (!path) {
        printf("No device found in flashloader/SDP mode.\n");
        return 1;
    }
    size_t img_size = 0;
    uint8_t* image = load_asset(assets_dir, "restricted_ivt_flashloader.bin", &img_size);
    if (!image) {
        printf("Missing restricted_ivt_flashloader.bin\n");
        return 1;
    }
    sdp_device_t* dev = sdp_open(path, 5000);
    int last_pct = -1;
    printf("Uploading flashloader (%zu bytes)...\n", img_size);
    int status = sdp_upload_and_run(dev, image, img_size, progress_printer, &last_pct);
    printf("SDP load result: 0x%x\n", status);
    sdp_close(dev);
    free(image);
    free(path);
    hid_exit();
    return 0;
}

int cmd_flash_firmware(const char* assets_dir, const char* fw_path, int yes) {
    hid_init();
    char* path = find_hid_path(KBOOT_VENDOR_ID, KBOOT_PRODUCT_ID);
    if (!path) {
        printf("No device found in Kboot/update mode.\n");
        return 1;
    }
    
    FILE* f = fopen(fw_path, "rb");
    if (!f) { printf("Cannot open firmware %s\n", fw_path); return 1; }
    fseek(f, 0, SEEK_END); size_t fw_size = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* fw_image = malloc(fw_size);
    fread(fw_image, 1, fw_size, f);
    fclose(f);
    
    build_info_t binfo;
    if (parse_build_info(fw_image, fw_size, &binfo) < 0) {
        printf("Firmware parsing failed\n");
        return 1;
    }
    printf("Image targets %s (build %u)\n", binfo.partition->name, binfo.build_number);
    
    if (!yes) {
        printf("This will erase and reflash %s on the connected controller. Continue? [y/N] ", binfo.partition->name);
        char ans[10];
        if (fgets(ans, sizeof(ans), stdin) == NULL || (ans[0] != 'y' && ans[0] != 'Y')) {
            printf("Aborted.\n");
            return 1;
        }
    }
    
    size_t vp_size = 0, wb_size = 0;
    uint8_t* vp = load_asset(assets_dir, "flashloader_fcb_get_vendor_id.bin", &vp_size);
    uint8_t* wb = load_asset(assets_dir, "flashloader_fcb_w25q128jw.bin", &wb_size);
    
    kboot_device_t* dev = kboot_open(path, 5000);
    int last_pct = -1;
    kboot_flash_firmware(dev, fw_image, fw_size, &binfo, vp, vp_size, wb, wb_size, progress_printer, &last_pct);
    
    kboot_close(dev);
    free(vp); free(wb); free(fw_image); free(path);
    hid_exit();
    return 0;
}

int cmd_auto(const char* assets_dir, const char* fw_path, int yes) {
    hid_init();
    char* path = find_hid_path(KBOOT_VENDOR_ID, KBOOT_PRODUCT_ID);
    if (path) {
        printf("Device already in Kboot/update mode.\n");
        free(path);
        return cmd_flash_firmware(assets_dir, fw_path, yes);
    }
    path = find_hid_path(SDP_VENDOR_ID, SDP_PRODUCT_ID);
    if (path) {
        free(path);
        printf("Device in flashloader/SDP mode; uploading flashloader first...\n");
        cmd_flash_loader(assets_dir);
        printf("Waiting for device to re-enumerate in Kboot mode...\n");
        char* kb_path = wait_for_hid(KBOOT_VENDOR_ID, KBOOT_PRODUCT_ID, 20);
        if (!kb_path) {
            printf("Timed out waiting for Kboot device.\n");
            return 1;
        }
        free(kb_path);
        return cmd_flash_firmware(assets_dir, fw_path, yes);
    }
    printf("No device found in flashloader or Kboot mode.\n");
    return 1;
}

void print_help() {
    printf("Stadia Controller Firmware Updater\n\n");
    printf("Usage: stadia-flash <command> [options]\n\n");
    printf("Commands:\n");
    printf("  info             Read firmware version + battery from normal-mode controller\n");
    printf("  list             List candidate devices by mode\n");
    printf("  flash-loader     Upload + start the flashloader\n");
    printf("  flash-firmware   Flash a firmware image (device must already be in Kboot mode)\n");
    printf("  auto             Detect device mode and flash automatically\n\n");
    printf("Options:\n");
    printf("  -h, --help       Show this help message and exit\n");
    printf("  --assets-dir     Path to directory containing flashloader/probe assets (default: ./data)\n");
    printf("  -y, --yes        Don't prompt for confirmation (for flash-firmware/auto)\n");
}

int main(int argc, char** argv) {
    const char* assets_dir = "./data";
    int yes = 0;
    
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    // Simple argument parsing for global options
    const char* cmd = NULL;
    const char* fw_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "--assets-dir") == 0 && i + 1 < argc) {
            assets_dir = argv[++i];
        } else if (strcmp(argv[i], "-y") == 0 || strcmp(argv[i], "--yes") == 0) {
            yes = 1;
        } else if (!cmd) {
            cmd = argv[i];
        } else if (!fw_path) {
            fw_path = argv[i];
        }
    }
    
    if (!cmd) {
        print_help();
        return 1;
    }
    
    if (strcmp(cmd, "info") == 0) return cmd_info();
    if (strcmp(cmd, "list") == 0) return cmd_list();
    if (strcmp(cmd, "flash-loader") == 0) return cmd_flash_loader(assets_dir);
    if (strcmp(cmd, "flash-firmware") == 0) {
        if (!fw_path) { printf("Missing firmware path\n"); return 1; }
        return cmd_flash_firmware(assets_dir, fw_path, yes);
    }
    if (strcmp(cmd, "auto") == 0) {
        if (!fw_path) { printf("Missing firmware path\n"); return 1; }
        return cmd_auto(assets_dir, fw_path, yes);
    }
    
    printf("Unknown command: %s\n", cmd);
    return 1;
}
