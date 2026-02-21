# RetroForge

A retro CRT-style system monitoring dashboard built with C++ and raylib. Authentic green-phosphor aesthetics, real-time system metrics, an embedded terminal, network diagnostics, anomaly detection, and a CPU stress tester — all wrapped in a self-contained single executable.

![RetroForge Screenshot](screenshot.png)

---

## Features

### Visuals
- **Authentic CRT shader** — scanlines, barrel distortion, phosphor glow, vignette, and subtle flicker
- **Multiple color themes** — Green Phosphor, Amber, White, Cyan, Red, and more
- **VGA retro font** support with automatic fallback

### System Monitoring
- **Live CPU, RAM, and Disk usage** — real Windows/Linux/macOS API data
- **Multi-drive disk monitoring** — all mounted drives with usage bars
- **Real-time network speed** — download and upload in KB/s or MB/s, auto-scaled
- **Process count and system uptime**
- **Computer name display**
- **Simulated mode** — smooth animated fake data for demo/screensaver use

### Anomaly Detector
Watches live metrics and triggers a red flashing alert when:
- **CPU** stays above 90% for 3+ consecutive seconds
- **RAM** exceeds 95%
- **Network** spikes 10× its 30-second rolling baseline

All anomaly events are logged with timestamps in the system log.

### CPU Stress Test
Press **F5** (in live monitoring mode) to peg all CPU cores for 30 seconds. Watch the anomaly detector trigger in real time. Press **F5** again to stop early. Progress shown in the bottom bar.

### Network Diagnostics
Built-in internet speed test against `speed.cloudflare.com`. Results saved to `speedtest_results.txt`.

### System Information
Full hardware and OS info panel — CPU name, core count, RAM, OS version, hostname.

### Embedded Terminal
Multi-tab terminal (up to 4 tabs) running native shell commands. Supports `cd` for directory navigation.
> **Note:** Use simple one-shot commands (`dir`, `ping`, `ipconfig`, `ls`). Interactive programs (`python`, `ssh`, etc.) are not supported.

### Customizable Widgets
Toggle any widget on/off from the Customize menu. Settings persist across sessions via `dashboard.cfg`.

---

## Download

**Windows users:** grab the latest release from the [Releases](../../releases) page. It's a single `.zip` — no installer, no runtime required. Just unzip and run `RetroForge.exe`.

---

## Building from Source

### Requirements
- CMake 3.16+
- C++17 compiler (MSVC 2019+, GCC 10+, Clang 12+)
- [vcpkg](https://github.com/microsoft/vcpkg)

### Windows (self-contained static build)

```cmd
vcpkg install raylib:x64-windows-static

mkdir build && cd build
cmake .. ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

Output: `build/Release/RetroForge.exe` — no DLL dependencies.

### Linux

```bash
sudo apt install libraylib-dev   # Ubuntu/Debian
# or: sudo pacman -S raylib      # Arch

mkdir build && cd build
cmake ..
make -j$(nproc)
./RetroForge
```

### macOS

```bash
brew install raylib

mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.logicalcpu)
./RetroForge
```

---

## Keyboard Controls

| Key | Action |
|-----|--------|
| `TAB` | Open / close menu |
| `↑ / ↓` | Navigate menu |
| `ENTER` | Select menu item |
| `ESC` | Back / close overlay |
| `F5` | Start / stop CPU stress test (live mode only) |
| `Ctrl+T` | New terminal tab |
| `Ctrl+W` | Close terminal tab |
| `Ctrl+Tab` | Switch terminal tab |

---

## Project Structure

```
RetroForge/
├── main.cpp                  # Entry point, input handling, game loop
├── dashboard.cpp / .h        # Widget rendering, stats update, anomaly detection
├── ui_menus.cpp / .h         # Onboarding, color theme, widget menus
├── system_monitor.cpp        # Windows system metrics (CPU, RAM, Disk, Net)
├── system_monitor_posix.cpp  # Linux/macOS system metrics
├── system_monitor.h          # Shared interface
├── speedtest.cpp / .h        # Internet speed test (Cloudflare)
├── stress_test.cpp / .h      # CPU stress test (all cores)
├── terminal.cpp / .h         # Multi-tab embedded terminal
├── theme.cpp / .h            # Color theme definitions
├── config.cpp / .h           # Settings persistence (dashboard.cfg)
├── CMakeLists.txt
└── resources/
    ├── shaders/crt.fsh       # CRT post-processing fragment shader
    └── fonts/VGA.ttf         # Optional VGA retro font
```

---

## License

MIT — do whatever you want with it.
