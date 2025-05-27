#!/bin/bash
# ThermortoZigBee Firmware Flash Script
# This script helps flash the ESP32-C6 with the Thermor Zigbee firmware

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
PORT="/dev/ttyUSB0"
BAUD="460800"
CHIP="esp32c6"
BUILD_DIR="../firmware/build"

# Function to print colored output
print_color() {
    color=$1
    shift
    echo -e "${color}$@${NC}"
}

# Check if running as root (not recommended)
if [ "$EUID" -eq 0 ]; then 
   print_color $YELLOW "Warning: Running as root is not recommended!"
fi

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -b|--baud)
            BAUD="$2"
            shift 2
            ;;
        --erase)
            ERASE_FLASH=1
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  -p, --port PORT    Serial port (default: /dev/ttyUSB0)"
            echo "  -b, --baud BAUD    Baud rate (default: 460800)"
            echo "  --erase            Erase flash before programming"
            echo "  -h, --help         Show this help message"
            exit 0
            ;;
        *)
            print_color $RED "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Header
echo "====================================="
echo "ThermortoZigBee Firmware Flash Tool"
echo "====================================="
echo ""

# Check if ESP-IDF is sourced
if [ -z "$IDF_PATH" ]; then
    print_color $RED "Error: ESP-IDF environment not found!"
    echo "Please run: source /path/to/esp-idf/export.sh"
    exit 1
fi

print_color $GREEN "✓ ESP-IDF environment found: $IDF_PATH"

# Check if serial port exists
if [ ! -e "$PORT" ]; then
    print_color $RED "Error: Serial port $PORT not found!"
    echo "Available ports:"
    ls /dev/tty* | grep -E "(USB|ACM)" || echo "No USB serial ports found"
    exit 1
fi

print_color $GREEN "✓ Serial port found: $PORT"

# Check if firmware is built
if [ ! -f "$BUILD_DIR/thermor_zigbee.bin" ]; then
    print_color $YELLOW "Firmware not found. Building..."
    cd ../firmware
    idf.py build
    cd - > /dev/null
fi

# Show configuration
echo ""
echo "Configuration:"
echo "  Port: $PORT"
echo "  Baud: $BAUD"
echo "  Chip: $CHIP"
echo ""

# Safety warning
print_color $YELLOW "⚠️  SAFETY WARNING ⚠️"
echo "Ensure the ESP32-C6 is properly isolated from mains voltage!"
echo "Never flash while the device is connected to 230V!"
echo ""
read -p "Press Enter to continue or Ctrl+C to abort..."

# Erase flash if requested
if [ "$ERASE_FLASH" ]; then
    print_color $YELLOW "Erasing flash..."
    esptool.py --chip $CHIP --port $PORT erase_flash
    print_color $GREEN "✓ Flash erased"
fi

# Flash the firmware
print_color $YELLOW "Flashing firmware..."
echo "If the device doesn't enter bootloader mode automatically,"
echo "hold the BOOT button while pressing RESET."
echo ""

cd ../firmware
idf.py -p $PORT -b $BAUD flash

# Monitor after flashing
print_color $GREEN "✓ Firmware flashed successfully!"
echo ""
echo "Starting monitor (Ctrl+] to exit)..."
echo ""

idf.py -p $PORT monitor

# Cleanup
cd - > /dev/null

print_color $GREEN "Done!"