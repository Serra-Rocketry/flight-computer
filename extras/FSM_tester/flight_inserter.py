#!/usr/bin/env python3
"""
flight_inserter.py - reads CSV and sends over serial to Arduino/ESP32,
and displays real-time feedback from the flight state machine.

python flight_inserter.py --port /dev/ttyUSB0 --baud 115200 --file dados_filtrados.csv --delay 0.01

"""

import argparse
import csv
import sys
import time
import logging
import serial
import threading

# Try to import list_ports; if not available, disable the feature
try:
    import serial.tools.list_ports
    HAS_LIST_PORTS = True
except ImportError:
    HAS_LIST_PORTS = False

def parse_arguments():
    parser = argparse.ArgumentParser(description="Send flight data from CSV to Arduino over serial.")
    parser.add_argument('--port', required=True, help="Serial port (e.g., /dev/ttyUSB0, COM3)")
    parser.add_argument('--baud', type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument('--file', required=True, help="Path to CSV file (columns: millis,altp,ax,ay,az)")
    parser.add_argument('--delay', type=float, default=0.0,
                        help="Delay in seconds between rows (default: 0.0, send as fast as possible)")
    parser.add_argument('--verbose', action='store_true', help="Enable verbose logging")
    return parser.parse_args()

def setup_logging(verbose):
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(level=level, format='%(asctime)s - %(levelname)s - %(message)s')

def open_serial(port, baud):
    try:
        ser = serial.Serial(port, baud, timeout=1)
        logging.info(f"Opened {port} at {baud} baud")
        return ser
    except serial.SerialException as e:
        logging.error(f"Failed to open serial port {port}: {e}")
        sys.exit(1)

def read_csv(file_path):
    """Read CSV file and return list of rows as dictionaries."""

    try:
        data = []
        with open(file_path, 'r') as f:
            # Check if there's a header by reading first line
            first_line = f.readline().strip()
            f.seek(0)  # Go back to beginning
            
            # If first line contains column names, use DictReader
            if 'millis' in first_line.lower() or 'altp' in first_line.lower():
                reader = csv.DictReader(f)
                for row in reader:
                    data.append(row)
                logging.debug(f"Detected header row: {reader.fieldnames}")
            else:
                # No header, use column names
                reader = csv.reader(f)
                columns = ['millis', 'altp', 'ax', 'ay', 'az']
                for i, row in enumerate(reader):
                    if len(row) >= 5:  # Ensure we have enough columns
                        
                        data.append(dict(zip(columns, row[:5])))
                    else:
                        logging.warning(f"Row {i} has {len(row)} columns, expected at least 5. Skipping.")
        
        logging.info(f"Loaded {len(data)} rows from {file_path}")
        return data
    except FileNotFoundError:
        logging.error(f"File not found: {file_path}")
        sys.exit(1)
    except Exception as e:
        logging.error(f"Error reading CSV: {e}")
        sys.exit(1)

def serial_reader(ser, stop_event):
    """
    Thread function to continuously read and display feedback from ESP32.
    """
    logging.info("📡 Serial reader thread started - listening for ESP32 feedback...")
    print("\n" + "="*60)
    print("🛰️  ESP32 Flight State Machine Feedback")
    print("="*60 + "\n")
    
    while not stop_event.is_set():
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    # Print with a prefix to distinguish from our logs
                    print(f"📨 {line}")
        except serial.SerialException as e:
            logging.error(f"Serial read error: {e}")
            break
        except Exception as e:
            logging.error(f"Unexpected error in reader thread: {e}")
            break
        
        time.sleep(0.01)  # Small delay to prevent CPU hogging
    
    logging.info("Serial reader thread stopped")

def send_data(ser, data, delay, stop_event):
    """Send data rows over serial and display ESP32 feedback."""
    logging.info("Starting data transmission. Press Ctrl+C to stop.")
    print("\n" + "🔄 Sending flight data...\n")
    
    try:
        # ENVIAR COMANDO DE RESET ANTES DE COMEÇAR
        logging.info("Sending RESET command to Arduino...")
        ser.write(b"RESET\n")
        time.sleep(0.5)  # Aguardar reset
        
        # Limpar qualquer feedback do reset
        while ser.in_waiting > 0:
            ser.readline()
        
        # Clear any pending data in serial buffer
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # Small delay to ensure ESP32 is ready
        time.sleep(0.5)
        
        sent_count = 0
        for i, row in enumerate(data):
            # Extract values, handle potential missing keys
            try:
                millis = float(row['millis'])
                altp = float(row['altp'])
                ax = float(row['ax'])
                ay = float(row['ay'])
                az = float(row['az'])
            except KeyError as e:
                logging.error(f"Missing column {e} in row {i}. Skipping.")
                continue
            except ValueError as e:
                logging.error(f"Invalid numeric value in row {i}: {e}. Skipping.")
                continue

            # Format as: millis,altp,ax,ay,az
            line = f"{millis:.6f},{altp:.3f},{ax:.3f},{ay:.3f},{az:.3f}\n"
            
            # Send data
            ser.write(line.encode())
            sent_count += 1
            
            # Optional delay for real-time simulation
            if delay > 0:
                time.sleep(delay)
            
            # Log progress
            if sent_count % 10 == 0:
                logging.debug(f"Sent {sent_count}/{len(data)} rows...")
                
        logging.info(f"✅ All {sent_count} rows sent successfully.")
        
    except KeyboardInterrupt:
        logging.info("⚠️ Transmission interrupted by user.")
    except serial.SerialException as e:
        logging.error(f"Serial communication error: {e}")
    finally:
        stop_event.set()  # Signal reader thread to stop
        time.sleep(0.5)   # Give reader thread time to finish
        ser.close()
        logging.info("Serial port closed.")

def main():
    args = parse_arguments()
    setup_logging(args.verbose)

    # Optional port listing
    if args.verbose and HAS_LIST_PORTS:
        ports = [port.device for port in serial.tools.list_ports.comports()]
        logging.debug(f"Available serial ports: {ports}")

    # Open serial connection
    ser = open_serial(args.port, args.baud)
    
    # Read CSV data
    data = read_csv(args.file)
    
    # Create stop event for thread coordination
    stop_event = threading.Event()
    
    # Start serial reader thread to capture ESP32 feedback
    reader_thread = threading.Thread(target=serial_reader, args=(ser, stop_event))
    reader_thread.daemon = True  # Thread will exit when main thread exits
    reader_thread.start()
    
    # Wait a moment for reader thread to start
    time.sleep(0.5)
    
    send_data(ser, data, args.delay, stop_event)
    
    # Wait for reader thread to finish (or timeout)
    reader_thread.join(timeout=2.0)
    
    print("\n" + "="*60)
    print("🏁 Flight data transmission completed")
    print("="*60)

if __name__ == '__main__':
    main()