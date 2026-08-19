import serial
import csv
import time
import sys

PORT = "COM4"
BAUD = 921600
OUTPUT_FILE = f"data/imu_log_{time.strftime('%Y%m%d_%H%M%S')}.csv"

HEADER = ["micros", "sampleCount", "ax_raw", "ay_raw", "az_raw",
          "temp_raw", "gx_raw", "gy_raw", "gz_raw", "host_time"]

def main():
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"Failed to open {PORT}: {e}")
        sys.exit(1)

    print(f"Connected to {PORT}. Logging to {OUTPUT_FILE}")
    print("Press Ctrl+C to stop.\n")

    with open(OUTPUT_FILE, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(HEADER)

        try:
            while True:
                raw_line = ser.readline()
                if not raw_line:
                    continue

                try:
                    decoded = raw_line.decode("utf-8", errors="strict").strip()
                except UnicodeDecodeError:
                    continue

                if not decoded:
                    continue

                fields = decoded.split(",")

                host_time = time.time()
                writer.writerow(fields + [f"{host_time:.6f}"])

        except KeyboardInterrupt:
            print("\nStopped by user.")
        finally:
            ser.close()
            print(f"Saved to {OUTPUT_FILE}")

if __name__ == "__main__":
    main()