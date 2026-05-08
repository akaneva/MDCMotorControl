# Fixing FTDI VCP/JTAG for Olimex ARM-USB-OCD-H (macOS & Windows)
This document outlines the issue of the Olimex ARM-USB-OCD-H device not being recognized as a Virtual COM Port (VCP) and provides step-by-step methods to fix it by reprogramming its EEPROM.

## 1. Problem Description
When using specialized JTAG debuggers (like Olimex) based on the FTDI FT2232H dual-channel chip, the device is often recognized only as a JTAG interface. It fails to create a virtual serial port (COM/VCP) in the operating system. This prevents the simultaneous use of the hardware debugger and a serial console to read UART data from the microcontroller.

## 2. Root Cause
The issue stems from the differing identification numbers (VID and PID) programmed into the chip:

Standard FTDI FT2232H: VID: 0x0403, PID: 0x6010

Olimex ARM-USB-OCD-H: VID: 0x15BA, PID: 0x002B

Operating system drivers (especially the strict macOS Dext drivers introduced in newer versions) are programmed to activate VCP functionality only for the standard FTDI PID. Since Olimex uses a custom PID, the system ignores the chip's second channel (UART) completely.

## 3. Theoretical Solution
The solution is to reprogram the EEPROM memory of the FTDI chip so that it presents itself to the computer using the standard FTDI identifiers (0x0403:0x6010). This forces the OS to load the standard dual-port serial driver, exposing both channels to the system.

## 4. Implementation on Windows
Windows offers the easiest graphical method for this modification.

- Download and install FT_PROG from the official FTDI website.

- Plug in your device and click the magnifying glass icon (Scan and Parse).

- In the device tree, navigate to USB Device Descriptor -> Custom VID/PID.

- Change the values to:

        Vendor ID: 0403

        Product ID: 6010

- Click the lightning bolt icon (Program) to flash the new settings to the device.

- Unplug and re-plug the device.

## 5. Implementation on macOS (Terminal)
Since FT_PROG is not available for Mac, we can achieve the same result using the pyftdi Python library.

- Preparation
    First, install the required dependencies using Terminal:

    ```
    # Install the low-level USB library via Homebrew
    brew install libusb

    # Install the Python FTDI library
    pip3 install pyftdi
    ```

- Reprogramming

    Verify the device connection:

    ```
    python3 -c "from pyftdi.ftdi import Ftdi; Ftdi.add_custom_vendor(0x15ba, 'Olimex'); Ftdi.add_custom_product(0x15ba, 0x002b); Ftdi.show_devices()"

    ```
- Flash the new VID/PID:
    (Note: Adjust the path to ftconf.py depending on your specific Python installation folder)

    ```
    python3 -c "from pyftdi.ftdi import Ftdi; Ftdi.add_custom_vendor(0x15ba, 'Olimex'); Ftdi.add_custom_product(0x15ba, 0x002b); import sys; sys.argv=['ftconf.py', 'ftdi://0x15ba:0x002b/1', '--vid', '0x0403', '--pid', '0x6010', '-S', '128', '-u']; exec(open('/Users/YOUR_USERNAME/Library/Python/3.9/bin/ftconf.py').read())"

    ```


## 6. Result and Usage
After reconnecting the hardware, two virtual ports will appear in your system (e.g., /dev/cu.usbserial-... on macOS):

Channel A (Port 1): Physically wired to JTAG.

Channel B (Port 2): Physically wired to UART (This is your Serial Monitor / COM port).


## 7. PlatformIO Configuration (platformio.ini)

EXAMPLE : 

```
[env:olimex_ocd_h_VirtualCOM]
platform = ststm32
board = olimex_e407
framework = stm32cube


upload_protocol = custom
debug_tool = custom

;Custom OpenOCD setup for Olimex OCD-H (Debug)
debug_server =
  ${platformio.packages_dir}/tool-openocd/bin/openocd
  -f interface/ftdi/olimex-arm-usb-ocd-h.cfg
  -c "ftdi_vid_pid 0x0403 0x6010"       ; new  debuger  VID/PID  
  -f target/stm32f4x.cfg
  -c "adapter speed 2000"

; Custom OpenOCD setup for Olimex OCD-H (Upload)
upload_command = 
  ${platformio.packages_dir}/tool-openocd/bin/openocd 
  -f interface/ftdi/olimex-arm-usb-ocd-h.cfg 
  -c "ftdi_vid_pid 0x0403 0x6010" 
  -f target/stm32f4x.cfg -c "adapter speed 2000" 
  -c "program .pio/build/${PIOENV}/firmware.elf verify reset exit"
```

Important Notes
Warning: Modifying the EEPROM may make the device undetectable by proprietary Olimex software tools that strictly look for PID 0x002B.

Backup: It is highly recommended to create a backup of your original EEPROM before making any changes (using ftconf.py ... -o backup.ini).