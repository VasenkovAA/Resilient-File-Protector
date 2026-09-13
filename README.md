# R.F.P. — Resilient File Protector

**Resilient File Protector** is a C++20 / Qt 6 project for hiding private data inside ordinary‑looking files.
The first implementation stage focuses on steganography in raster images, primarily PNG; the encryption layer is built on top of **OpenSSL**.

The project is intentionally split into an independent core and a GUI application:

- `rfp_core` — common byte buffers, errors and utility algorithms;
- `rfp_stego` — steganography algorithms independent from Qt;
- `rfp_crypto` — cryptography layer built on **OpenSSL** (replaces the earlier Botan placeholder);
- `rfp_gui` — Qt 6.8 LTS desktop application;
- `rfp_cli` — small command‑line utility useful for testing and automation;
- `tests` — GoogleTest‑based unit tests;
- `third_party/openssl` — OpenSSL submodule (optional, used when the system does not provide it).

## Current stage

- CMake‑based C++20 build with presets.
- Qt 6 GUI with full steganography controls.
- Library targets separated from GUI.
- GoogleTest integration with **128 unit tests** covering `core`, `stego` and `crypto`.
- GitHub Actions CI for Linux, Windows and macOS.
- DevContainer for reproducible development.
- **LSB steganography with two slot selection modes:**
  - **Uniform** – sequential or shuffled (backward‑compatible).
  - **Smart** – dispersion‑based filtering and sorting for better visual concealment.
- GUI displays real‑time capacity and CRC32 integrity checks.
- CLI self‑test supports all parameters.
- **Cryptography module (`rfp_crypto`) fully implemented on top of OpenSSL 3.x** with:
  - `Cipher` — AES‑GCM, ChaCha20‑Poly1305, AES‑CBC, AES‑CTR;
  - `Kdf` — PBKDF2‑HMAC‑SHA256/512, scrypt;
  - `Hash` — SHA‑2, SHA‑3, BLAKE2b, HMAC;
  - `Random` — CSPRNG;
  - `SecureMemory` — constant‑time compare + secure wipe;
  - `Encoding` — Base64, Hex;
  - `CryptoRegistry` — algorithm registry and backend info.

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

## OpenSSL integration

The `rfp_crypto` module links against OpenSSL. Only `libcrypto` is required — `libssl` is intentionally **not** linked, since the project performs symmetric cryptography only and has no TLS component.

The build system supports three modes, controlled by the CMake cache variable `RFP_USE_SUBMODULE_OPENSSL`:

| Value  | Behaviour |
|--------|-----------|
| `AUTO` *(default)* | Use system OpenSSL if found; otherwise build from `third_party/openssl`. |
| `ON`   | Always build OpenSSL from the `third_party/openssl` submodule. |
| `OFF`  | Always use system OpenSSL; fail if not found. |

The integration logic lives in `cmake/OpenSSL.cmake`. When the submodule is used, OpenSSL is built statically via `ExternalProject_Add` and exposed as the imported targets `OpenSSL::Crypto` and `OpenSSL::SSL`.

**Recommended:** rely on the system package (`libssl-dev` on Ubuntu, `openssl` via Homebrew on macOS, `openssl` via vcpkg on Windows). This is fast and avoids building OpenSSL on every clean checkout.

To initialise the submodule only when you actually need it:

```bash
git submodule update --init --recursive third_party/openssl
```

Then configure with:

```bash
cmake --preset dev -DRFP_USE_SUBMODULE_OPENSSL=ON
```

---

## Build configurations

The project ships with **CMake presets** for the most common scenarios. All presets live in `CMakePresets.json` and can be listed with:

```bash
cmake --list-presets
```

### Available presets

