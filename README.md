# R.F.P. — Resilient File Protector

**Resilient File Protector** is a C++20/Qt 6 project for hiding private data inside ordinary-looking files.
The first implementation stage focuses on steganography in raster images, primarily PNG.

The project is intentionally split into an independent core and a GUI application:

- `rfp_core` — common byte buffers, errors and utility algorithms;
- `rfp_stego` — steganography algorithms independent from Qt;
- `rfp_crypto` — placeholder module for the future encryption stage;
- `rfp_gui` — Qt 6.8 LTS desktop application;
- `rfp_cli` — small command-line utility useful for testing and automation;
- `tests` — GoogleTest-based unit tests.

## Current stage

- CMake-based C++20 build with presets.
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

Extraction requires the same parameters that were used during embedding.
The GUI shows all parameters after embedding, so you can record them.

## Steganography parameters

### Basic (for both modes)

- **Bits per channel** – number of LSBs to use (1–4).
- **Seed** – random seed for shuffling; `0` means no shuffle.
- **Channels** – select which colour channels (R, G, B, A) are used.
- **Payload size** (for extraction) – number of bytes to read.

### Smart mode specific

- **Mode** – choose `Uniform` or `Smart`.
- **Window size** – 3,5,7,9,11,13.
- **Dispersion metric** – `Luminance`, `Per‑channel`, `Sum`.
- **Threshold** – minimum dispersion value; slots below this are discarded.
- **Apply shuffle after sorting** – if enabled, the sorted list is shuffled using the seed.

The GUI provides an **Auto** button that suggests a threshold (70th percentile of all dispersions) for the loaded image.

## Requirements

Required:

- CMake 3.24+;
- C++20 compiler;
- Ninja or another CMake generator;
- Qt 6.8 LTS with `Core`, `Gui`, `Widgets`;
- GoogleTest installed as a package, not through CMake `FetchContent`.

Recommended:

- GCC 13+, Clang 17+ or MSVC 2022;
- Qt Creator or VS Code;
- DevContainer for Linux development.

## Build

### Configure

```bash
cmake --preset dev
```

### Build

```bash
cmake --build --preset dev
```

### Run tests

```bash
ctest --preset dev
```

### Build without GUI

Useful when Qt is not installed:

```bash
cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only
```

## Run

After building, the GUI executable is located under the selected build directory.
For example, on Linux:

```bash
./build/dev/src/gui/rfp-gui
```

The CLI target is also built:

```bash
./build/dev/src/cli/rfp-cli --help
```

CLI self‑test accepts all parameters, e.g.:

```bash
./build/dev/src/cli/rfp-cli self-test --mode smart --threshold 50 --window 5 --metric luminance --shuffle on
```

## Installing dependencies

### Ubuntu/Debian

System repositories may not contain Qt 6.8 LTS. For the exact Qt version, use the Qt online installer, `aqtinstall`, or the provided DevContainer.

For non-GUI/core-only builds:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build libgtest-dev
```

### Windows

Recommended setup:

- Visual Studio 2022 with C++ tools;
- CMake;
- Ninja;
- Qt 6.8 LTS;
- GoogleTest installed through vcpkg or another package manager.

Example with vcpkg:

```powershell
vcpkg install gtest:x64-windows
cmake --preset dev -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build --preset dev
ctest --preset dev
```

### macOS

Recommended setup:

```bash
brew install cmake ninja googletest
```

Install Qt 6.8 LTS separately using the Qt installer or `aqtinstall`.

## DevContainer

Open the repository in VS Code and choose:

```text
Dev Containers: Reopen in Container
```

The container installs:

- CMake;
- Ninja;
- GCC;
- GoogleTest;
- Qt 6.8 through `aqtinstall`.

Then run:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## Repository layout

```text
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

## Roadmap

### Stage 1 — Image steganography (completed)

- PNG-oriented raster steganography;
- text payload support;
- extraction by user-supplied parameters;
- integrity check via CRC32;
- GUI workflow;
- **smart slot selection based on local dispersion**.

### Stage 2 — Source-code-like masking

- payload representation as ordinary-looking source code;
- formatting rules;
- extraction parameters.

### Stage 3 — Block encryption

- pluggable encryption layer;
- algorithm can evolve independently from the steganography module;
- encryption should be applied before hiding data.

## Notes

This project is intended for real personal use but should not yet be treated as production-grade cryptographic software.
The steganography layer hides the presence of data; it does not provide cryptographic secrecy by itself.
