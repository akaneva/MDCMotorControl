import serial
import struct
import sys
import time

# --- CONFIGURATION ---
# Replace with your actual port (e.g., '/dev/cu.usbserial-110')
COM_PORT = '/dev/cu.usbserial-A5069RR4'  
BAUD_RATE = 230400                 # 1 Mbps match with STM32
SYNC_BYTE = 0x55                     # Header byte to sync the stream
MAX_POS = 14399                      # Max encoder steps (3600 PPR * 4 - 1)

def calc_crc8(data: bytes) -> int:
    """
    Pure Python implementation of CRC-8 ATM.
    Matches the C++ firmware exactly (Polynomial: 0x07, Initial: 0x00).[cite: 1]
    """
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc

def main():
    try:
        # Open serial port with a short timeout to keep the loop responsive
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.1)
        print(f"Connected to {COM_PORT} at {BAUD_RATE} bps.")
        print("Listening for telemetry... (Press Ctrl+C to stop)")
    except Exception as e:
        print(f"Failed to connect: {e}")
        sys.exit(1)

    # Tracking variables
    last_pos = None
    packets_received = 0
    errors_crc = 0
    errors_skipped = 0
    
    # Timing variables for Real-Time Speed calculation
    start_time = time.time()
    last_update_time = start_time

    try:
        while True:
            # 1. SEARCH FOR SYNC BYTE
            # We read byte-by-byte to align with the packet start[cite: 1]
            raw_data = ser.read(1)
            
            # If no data (timeout), just continue
            if not raw_data:
                continue
                
            # If byte is not 0x55, it is likely line noise; ignore it
            if raw_data[0] != SYNC_BYTE:
                continue 

            # 2. READ PAYLOAD (2 bytes for Position + 1 byte for CRC)[cite: 1]
            payload = ser.read(3)
            if len(payload) < 3:
                # Packet interrupted or incomplete
                continue

            pos_bytes = payload[0:2]
            received_crc = payload[2]

            # 3. CRC VERIFICATION
            # Verify data integrity using the same CRC-8 ATM algorithm[cite: 1]
            if calc_crc8(pos_bytes) != received_crc:
                errors_crc += 1
                continue

            # 4. DECODE POSITION
            # '>H' = Big-Endian Unsigned Short (16-bit)[cite: 1]
            # This matches the __builtin_bswap16() used on STM32[cite: 1]
            current_pos = struct.unpack('>H', pos_bytes)[0]
            packets_received += 1

            # 5. SEQUENCE VALIDATION (Detect skipped steps)
            if last_pos is not None:
                diff = current_pos - last_pos
                # A valid move is exactly +/- 1 step or a rollover at 0/14399[cite: 1]
                is_rollover = abs(diff) == MAX_POS
                is_valid_step = abs(diff) == 1 or is_rollover
                
                # If the jump is larger than 1 and it's not a rollover, a packet was lost
                if not is_valid_step and current_pos != last_pos:
                    errors_skipped += 1

            last_pos = current_pos

            # 6. REAL-TIME STATUS UPDATE (Every 1000 packets)
            if packets_received % 1000 == 0:
                current_time = time.time()
                # Calculate delta time for the last 1000 packets only
                delta_time = current_time - last_update_time
                
                if delta_time > 0:
                    # Instantaneous speed (packets per second)
                    instant_pps = 1000 / delta_time
                else:
                    instant_pps = 0
                
                # Convert raw encoder steps to degrees[cite: 1]
                degrees = current_pos * 0.025 # (360 / 14400)[cite: 1]
                
                print(f"Pos: {current_pos:5d} ({degrees:6.2f}°) | "
                      f"Real Speed: {instant_pps:5.0f} pkts/s | "
                      f"Skips: {errors_skipped} | "
                      f"CRC Errors: {errors_crc}")
                
                # Reset timer for the next interval
                last_update_time = current_time

    except KeyboardInterrupt:
        print("\nTelemetry stopped by user.")
    finally:
        ser.close()
        # Final session summary
        total_time = time.time() - start_time
        avg_speed = packets_received / total_time if total_time > 0 else 0
        print("-" * 50)
        print(f"Final Summary:")
        print(f"Total Packets: {packets_received}")
        print(f"Average Speed: {avg_speed:.0f} pkts/s")
        print(f"Total Skips:   {errors_skipped}")
        print(f"CRC Errors:    {errors_crc}")

if __name__ == "__main__":
    main()