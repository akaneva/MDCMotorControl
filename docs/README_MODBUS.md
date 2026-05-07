# Modbus TCP Protocol Documentation for MDC Motor Control

This documentation describes the Modbus TCP communication interface of the system. It is intended for software developers building HMI, GUI, or external control scripts to interface with the motor controller.

## 1. Connection Parameters
* **Protocol:** Modbus TCP
* **Default Port:** `502`
* **Maximum Registers per Read:** `20` (Requests exceeding 20 registers will be ignored to prevent buffer overflow).

## 2. Supported Modbus Function Codes
The system supports the following Modbus Function Codes (FC) for accessing Holding Registers (4xxxx):
* **`FC 03 (0x03)`** - Read Holding Registers (Read one or multiple registers)
* **`FC 06 (0x06)`** - Write Single Register (Write to a single register)

> **Note:** All registers are 16-bit unsigned integers (`uint16_t`). Data is transmitted in **Big-Endian** byte order, which is the standard for Modbus.

---

## 3. Register Map

### 3.1. Telemetry (Read-Only - FC 03)
This block provides real-time information regarding the hardware state.

| Register| Name              | Access | Scale | Description                                                                                             |
| :---    | :---              | :---:  | :---: | :---                                                                                                    |
| **100** | `REG_ENCODER_POS` | R      | x1    | Current absolute encoder position (in pulses). Range: `0 - 14399`.                                      |
| **101** | `REG_MOTOR_STATE` | R      | x1    | Current motor state: <br>`0` = Stopped <br>`1` = Accelerating <br>`2` = Cruising <br>`3` = Decelerating |
| **102** | `REG_CYCLE_TIME`  | R      | x1    | Time of the last full rotation in milliseconds (ms). Max value: `65535`.                                |

### 3.2. Motor Control (Read / Write - FC 03, FC 06)
This block controls the physical movement and operational parameters of the stepper motor.

| Register | Name                  | Access | Scale  | Description                                                                                  |
| :---    | :---                  | :---:  | :---:   | :---                                                                                         |
| **200** | `REG_MOTOR_CTRL`      | R/W    | x1      | Motor Start/Stop command: <br>`0` = Stop <br>`1` = Start                                     |
| **201** | `REG_MOTOR_DIR`       | R/W    | x1      | Motor direction of rotation: <br>`0` = Forward <br>`1` = Reverse                             |
| **202** | `REG_MOTOR_RPM`       | R/W    | **x10** | Target speed in RPM (Revolutions Per Minute). <br>*Example:* Writing `155` sets `15.5 RPM`.  |
| **203** | `REG_MOTOR_ACCEL`     | R/W    | **x10** | Acceleration time in seconds. <br>*Example:* Writing `50` sets `5.0 sec`.                    |
| **204** | `REG_MOTOR_MICROSTEPS`| R/W    | x1      | Driver microstepping resolution. Range: `0 - 65535`.                                         |


### 3.3. Telemetry & Encoder Settings (Read / Write - FC 03, FC 06)
This block manages the high-speed UART telemetry stream and encoder logic.

| Register | Name              | Access | Scale     | Description                                                                       |
| :---     | :---              | :---:  | :---:     | :---                                                                              |
| **300**  | `REG_STREAM_CTRL` | R/W    | x1        | Fast UART telemetry control: <br>`0` = Stop Stream <br>`1` = Start Stream         |
| **301**  | `REG_ENCODER_DIR` | R/W    | x1        | Encoder logical direction: <br>`0` = Normal <br>`1` = Reversed                    |
| **302**  | `REG_UART_BAUD`   | R/W    | **/100**  | UART telemetry baud rate. <br>*Example:* Writing `1152` sets `115200 bps`, `10000` sets `1000000 bps`. |
| **303**  | `REG_AUTO_STREAM` | R/W    | x1        | Auto-start UART stream on boot: <br>`0` = Disabled <br>`1` = Enabled             |


### 3.4. Network Settings (Read / Write - FC 03, FC 06)
Network addresses are split across 16-bit registers (2 bytes per register).
*Example for IP Address 192.168.1.151:*
`HIGH` register contains 192.168 (Hex: `0xC0A8`, Dec: `49320`).
`LOW` register contains 1.151 (Hex: `0x0197`, Dec: `407`).

| Register | Name | Access | Scale | Description |
| :--- | :--- | :---: | :---: | :--- |
| **310** | `REG_IP_HIGH` | R/W | - | IP Address - High Bytes (Bytes 0, 1) |
| **311** | `REG_IP_LOW` | R/W | - | IP Address - Low Bytes (Bytes 2, 3) |
| **312** | `REG_NETMASK_HIGH` | R/W | - | Subnet Mask - High Bytes |
| **313** | `REG_NETMASK_LOW` | R/W | - | Subnet Mask - Low Bytes |
| **314** | `REG_GATEWAY_HIGH` | R/W | - | Default Gateway - High Bytes |
| **315** | `REG_GATEWAY_LOW` | R/W | - | Default Gateway - Low Bytes |

### 3.5. System Commands (Write-Only - FC 06)
These registers are used to trigger system-wide functions.

| Register | Name | Access | Description |
| :--- | :--- | :---: | :--- |
| **400** | `REG_SAVE_SETTINGS` | W | Save all current configuration parameters to non-volatile memory (Flash). Write `1` to trigger. |
| **401** | `REG_SYSTEM_REBOOT` | W | Hardware reboot the system. Write `1` to trigger. (System will delay reboot by 1000ms to allow TCP response to be sent). |

---

## 4. Integration Examples

### 4.1. Reading Motor RPM
To read the currently set target speed (Register `202`):
1. Send an `FC 03` request to register `202` with length `1`.
2. If the response payload is `155`, divide by `10` to get `15.5 RPM`.

### 4.2. Changing the IP Address
To change the device IP address to `192.168.1.151`:
1. `192.168` translates to `0xC0A8` (Decimal `49320`). Send `FC 06` to register `310` with value `49320`.
2. `1.151` translates to `0x0197` (Decimal `407`). Send `FC 06` to register `311` with value `407`.
3. To make the setting persistent across reboots, send `FC 06` to register `400` with value `1`.
4. To apply the new IP address, reboot the device (Send `FC 06` to `401` with value `1`).