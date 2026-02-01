#!/bin/bash

echo "=== FT2 GUI Designer Build Test ==="

# Check if SDL2 is installed
if ! pkg-config --exists sdl2; then
    echo "SDL2 not found. Installing..."
    sudo apt-get update
    sudo apt-get install -y libsdl2-dev
fi

# Clean and build
echo "Building project..."
make clean
make

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "Controls:"
    echo "  1-6: Select tools (Select, Button, Radio, Checkbox, H-Scroll, V-Scroll)"
    echo "  Ctrl+S: Save design"
    echo "  Ctrl+O: Load design"
    echo "  Delete: Delete selected widget"
    echo "  ESC: Exit"
    echo ""
    echo "Starting FT2 GUI Designer..."
    ./ft2_gui_designer
else
    echo "✗ Build failed!"
    exit 1
fi 