| Preset | Build type | GUI | CLI | Tests | `Werror` | Purpose |
|--------|------------|:---:|:---:|:-----:|:--------:|---------|
| `dev` | Debug | ✔ | ✔ | ✔ | ✗ | Day‑to‑day development. Fast, verbose, easy to debug. |
| `ci` | Release | ✔ | ✔ | ✔ | ✔ | Continuous integration. Optimized, all warnings are errors. |
| `core-only` | Debug | ✗ | ✔ | ✔ | ✗ | Core + CLI + crypto only, no Qt dependency. Useful on headless boxes. |

### Typical workflow

```bash
# --- Developer build (Debug) ---
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# --- CI-like build (Release, Werror) ---
cmake --preset ci
cmake --build --preset ci --parallel
ctest --preset ci

# --- Qt-free build (no GUI) ---
cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only
```

### Custom configurations

Every preset is just a named set of cache variables. To override a value on the fly:

```bash
cmake --preset dev -DRFP_BUILD_GUI=OFF
cmake --preset dev -DRFP_WARNINGS_AS_ERRORS=ON
cmake --preset dev -DRFP_USE_SUBMODULE_OPENSSL=ON
```

The top‑level `CMakeLists.txt` exposes the following options:

| Option | Default | Meaning |
|--------|:-------:|---------|
| `RFP_BUILD_GUI` | `ON` | Build the Qt 6 GUI application. |
| `RFP_BUILD_CLI` | `ON` | Build the command‑line utility. |
| `RFP_BUILD_TESTS` | `ON` | Build the GoogleTest suite. |
| `RFP_WARNINGS_AS_ERRORS` | `OFF` | Promote all warnings to errors (`-Werror` / `/WX`). |
| `RFP_USE_SUBMODULE_OPENSSL` | `AUTO` | OpenSSL source selection (see above). |
| `RFP_ENABLE_COVERAGE` | `OFF` | Instrument the build for coverage measurement. |

---

## Build and run

The project supports several ways to build and run the code. Choose the one that fits your environment best.

### 1. Dev Container (recommended for VS Code)

The repository includes a `.devcontainer` folder with a `Dockerfile` and `devcontainer.json`. This sets up a complete development environment with all dependencies (CMake, Ninja, GCC, Qt 6, GoogleTest, OpenSSL).

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
    libgtest-dev libssl-dev pkg-config \
    git ca-certificates
```

> **Note:** `libssl-dev` provides the system OpenSSL 3.x used by default. If your distribution does not provide Qt 6.8 LTS, use the Qt online installer or `aqtinstall`.

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

For a Qt‑free build (core + CLI + crypto only):

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
- Ninja (or use the Visual Studio generator)
- Qt 6.8 LTS (installer or `aqtinstall`)
- vcpkg (for GoogleTest **and** OpenSSL)
- *(only if you intend to build OpenSSL from the submodule)* Perl (Strawberry Perl) and `nmake` from the VS environment

**Setup vcpkg (example):**

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install gtest:x64-windows
.\vcpkg install openssl:x64-windows
```

**Configure and build:**

Open a *Developer Command Prompt for VS 2022* or use PowerShell with the MSVC environment loaded.

```powershell
cmake --preset dev -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build --preset dev
ctest --preset dev
```

If vcpkg is not used, CMake will try `find_package(OpenSSL)`. When neither system nor vcpkg OpenSSL is found, the fallback (submodule) will be used — configure it explicitly with:

```powershell
cmake --preset dev -DRFP_USE_SUBMODULE_OPENSSL=ON
```

**Run the GUI:**

```powershell
.\build\dev\src\gui\Release\rfp-gui.exe
```

If you get missing DLL errors, see the section below on how to handle Qt (and OpenSSL) DLLs.

---

### 5. Native build on macOS

**Install dependencies via Homebrew:**

```bash
brew install cmake ninja googletest openssl@3
```

Because OpenSSL is keg‑only on macOS, you may need to point CMake at it:

