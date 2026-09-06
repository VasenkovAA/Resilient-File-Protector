# R.F.P. — Resilient File Protector

**Resilient File Protector** is a C++20/Qt 6 project for hiding private data inside ordinary‑looking files.  
The first implementation stage focuses on steganography in raster images, primarily PNG.

The project is intentionally split into an independent core and a GUI application:

- `rfp_core` — common byte buffers, errors and utility algorithms;
- `rfp_stego` — steganography algorithms independent from Qt;
- `rfp_crypto` — placeholder module for the future encryption stage;
- `rfp_gui` — Qt 6.8 LTS desktop application;
- `rfp_cli` — small command‑line utility useful for testing and automation;
- `tests` — GoogleTest‑based unit tests.

## Current stage

- CMake‑based C++20 build with presets.
- Qt 6 GUI with full steganography controls.
- Library targets separated from GUI.
- GoogleTest integration.
- GitHub Actions CI for Linux, Windows and macOS.
- DevContainer for reproducible development.
- **LSB steganography with two slot selection modes:**
  - **Uniform** – sequential or shuffled (backward‑compatible).
  - **Smart** – dispersion‑based filtering and sorting for better visual concealment.
- GUI displays real‑time capacity and CRC32 integrity checks.
- CLI self‑test supports all parameters.

## Important design decision

The application does **not** write anything into image metadata such as EXIF, PNG text chunks or custom file headers.  
The image remains a normal raster image.

Extraction requires the **same** parameters that were used during embedding.  
The GUI shows all parameters after embedding, so you can record them.

## Steganography parameters

### Basic (for both modes)

- **Bits per channel** – number of LSBs to use (1–4).
- **Seed** – random seed for shuffling; `0` means no shuffle.
- **Channels** – select which colour channels (R, G, B, A) are used.
- **Payload size** (for extraction) – number of bytes to read.

### Smart mode specific

- **Mode** – choose `Uniform` or `Smart`.
- **Window size** – 3, 5, 7, 9, 11, 13.
- **Dispersion metric** – `Luminance`, `Per‑channel`, `Sum`.
- **Threshold** – minimum dispersion value; slots below this are discarded.
- **Apply shuffle after sorting** – if enabled, the sorted list is shuffled using the seed.

The GUI provides an **Auto** button that suggests a threshold (70th percentile of all dispersions) for the loaded image.

---

## Build and run

The project supports several ways to build and run the code. Choose the one that fits your environment best.

### 1. Dev Container (recommended for VS Code)

The repository includes a `.devcontainer` folder with a `Dockerfile` and `devcontainer.json`. This sets up a complete development environment with all dependencies (CMake, Ninja, GCC, Qt 6.8, GoogleTest).

**Steps:**

1. Open the repository in Visual Studio Code.
2. When prompted, click **“Reopen in Container”** (or run the command *Dev Containers: Reopen in Container*).
3. Once the container is running, open a terminal and build:
   ```bash
   cmake --preset dev
   cmake --build --preset dev
   ```
4. Run tests:
   ```bash
   ctest --preset dev
   ```

All build outputs are placed inside the container and are not persisted to the host unless you copy them.

---

### 2. Docker / Podman (using the root Dockerfile)

A `Dockerfile` is provided at the project root. It installs all system dependencies, copies the source code, and sets up the working directory.

**Build the image:**

```bash
docker build -t rfp .
# or with podman:
podman build -t rfp .
```

**Run an interactive shell inside the container:**

```bash
docker run -it --rm -v $(pwd):/workspace rfp bash
```

Once inside, you can build and test as usual:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

**Run the GUI from inside the container** (requires X11 forwarding or a Wayland socket):

```bash
# Allow X11 connections (on Linux host)
xhost +local:docker
docker run -it --rm \
  -v $(pwd):/workspace \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  rfp bash
# inside container:
./build/dev/src/gui/rfp-gui
```

For Podman, replace `docker` with `podman`.

---

### 3. Native build on Linux

**Install dependencies (Ubuntu 24.04):**

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build \
    qt6-base-dev qt6-tools-dev libgl1-mesa-dev \
    libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 \
    libxcb-image0 libxcb-keysyms1 libxcb-render-util0 \
    libxcb-xinerama0 libxcb-xinput0 \
    libgtest-dev git ca-certificates
