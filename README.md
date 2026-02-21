# RetroForge

A nostalgic 1980s/90s green-phosphor CRT monitor style system information dashboard built with raylib and C++.

## Features

- **Authentic CRT aesthetic**: Scanlines, barrel distortion, phosphor glow, subtle flicker, and vignette
- **Retro terminal UI**: Green-phosphor text with amber accents
- **Interactive keyboard controls**: Arrow key navigation with TAB menu toggle
- **Real-time system monitoring**: Actual CPU, RAM, and Disk usage from Windows APIs
- **Multiple view modes**: Switch between simulated and live monitoring
- **Animated system stats**: Smooth transitions and progress bars
- **Network monitoring**: Real-time network speed display
- **System log**: Scrolling log entries with fade-out effect
- **Flashing alerts**: Yellow anomaly indicator
- **1280×720 resolution**: Windowed and resizable

## Build Requirements

- CMake 3.16+
- C++17 compatible compiler
- raylib library (installed via package manager or vcpkg)

## Building

### Using vcpkg (recommended)

```bash
# Install raylib via vcpkg
vcpkg install raylib

# Build the project
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

### Using system package manager

**Ubuntu/Debian:**
```bash
sudo apt install libraylib-dev
mkdir build && cd build
cmake ..
make
```

**Windows (with vcpkg):**
```cmd
vcpkg install raylib:x64-windows
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

**macOS (with Homebrew):**
```bash
brew install raylib
mkdir build && cd build
cmake ..
make
```

## Running

After building, run the executable:

```bash
# Linux/macOS
./RetroForge

# Windows
.\RetroForge.exe
```

## Keyboard Controls

The dashboard features an interactive control panel accessible via keyboard:

- **TAB**: Toggle control panel menu on/off
- **UP/DOWN Arrow Keys**: Navigate through menu options
- **ENTER**: Select/activate the highlighted menu option
- **ESC**: Exit application

### Menu Options

1. **Dashboard View** - Simulated system data with smooth animations
2. **Real-Time Monitoring** - Live CPU, RAM, and Disk usage from Windows APIs
3. **Network Diagnostics** - Coming soon
4. **System Information** - Coming soon

The title bar shows `[SIM]` for simulated mode or `[LIVE]` for real-time monitoring.

## Optional: Retro Font

For an authentic retro look, place a VGA/DOS-style font at:
```
resources/fonts/VGA.ttf
```

The app will fall back to the default font if this file is not found.

## Project Structure

```
├── CMakeLists.txt          # CMake build configuration
├── main.cpp                # Main application and UI code
├── system_monitor.h        # System monitoring interface
├── system_monitor.cpp      # Windows API system monitoring implementation
├── resources/
│   ├── shaders/
│   │   └── crt.fsh        # CRT post-processing fragment shader
│   └── fonts/
│       └── VGA.ttf        # Optional retro font
└── README.md
```

## Next Steps

To enhance this dashboard with real functionality:

1. **Real System Monitoring**
   - Windows: Use `GetSystemTimes()` and `GlobalMemoryStatusEx()`
   - Linux: Read from `/proc/stat`, `/proc/meminfo`
   - macOS: Use `sysctl()` calls

2. **Network Speed Testing**
   - Implement HTTP download speed test
   - Add ping latency measurement

3. **External Data Sources**
   - Cryptocurrency price ticker via API
   - Weather information
   - Stock market data

4. **Interactive Features**
   - Keyboard controls for different views
   - Clickable elements for detailed info
   - Configuration menu

5. **Enhanced Visual Effects**
   - More sophisticated shader effects
   - Animated transitions
   - Particle effects for alerts

## License

This project is provided as-is for educational and entertainment purposes.
