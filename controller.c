#include "controller.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const uint16_t GOOGLE_PRODUCT_IDS[2] = {37888, 37995};

static int vendor_interface_number(libusb_device* dev) {
    struct libusb_config_descriptor* config;
    if (libusb_get_active_config_descriptor(dev, &config) != 0) return -1;
    for (int i = 0; i < config->bNumInterfaces; i++) {
        for (int j = 0; j < config->interface[i].num_altsetting; j++) {
            if (config->interface[i].altsetting[j].bInterfaceClass == 255) {
                int num = config->interface[i].altsetting[j].bInterfaceNumber;
                libusb_free_config_descriptor(config);
                return num;
            }
        }
    }
    libusb_free_config_descriptor(config);
    return -1;
}

libusb_device_handle* find_controller(libusb_context* ctx) {
    libusb_device** list;
    ssize_t cnt = libusb_get_device_list(ctx, &list);
    libusb_device_handle* found = NULL;

    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device* dev = list[i];
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(dev, &desc);
        if (desc.idVendor == GOOGLE_VENDOR_ID) {
            if (desc.idProduct == GOOGLE_PRODUCT_IDS[0] || desc.idProduct == GOOGLE_PRODUCT_IDS[1]) {
                if (libusb_open(dev, &found) == 0) {
                    break;
                }
            }
        }
    }
    libusb_free_device_list(list, 1);
    return found;
}

int claim_controller(libusb_device_handle* dev_handle) {
    libusb_device* dev = libusb_get_device(dev_handle);
    int intf = vendor_interface_number(dev);
    if (intf < 0) return -1;

    if (libusb_kernel_driver_active(dev_handle, intf) == 1) {
        libusb_detach_kernel_driver(dev_handle, intf);
    }
    if (libusb_claim_interface(dev_handle, intf) < 0) return -1;
    return intf;
}

int get_version(libusb_device_handle* dev_handle, uint32_t* out_version) {
    libusb_device* dev = libusb_get_device(dev_handle);
    int intf = vendor_interface_number(dev);
    uint8_t data[64];
    int res = libusb_control_transfer(dev_handle, 0xA1, 129, 0, intf, data, sizeof(data), 1000);
    if (res < 4) return -1;
    *out_version = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    return 0;
}

int get_battery(libusb_device_handle* dev_handle, uint16_t* out_battery) {
    libusb_device* dev = libusb_get_device(dev_handle);
    int intf = vendor_interface_number(dev);
    libusb_control_transfer(dev_handle, 0x21, 131, 0, intf, NULL, 0, 1000);
    usleep(500000);
    uint8_t data[64];
    int res = libusb_control_transfer(dev_handle, 0xA1, 132, 0, intf, data, sizeof(data), 1000);
    if (res < 2) return -1;
    *out_battery = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    return 0;
}

int get_serial_number(libusb_device_handle* dev_handle, char* serial, int max_len) {
    struct libusb_device_descriptor desc;
    libusb_device* dev = libusb_get_device(dev_handle);
    libusb_get_device_descriptor(dev, &desc);
    if (desc.iSerialNumber == 0) return -1;
    int res = libusb_get_string_descriptor_ascii(dev_handle, desc.iSerialNumber, (unsigned char*)serial, max_len);
    return res > 0 ? 0 : -1;
}

const char* classify_variant(const char* serial) {
    if (strncmp(serial, "91", 2) == 0 || strncmp(serial, "92", 2) == 0 ||
        strncmp(serial, "93", 2) == 0 || strncmp(serial, "94", 2) == 0) {
        return NULL; // Blocked
    }
    if (strncmp(serial, "95", 2) == 0 || strncmp(serial, "96", 2) == 0 ||
        strncmp(serial, "97", 2) == 0) {
        return "dvt";
    }
    if (strncmp(serial, "98", 2) == 0 && strncmp(serial, "9809", 4) < 0) {
        return "dvt";
    }
    return "pvt";
}
