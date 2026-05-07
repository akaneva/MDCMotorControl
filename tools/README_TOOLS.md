# Setup Guide: Running the Modbus GUI  and  the COM Test on Windows

This guide provides step-by-step instructions to set up the environment and run the `modbus_gui_flash.py` program on a Windows operating system.

---

## 1. Install Python
The application requires Python 3.x to run.
1. Visit the official [python.org](https://www.python.org/downloads/) website.
2. Download the latest stable installer for Windows (e.g., Python 3.11 or 3.12).
3. **IMPORTANT:** During installation, ensure you check the box **"Add Python to PATH"**. This allows you to run Python from the Command Prompt.
4. Click **Install Now**.

## 2. Verify Installation
To confirm Python is correctly installed:
1. Open the **Command Prompt** (press `Win + R`, type `cmd`, and hit Enter).
2. Type the following command and press Enter:
   ```
   python --version
   ```
 
## 3. Prepare the Python Environment
Before installing the project dependencies, it is highly recommended to ensure that your local package manager is up to date. Open your terminal and run:
1. Upgrade the package manager: 
    ```
    python -m pip install --upgrade pip
    ```

## 4. Install dependencies 
   ```
   pip install pymodbus matplotlib pyserial
   ```

## 5. How to Verify Installation
   ```
   pip list
   ```
## 6. Run the Application

### 6.1 Pre-run software and hardware configuration
1. Network Setup for Modbus GUI
- The STM32 board uses a static IP address (Default: `192.168.1.151`). Your PC must be on the same subnet to establish a Modbus TCP connection.
    - Connect the STM32 board and your PC via an Ethernet cable to the same physical network
    - Go to Windows Network Settings -> Ethernet -> Edit IPv4 properties.
    Set your PC to a **Static IP**:
    * **IP Address:** `192.168.1.10` (or any address except .151)
    * **Subnet Mask:** `255.255.255.0`

2. Serial Setup for UART Telemtry
    - The Python script needs to know which USB port the STM32 is connected to
    - Open **Device Manager** in Windows and look under "Ports (COM & LPT)" to find your board's port number (e.g., `COM3`, `COM5`).
    - Open `com_test.py` in any text editor.
    - Locate the configuration section at the top and change the port to match yours:
        ```python
            COM_PORT = 'COM3'  # Change this to your actual COM port
            BAUD_RATE = 230400    # Change this to match the STM32 baud rate
        ```

### 6.2 Run program

   ```     
    python modbus_gui_flash.py
    python com_test.py 

   ```



