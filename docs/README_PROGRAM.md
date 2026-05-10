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

### 3.3. OpenOCD Interface Troubleshooting (VID/PID Override)
*Important:* If OpenOCD fails to initialize with an `unable to open ftdi device with description 'Olimex...'` error, the programmer's internal EEPROM might be missing the custom Olimex string and is instead presenting standard FTDI factory identifiers.

**How to check your device's VID and PID:**
1. Open Windows **Device Manager**.
2. Locate the device (usually under *Universal Serial Bus devices* or *Ports (COM & LPT)*).
3. Right-click the device and select **Properties**.
4. Navigate to the **Details** tab.
5. From the *Property* drop-down menu, select **Hardware Ids**.
6. Check the displayed values:
   * Normal Olimex IDs: `VID_15BA&PID_002B`
   * Generic FTDI IDs: `VID_0403&PID_6010`

**Applying the Fix:**
If your device shows the generic IDs (`VID_0403` and `PID_6010`), you must override the OpenOCD configuration:
1. Open the interface configuration file in a text editor: 
   `C:\openocd\scripts\interface\ftdi\olimex-arm-usb-ocd-h.cfg` *(adjust path as needed)*.
2. Change the target VID and PID to the generic FTDI values:
   Change `ftdi vid_pid 0x15ba 0x002b` to `ftdi vid_pid 0x0403 0x6010`.
3. Comment out the specific device description check by adding a `#` prefix:
   `# ftdi device_desc "Olimex OpenOCD JTAG ARM-USB-OCD-H"`
4. Save the file and restart the OpenOCD command.

---

## 4. Firmware Flashing Instructions

Operations are performed via the Command Prompt within the directory containing the compiled project files.

### For .elf files (Automatic addressing)
```cmd
openocd -f interface/ftdi/olimex-arm-usb-ocd-h.cfg -f target/stm32f4x.cfg -c "program firmware.elf verify reset exit"

### For .bin files (Manual addressing required)
Since binary files lack memory address information, the flash base address must be explicitly provided (typically `0x08000000` for STM32).
```cmd
openocd -f interface/ftdi/olimex-arm-usb-ocd-h.cfg -f target/stm32f4x.cfg -c "program firmware.bin 0x08000000 verify reset exit"