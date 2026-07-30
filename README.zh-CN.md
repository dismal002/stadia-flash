# Stadia Flash Tool (C 语言移植版)

这是一个用 C 语言编写的用于刷写 Google Stadia 手柄固件的命令行工具。

## 功能特性

- **动态 HID 解析**：自动查询并解析 USB HID 报告描述符，完全按照设备预期配置数据包大小。
- **跨平台潜力**：依赖标准跨平台库 (`libusb` 和 `hidapi`)。

## 依赖项

要编译此工具，您需要 `make`、C 编译器（如 `gcc`）以及 `libusb-1.0` 和 `hidapi` 的开发头文件。

在 Debian/Ubuntu 系统上：
```bash
sudo apt install build-essential libusb-1.0-0-dev libhidapi-dev
```

## 编译说明

在项目根目录下运行 `make` 即可：

```bash
make
```

这将会生成名为 `stadia-flash` 的可执行文件。

## 使用方法

```text
Stadia Controller Firmware Updater

Usage: stadia-flash <command> [options]

Commands:
  info             从普通模式的手柄读取固件版本和电量
  list             列出当前已连接的候选设备及其模式
  flash-loader     上传并启动 flashloader
  flash-firmware   刷写固件映像 (设备必须已处于 Kboot 模式)
  auto             自动检测设备模式并执行刷写流程

Options:
  -h, --help       显示此帮助信息并退出
  --assets-dir     包含 flashloader 资源的目录路径 (默认: ./data)
  -y, --yes        跳过确认提示 (适用于 flash-firmware/auto)
```