```bash
export OPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
export PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig"
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

## Tests

The test suite is built on top of **GoogleTest** and lives under `tests/`. All tests are registered with CTest via `gtest_discover_tests`, so each individual test case appears as a separate CTest entry.

### Running tests

```bash
# All tests for the current preset
ctest --preset dev

# Verbose output on failure
ctest --preset dev --output-on-failure

# Filter by name pattern (regex)
ctest --preset dev -R 'CryptoRegistry|Cipher|Hash|Kdf'
ctest --preset dev -R 'CipherTests\.'

# Parallel execution
ctest --preset dev -j 4
```

### Test layout

```
tests/
├── CMakeLists.txt
├── core/
│   └── Crc32Tests.cpp                     # CRC-32 reference vectors
└── crypto/
    ├── TestHelpers.h                      # hex helpers, buffer utilities
    ├── CryptoPlaceholderTests.cpp         # moduleStatus / backend info
    ├── CryptoRegistryTests.cpp            # ciphers/KDFs/hashes registry
    ├── CipherTests.cpp                    # NIST + RFC vectors, tampering, round-trips
    ├── HashTests.cpp                      # SHA-2/3, BLAKE2b, HMAC RFC 4231
    ├── KdfTests.cpp                       # PBKDF2, scrypt, error paths
    ├── RandomTests.cpp                    # CSPRNG properties
    ├── SecureMemoryTests.cpp              # wipe + constant-time compare
    ├── EncodingTests.cpp                  # Base64 RFC 4648, Hex
    ├── StegoParamsTests.cpp
    ├── StegoDispersionTests.cpp
    ├── StegoRoundTripTests.cpp
    ├── StegoSmartSelectionTests.cpp
    └── StegoEdgeCasesTests.cpp
```

### What is covered

- **Steganography** — round‑trips in both `Uniform` and `Smart` modes, capacity calculation, edge cases, dispersion metrics, slot selection errors.
- **Cryptography** — NIST SP 800‑38D (AES‑GCM), RFC 8439 (ChaCha20‑Poly1305), RFC 4231 (HMAC), RFC 4648 (Base64), RFC 6070 (PBKDF2), plus:
  - auto‑IV generation;
  - authentication failure on tampered ciphertext / tag / AAD / key;
  - error handling for unknown IDs, wrong key/IV/tag sizes;
  - thread‑safety of `DispersionCalculator` cache.

### Coverage measurement

Coverage is enabled with a single CMake option, no matter which platform you use:

```bash
cmake -S . -B build/cov -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DRFP_BUILD_GUI=OFF \
      -DRFP_ENABLE_COVERAGE=ON

cmake --build build/cov --target coverage
```

The `coverage` target runs the test suite and produces the reports under `build/cov/coverage/`:

| File | Purpose |
|------|---------|
| `coverage.txt` | Text summary printed to the terminal |
| `index.html` | Browseable per‑file HTML with line annotations |
| `coverage.xml` | Cobertura XML (for CI, Codecov, SonarQube) |
| `coverage.json` | Machine‑readable summary (gcovr only) |

The tooling is selected automatically:

| Platform | Preferred tool | Fallback |
|----------|----------------|----------|
| Linux / macOS | `gcovr` (`pip install gcovr`) | `lcov` + `genhtml` |
| Windows (MSVC) | OpenCppCoverage (`choco install opencppcoverage`) | — |

Current coverage for the cryptography module is **≥ 99 % lines / ≥ 95 % branches**. Remaining uncovered lines are defensive branches guarded with explicit `GCOVR_EXCL_START/STOP` markers and comments (e.g. `EVP_CIPHER_CTX_new() == nullptr`, provider lookup failures) — these are only reachable by fault injection into OpenSSL and are documented in the source.

To focus the report on the crypto module only:

```bash
gcovr --root . \
      --filter 'src/crypto/.*' \
      --filter 'include/rfp/crypto/.*' \
      --exclude-throw-branches \
      --exclude-unreachable-branches \
      --exclude-noncode-lines \
      --print-summary
