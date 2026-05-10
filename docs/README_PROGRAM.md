# Technical Documentation for STM32 Programming and Diagnostics

This document provides instructions for configuring the workspace, installing drivers, and flashing firmware onto STM32 microcontrollers using the **Olimex ARM-USB-OCD-H** programmer in a Windows environment.

---

## 1. Hardware Configuration

Before starting, ensure the correct physical connection of the components:

### 1.1. Programming Interface (JTAG)
* **Connection:** Connect the 20-pin ribbon cable to the JTAG connector on the target board.
* **Power Supply:** The target board must be powered independently. The programmer does not supply power to the target.

### 1.2. Diagnostic Data Interface (UART)
For telemetry monitoring, use the programmer's 10-pin connector (RS232/UART). Connect it according to the following schematic:
* `GND` (Olimex) ↔ `GND` (STM32)
* `TXD` (Olimex) ↔ `RXD` (STM32)
* `RXD` (Olimex) ↔ `TXD` (STM32)

---

## 2. System Requirements and Software

The following software is required:

1. **OpenOCD (xPack version):** It is recommended to extract it to `C:\openocd` and add `C:\openocd\bin` to the Windows `PATH` system environment variable.
2. **Zadig:** A tool for managing USB drivers.
3. **FTDI VCP Drivers:** Official drivers for the Virtual COM Port.

---

## 3. Driver Configuration

The device functions as a composite USB device with two interfaces (Interface 0 and Interface 1).

### 3.1. Data Channel (Interface 1)
Used for serial communication (COM port).
1. Install the official FTDI VCP driver (`setup.exe`).
2. In *Device Manager*, the device should be identified under the `Ports (COM & LPT)` section as `USB Serial Port (COMx)`.

### 3.2. Programming Channel (Interface 0)
Used by OpenOCD for JTAG communication.
1. Launch **Zadig**.
2. From the `Options` menu, select `List All Devices`.
3. Select `Olimex OpenOCD JTAG ARM-USB-OCD-H (Interface 0)`.
4. Change the driver to **WinUSB** by clicking the `Replace Driver` button.

---

## 4. Firmware Flashing Instructions

Operations are performed via the Command Prompt within the directory containing the compiled project files.

### For .elf files (Automatic addressing)
```cmd
openocd -f interface/ftdi/olimex-arm-usb-ocd-h.cfg -f target/stm32f4x.cfg -c "program firmware.elf verify reset exit"