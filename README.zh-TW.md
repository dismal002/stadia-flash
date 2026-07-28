# Stadia Flash Tool (C 語言移植版)

這是一個用 C 語言編寫的用於刷寫 Google Stadia 手把韌體的命令列工具。它是 Python/JavaScript 工具的 C 語言原生移植版本，旨在提供原生執行效能與更好的程式碼可維護性。

## 功能特性

- **無需 Python 環境**：編譯為單個原生二進位檔案即可執行。
- **動態 HID 解析**：自動查詢並解析 USB HID 報告描述符，完全依照設備預期配置資料包大小。
- **跨平台潛力**：依賴標準跨平台函式庫 (`libusb` 和 `hidapi`)。

## 依賴項

要編譯此工具，您需要 `make`、C 編譯器（如 `gcc`）以及 `libusb-1.0` 和 `hidapi` 的開發標頭檔。

在 Debian/Ubuntu 系統上：
```bash
sudo apt install build-essential libusb-1.0-0-dev libhidapi-dev
```

## 編譯說明

在專案根目錄下執行 `make` 即可：

```bash
make
```

這將會產生名為 `stadia-flash` 的可執行檔案。

## 使用方法

```text
Stadia Controller Firmware Updater

Usage: stadia-flash <command> [options]

Commands:
  info             從一般模式的手把讀取韌體版本與電量
  list             列出目前已連接的候選設備及其模式
  flash-loader     上傳並啟動 flashloader
  flash-firmware   刷寫韌體映像 (設備必須已處於 Kboot 模式)
  auto             自動偵測設備模式並執行刷寫流程

Options:
  -h, --help       顯示此幫助資訊並退出
  --assets-dir     包含 flashloader 資源的目錄路徑 (預設: ./data)
  -y, --yes        跳過確認提示 (適用於 flash-firmware/auto)
```

## 資料資源

工具正常運作需要原版的 Stadia `.bin` 韌體檔案與 flashloader 二進位檔案。預設情況下，工具會期望這些檔案存放在執行目錄下的 `./data/` 資料夾中。