```

> **Note:** If your distribution does not provide Qt 6.8 LTS, use the Qt online installer or `aqtinstall`.

**Build:**

```bash
cmake --preset dev
cmake --build --preset dev
```

**Run tests:**

```bash
ctest --preset dev
```

**Run the GUI:**

```bash
./build/dev/src/gui/rfp-gui
```

**Run the CLI:**

```bash
./build/dev/src/cli/rfp-cli --help
```

For a Qt‑free build (core + CLI only):

```bash
cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only
```

---

### 4. Native build on Windows

**Required tools:**

- Visual Studio 2022 (with C++ workload)
- CMake 3.24+
- Ninja (or use Visual Studio generator)
- Qt 6.8 LTS (installer or `aqtinstall`)
- vcpkg (for GoogleTest)

**Setup vcpkg (example):**

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install gtest:x64-windows
```

**Configure and build:**

Open a *Developer Command Prompt for VS 2022* or use PowerShell with the MSVC environment loaded.

```powershell
cmake --preset dev -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build --preset dev
ctest --preset dev
```

**Run the GUI:**

```powershell
.\build\dev\src\gui\Release\rfp-gui.exe
```

If you get missing DLL errors, see the section below on how to handle Qt DLLs.

---

### 5. Native build on macOS

**Install dependencies via Homebrew:**

```bash
brew install cmake ninja googletest
```

Install Qt 6.8 LTS separately (using the official installer or `aqtinstall`).  
Then build as on Linux:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The GUI executable will be inside the application bundle:

```bash
open ./build/dev/src/gui/rfp-gui.app
```

---

## Running tests and applications

### Run all unit tests

```bash
ctest --preset dev
```

To run a specific test, use:

```bash
ctest --preset dev -R <test_name_pattern>
```

### Run the GUI application

After a successful build, the GUI executable is located at:

- **Linux/macOS:** `build/dev/src/gui/rfp-gui` (or `rfp-gui.app` on macOS)
- **Windows:** `build\dev\src\gui\Release\rfp-gui.exe` (or `Debug` depending on build type)

### Run the CLI tool

The CLI is built alongside the GUI:

```bash
./build/dev/src/cli/rfp-cli --help
```

Example self‑test with smart mode:

```bash
./build/dev/src/cli/rfp-cli self-test --mode smart --threshold 50 --window 5 --metric luminance --shuffle on
```

---

## Handling Qt DLLs on Windows

When running the GUI on Windows, you may encounter missing Qt DLL errors. To resolve this:

1. **Use `windeployqt`** (recommended)  
   Open a Qt command prompt or add Qt bin to your PATH, then run:
   ```powershell
   windeployqt.exe build\dev\src\gui\Release\rfp-gui.exe --release --no-translations
   ```
   This copies all required Qt DLLs and plugins into the executable directory.

2. **Manually copy DLLs**  
   Copy the following Qt 6 DLLs from your Qt installation (`bin` folder) to the folder containing `rfp-gui.exe`:
   - `Qt6Core.dll`
   - `Qt6Gui.dll`
   - `Qt6Widgets.dll`
   - `Qt6Concurrent.dll`
   - and the `platforms/qwindows.dll` plugin (placed in a `platforms` subdirectory).

3. **Add Qt bin to `PATH`**  
   Set the environment variable `PATH=%PATH%;C:\Qt\6.8.0\msvc2022_64\bin` before launching the executable.

The same approach applies if you use other compilers (MinGW, etc.) – adjust paths accordingly.

---

## Requirements (summary)

- CMake 3.24+
- C++20 compiler (GCC 13+, Clang 17+, MSVC 2022)
- Ninja (or any generator)
- Qt 6.8 LTS with `Core`, `Gui`, `Widgets`
- GoogleTest (installed as a system package or via vcpkg)

---

## Repository layout

```
.
├── .devcontainer/
├── .github/workflows/
├── cmake/
├── docs/
├── examples/
├── include/rfp/
├── src/
│   ├── cli/
│   ├── core/
│   ├── crypto/
│   ├── gui/
│   └── stego/
└── tests/
```

---

## Roadmap

### Stage 1 — Image steganography (completed)

- PNG‑oriented raster steganography;
- text payload support;
- extraction by user‑supplied parameters;
- integrity check via CRC32;
- GUI workflow;
- **smart slot selection based on local dispersion**.

### Stage 2 — Source‑code‑like masking

- payload representation as ordinary‑looking source code;
- formatting rules;
- extraction parameters.

### Stage 3 — Block encryption

- pluggable encryption layer;
- algorithm can evolve independently from the steganography module;
- encryption should be applied before hiding data.