```

---

## Running the applications

### GUI

After a successful build the GUI executable is located at:

- **Linux/macOS:** `build/dev/src/gui/rfp-gui` (or `rfp-gui.app` on macOS)
- **Windows:** `build\dev\src\gui\Release\rfp-gui.exe` (or `Debug` depending on build type)

### CLI

The CLI is built alongside the GUI:

```bash
./build/dev/src/cli/rfp-cli --help
```

Example self‑test with smart mode:

```bash
./build/dev/src/cli/rfp-cli self-test --mode smart --threshold 50 --window 5 --metric luminance --shuffle on
```

---

## Handling Qt and OpenSSL DLLs on Windows

When running the GUI on Windows, you may encounter missing DLL errors. To resolve this:

1. **Use `windeployqt`** (recommended)
   Open a Qt command prompt or add Qt bin to your PATH, then run:
   ```powershell
   windeployqt.exe build\dev\src\gui\Release\rfp-gui.exe --release --no-translations
   ```
   This copies all required Qt DLLs and plugins into the executable directory.

2. **Manually copy Qt DLLs**
   Copy the following Qt 6 DLLs from your Qt installation (`bin` folder) to the folder containing `rfp-gui.exe`:
   - `Qt6Core.dll`
   - `Qt6Gui.dll`
   - `Qt6Widgets.dll`
   - `Qt6Concurrent.dll`
   - and the `platforms/qwindows.dll` plugin (placed in a `platforms` subdirectory).

3. **OpenSSL DLLs**
   If you built against a **shared** OpenSSL (the vcpkg default), copy `libcrypto-3-x64.dll` next to the executables, or add its directory to `PATH`.
   If you use `RFP_USE_SUBMODULE_OPENSSL=ON`, OpenSSL is built **statically** (`no-shared`) and no extra DLLs are needed.

4. **Add Qt bin to `PATH`**
   Set the environment variable `PATH=%PATH%;C:\Qt\6.8.0\msvc2022_64\bin` before launching the executable.

The same approach applies if you use other compilers (MinGW, etc.) – adjust paths accordingly.

---

## Requirements (summary)

- CMake 3.24+
- C++20 compiler (GCC 13+, Clang 17+, MSVC 2022)
- Ninja (or any generator)
- Qt 6.8 LTS with `Core`, `Gui`, `Widgets` (optional — only for the GUI target)
- GoogleTest (installed as a system package or via vcpkg)
- **OpenSSL 3.x** (system package, vcpkg, Homebrew, or built from the bundled submodule)
- **gcovr** (optional — for coverage reports)
- On Windows, when building OpenSSL from the submodule: Perl + Visual Studio `nmake`

---

## Repository layout

```
.
├── .devcontainer/
├── .github/workflows/
├── cmake/
│   ├── CompilerWarnings.cmake
│   ├── Coverage.cmake             # cross-platform coverage target
│   ├── OpenSSL.cmake              # OpenSSL integration (system / submodule)
│   └── ProjectOptions.cmake
├── docs/
├── examples/
├── include/rfp/
│   ├── core/
│   ├── crypto/
│   └── stego/
├── src/
│   ├── cli/
│   ├── core/
│   ├── crypto/                    # rfp_crypto — links OpenSSL
│   ├── gui/
│   └── stego/
├── tests/
│   ├── core/
│   ├── crypto/
│   └── stego/
└── third_party/
    └── openssl/                   # git submodule (optional)
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

### Stage 3 — Block encryption (in progress)

- **OpenSSL integrated as the cryptography backend** (submodule + system fallback);
- pluggable encryption layer inside `rfp_crypto`;
- algorithm can evolve independently from the steganography module;
- encryption is applied **before** hiding data;
- planned algorithms: AES‑GCM / ChaCha20‑Poly1305 with a user‑supplied key or passphrase (PBKDF2 / Argon2 KDF).

---

## License

See `LICENSE` in the repository root.