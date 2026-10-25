# NERV — The Nervous System of Your Computer

"Your machine is alive. See what it's thinking."

A production-grade, real-time native C++ desktop application that transforms live Windows operating-system activity into a beautiful, interactive, real-time visualization.

## Features
- **Real-Time OS Telemetry**: Genuine measurements from Windows performance counters, `GetSystemTimes`, `GlobalMemoryStatusEx`, `CreateToolhelp32Snapshot`, `GetProcessTimes`, `GetProcessMemoryInfo`, and `GetIfTable2`.
- **Living Process Graph**: Multiple visualization layouts:
  - *Neural Graph*: Force-directed organic network of active processes
  - *Hierarchy*: Parent-child tree hierarchy
  - *Resource Clusters*: Clustered by CPU, Memory, Disk, and Background states
  - *Timeline Layout*: Radial activity ring
- **Resource Shockwaves**: Dynamic visual shockwaves when system or process metrics cross configured thresholds.
- **Interactive Process Inspector**: Search, filter, pin, follow, and inspect any active process with CPU/RAM sparkline history.
- **System Activity Timeline & Ring Buffers**: Bounded time-series storage for CPU, RAM, Disk I/O, and Network throughput with JSON/CSV export.
- **Custom Shaders & Bloom Glow**: Custom GLSL OpenGL 3.3 Core shaders with deep negative space, bloom post-processing, and smooth antialiased node and edge rendering.
- **Non-blocking Threading Architecture**: Telemetry collected on a dedicated sampling thread with lock-free/short-lived synchronization.
- **Comprehensive Test Suite**: Unit and integration tests for calculations, memory metrics, PID reuse detection, ring buffers, and threshold event logic.

## Technology Stack
- **Language**: C++20
- **Build System**: CMake 3.20+
- **Graphics & Windowing**: OpenGL 3.3 Core, GLFW 3.4, GLAD
- **User Interface**: Dear ImGui
- **Serialization**: nlohmann/json
- **Testing**: Catch2 v3

## Building
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\bin\Release\nerv.exe
```
