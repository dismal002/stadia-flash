# Stadia Flash Tool (C Port)

A command-line tool for flashing firmware on Google Stadia controllers

## Features

- **Dynamic HID Parsing**: Automatically queries and parses USB HID report descriptors to configure payload sizes exactly as the device expects.
- **Cross-Platform Potential**: Relies on standard libraries (`libusb` and `hidapi`).

## Dependencies

To compile this tool, you need `make`, a C compiler (like `clang`), and the development headers for `libusb-1.0` and `hidapi`.

On Debian/Ubuntu:
```bash
sudo apt install build-essential libusb-1.0-0-dev libhidapi-dev
```

## Build Instructions

Simply run `make` in the root directory:

```bash
make
```

This will produce the `stadia-flash` executable.

## Usage

```text
Stadia Controller Firmware Updater

Usage: stadia-flash <command> [options]

Commands:
  info             Read firmware version + battery from normal-mode controller
  list             List candidate devices by mode
  flash-loader     Upload + start the flashloader
  flash-firmware   Flash a firmware image (device must already be in Kboot mode)
  auto             Detect device mode and flash automatically

Options:
  -h, --help       Show this help message and exit
  --assets-dir     Path to directory containing flashloader/probe assets (default: ./data)
  -y, --yes        Don't prompt for confirmation (for flash-firmware/auto)
```
