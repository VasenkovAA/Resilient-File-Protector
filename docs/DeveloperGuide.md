# Building, Testing & Debugging Guide

This document covers day‑to‑day workflows for developing **R.F.P.** on every supported platform: containers, Linux, Windows, macOS. It also covers the test runner, coverage tooling, sanitizers, and typical debugging recipes.

> **Conventions used below**
> - `<repo>` — the repository root.
> - `<preset>` — one of `dev`, `ci`, `core-only`.
> - Commands assume you have already entered the repository root unless stated otherwise.

---

## Table of contents

1. [Mental model of the build](#1-mental-model-of-the-build)
2. [Environments at a glance](#2-environments-at-a-glance)
3. [Dev Container](#3-dev-container)
4. [Linux](#4-linux)
5. [Windows](#5-windows)
6. [macOS](#6-macos)
7. [CMake presets in depth](#7-cmake-presets-in-depth)
8. [OpenSSL backend selection](#8-openssl-backend-selection)
9. [Running tests](#9-running-tests)
10. [Coverage](#10-coverage)
11. [Sanitizers](#11-sanitizers)
12. [Debugging recipes](#12-debugging-recipes)
13. [Common problems & fixes](#13-common-problems--fixes)
14. [CI parity checklist](#14-ci-parity-checklist)

---

## 1. Mental model of the build

The project produces several independent artifacts from a single CMake tree:

```
              ┌──────────────┐
              │  rfp_core    │   byte buffers, errors, CRC32
              └──────┬───────┘
                     │
        ┌────────────┴────────────┐
        ▼                         ▼
 ┌──────────────┐         ┌──────────────┐
 │  rfp_stego   │         │  rfp_crypto  │──────► libcrypto (OpenSSL 3.x)
 │  (no Qt)     │         │  (no Qt)     │
 └──────┬───────┘         └──────┬───────┘
        │                        │
        └───────────┬────────────┘
                    ▼
        ┌─────────────────────┐         ┌──────────────┐
        │      rfp_cli        │         │   rfp_gui    │──► Qt6 Core/Gui/Widgets
        └─────────────────────┘         └──────────────┘
                    │
                    ▼
        ┌─────────────────────┐
        │    GoogleTest       │
        └─────────────────────┘
```

Key consequences for the developer:

- The **core, stego and crypto** libraries have **no Qt dependency**. If Qt is unavailable, `RFP_BUILD_GUI=OFF` still gives you a fully functional CLI and test suite.
- The **GUI is the only target that requires Qt**. All other targets are portable C++20.
- The **only external native dependency of the libraries is `libcrypto`** from OpenSSL. `libssl` is intentionally not linked.

---

## 2. Environments at a glance

| Environment      | Primary use                        | Qt available | OpenSSL source | Notes                                        |
|------------------|-------------------------------------|:------------:|----------------|----------------------------------------------|
| Dev Container    | Day‑to‑day development              | ✔            | system         | Reproducible; VS Code integration out of the box |
| Docker / Podman  | CI parity, headless builds          | ✔            | system         | GUI needs X11/Wayland forwarding              |
| Linux native     | Fast iteration, profiling           | ✔ (apt/aqt)  | system         | Recommended on Ubuntu 24.04                   |
| Windows native   | Shipping binaries, GUI testing      | ✔            | vcpkg/system   | `windeployqt` for DLL deployment              |
| macOS native     | GUI development, Apple silicon      | ✔            | Homebrew       | Keg‑only OpenSSL requires `OPENSSL_ROOT_DIR`  |

---

## 3. Dev Container

The repository ships a `.devcontainer/` with a pre‑baked image (CMake, Ninja, GCC, Qt 6, GoogleTest, OpenSSL). It is the fastest way to get a working build on any host OS.

### 3.1 Launch

1. Install **Docker Desktop** (or Podman) and the VS Code **Dev Containers** extension.
2. Open the repo in VS Code.
3. Command palette → *Dev Containers: Reopen in Container*.
4. Wait for the image build to finish; VS Code reopens inside the container.

### 3.2 Build and run

```bash
cmake --preset dev
cmake --build --preset dev
ctest  --preset dev --output-on-failure
```

### 3.3 Debug from VS Code

`.vscode/launch.json` should contain at least one configuration. A minimal Linux/GDB config:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug rfp-cli",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/dev/src/cli/rfp-cli",
      "args": ["--help"],
      "cwd": "${workspaceFolder}",
      "MIMode": "gdb",
      "setupCommands": [
        { "description": "Enable pretty printing",
          "text": "-enable-pretty-printing",
          "ignoreFailures": true }
      ]
    },
    {
      "name": "Debug rfp-gui",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/dev/src/gui/rfp-gui",
      "args": [],
      "cwd": "${workspaceFolder}",
      "MIMode": "gdb"
    }
  ]
}
```

### 3.4 Notes

- The container mounts the repo at `/workspace`. Build outputs stay inside the container unless you copy them out.
- Qt needs a display to launch the GUI. Either use VS Code's X11 forwarding (Linux host) or run the GUI outside the container after copying the binary.

---

## 4. Linux

### 4.1 Install dependencies (Ubuntu 24.04)

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build pkg-config git ca-certificates \
    qt6-base-dev qt6-tools-dev libgl1-mesa-dev \
    libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 \
    libxcb-image0 libxcb-keysyms1 libxcb-render-util0 \
    libxcb-xinerama0 libxcb-xinput0 \
    libgtest-dev libssl-dev
```

Optional:

```bash
# Coverage
pip install gcovr
# or
sudo apt install lcov

# Debugging tools
sudo apt install gdb valgrind linux-tools-common linux-tools-generic
```

### 4.2 Configure, build, test

```bash
cmake --preset dev
cmake --build --preset dev --parallel
ctest  --preset dev --output-on-failure
```

### 4.3 Headless / core-only build

On servers without Qt:

```bash
cmake --preset core-only
cmake --build --preset core-only --parallel
ctest  --preset core-only
```

### 4.4 Debugging with GDB

```bash
gdb --args ./build/dev/src/cli/rfp-cli self-test --mode smart --threshold 50 --window 5 --metric luminance
```

Useful GDB commands:

| Command              | Effect                                                |
|----------------------|-------------------------------------------------------|
| `break <file>:<line>`| Set a breakpoint                                      |
| `run` / `r`          | Start the program                                     |
| `bt`                 | Backtrace                                             |
| `info threads`       | List threads                                          |
| `p <expr>`           | Print expression                                      |
| `watch <var>`        | Watchpoint on a variable                              |
| `catch throw`        | Break on every C++ exception                          |
| `thread apply all bt`| Backtrace for every thread (great for deadlocks)      |

Pretty‑printing for STL/Qt is enabled automatically in the Dev Container.

### 4.5 Valgrind / memcheck

```bash
valgrind --leak-check=full --track-origins=yes \
         --error-exitcode=1 \
         ./build/dev/src/cli/rfp-cli self-test
```

### 4.6 Profiling

```bash
perf record -g ./build/ci/src/cli/rfp-cli self-test
perf report
```

For GUI latency analysis, use `perf record -g ./build/ci/src/gui/rfp-gui`.

---

## 5. Windows

### 5.1 Prerequisites

- **Visual Studio 2022** with the *Desktop development with C++* workload.
- **CMake** ≥ 3.24 (bundled with VS 2022 works).
- **Ninja** — `choco install ninja`, or use the *Ninja* bundled with VS.
- **Qt 6.8 LTS** — official installer or `aqtinstall`.
- **vcpkg** — for GoogleTest and OpenSSL.
- *(Optional)* **Strawberry Perl** — only if you intend to build OpenSSL from the submodule.

### 5.2 vcpkg setup

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install gtest:x64-windows openssl:x64-windows
```

### 5.3 Build

Open a **Developer Command Prompt for VS 2022** (or load the environment into PowerShell with `vcvars64.bat`):

```powershell
cmake --preset dev -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build --preset dev
ctest  --preset dev --output-on-failure
```

If you use the Visual Studio generator instead of Ninja, pass `-G "Visual Studio 17 2022" -A x64` on the first configure and drop `--preset dev` in favour of explicit `--build build/dev`.

### 5.4 Running the GUI

```powershell
.\build\dev\src\gui\Release\rfp-gui.exe
```

If DLLs are missing:

```powershell
windeployqt.exe .\build\dev\src\gui\Release\rfp-gui.exe --release --no-translations
```

Copy `libcrypto-3-x64.dll` from your vcpkg `installed\x64-windows\bin` (or add that folder to `PATH`) unless you built OpenSSL statically via the submodule.

### 5.5 Debugging with Visual Studio

Two options:

**A. Open the folder in VS 2022 directly**

1. *File → Open → Folder…* → select the repo root.
2. VS picks up `CMakePresets.json` automatically.
3. Pick the `dev` preset in the toolbar.
4. Set `rfp-cli` or `rfp-gui` as the startup item and press **F5**.

**B. Attach to a running process**

1. Launch `rfp-gui.exe` normally.
2. VS → *Debug → Attach to Process…* → select `rfp-gui.exe`.

### 5.6 Debugging with WinDbg (no VS)

```powershell
windbgx.exe -g -G .\build\dev\src\gui\Release\rfp-gui.exe
```

Common commands:

| Command    | Effect                        |
|------------|-------------------------------|
| `g`        | Go                            |
| `k`        | Stack                         |
| `~*k`      | Stack for all threads         |
| `!analyze -v` | Detailed crash analysis     |
| `.ecxr`    | Switch to exception context   |

### 5.7 Sanitizers on Windows

MSVC supports **AddressSanitizer** and **`/RTC1`** runtime checks. Enable via CMake flags:

```powershell
cmake --preset dev ^
  -DCMAKE_CXX_FLAGS="/fsanitize=address /Zi" ^
  -DCMAKE_EXE_LINKER_FLAGS="/INCREMENTAL:NO"
```

ASan on MSVC requires the *C++ AddressSanitizer* component in the VS installer.

---

## 6. macOS

### 6.1 Dependencies

```bash
brew install cmake ninja googletest openssl@3
```

Qt 6.8 LTS via the official installer or `aqtinstall`:

```bash
pip install aqtinstall
aqt install-qt mac desktop 6.8.0 clang_64 -O ~/Qt
```

### 6.2 OpenSSL paths

Homebrew OpenSSL is keg‑only, so CMake will not find it unless you point at it:

```bash
export OPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
export PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig"
```

Add these lines to `~/.zshrc` to persist them.

### 6.3 Build & test

```bash
cmake --preset dev
cmake --build --preset dev --parallel
ctest  --preset dev --output-on-failure
```

### 6.4 Running the GUI

The GUI is built as an app bundle:

```bash
open ./build/dev/src/gui/rfp-gui.app
```

### 6.5 Debugging with LLDB

```bash
lldb -- ./build/dev/src/cli/rfp-cli self-test
(lldb) breakpoint set --name main
(lldb) run
(lldb) bt
```

Xcode users can open the project via *File → Open* on `build/dev/rfp.xcodeproj` (generate with `-G Xcode`), or simply use *Debug → Attach to Process* on `rfp-gui`.

### 6.6 Instrumenting memory

```bash
# Leaks (built-in)
MallocStackLogging=1 leaks --atExit -- ./build/dev/src/cli/rfp-cli self-test

# AddressSanitizer
cmake --preset dev -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
```

---

## 7. CMake presets in depth

All presets are declared in `CMakePresets.json`. List them with:

```bash
cmake --list-presets
```

### 7.1 What each preset sets

| Preset      | `CMAKE_BUILD_TYPE` | GUI | CLI | Tests | Werror | Other                                    |
|-------------|--------------------|:---:|:---:|:-----:|:------:|------------------------------------------|
| `dev`       | `Debug`            | ✔  | ✔  | ✔    | ✗      | `CMAKE_EXPORT_COMPILE_COMMANDS=ON`        |
| `ci`        | `Release`          | ✔  | ✔  | ✔    | ✔      | Parallel build friendly                   |
| `core-only` | `Debug`            | ✗  | ✔  | ✔    | ✗      | No Qt lookup at all                       |

### 7.2 Overriding on the command line

Any CMake cache variable can be overridden after the preset:

```bash
cmake --preset dev \
      -DRFP_BUILD_GUI=OFF \
      -DRFP_WARNINGS_AS_ERRORS=ON \
      -DRFP_USE_SUBMODULE_OPENSSL=ON
```

### 7.3 Build targets

| Target             | Produces                                       |
|--------------------|------------------------------------------------|
| `rfp_core`         | static library                                 |
| `rfp_stego`        | static library                                 |
| `rfp_crypto`       | static library linking `OpenSSL::Crypto`       |
| `rfp_cli`          | CLI executable                                 |
| `rfp-gui`          | GUI executable (or `.app` bundle on macOS)     |
| `rfp_tests`        | GoogleTest executable, one binary per module   |
| `coverage`         | *(optional)* generate coverage reports         |

Build a single target:

```bash
cmake --build --preset dev --target rfp_tests
```

### 7.4 Multi‑config generators

With the Visual Studio or Xcode generators, use `--config`:

```bash
cmake --build build/dev --config Debug
cmake --build build/dev --config Release
ctest --test-dir build/dev -C Debug
```

---

## 8. OpenSSL backend selection

Controlled by the cache variable `RFP_USE_SUBMODULE_OPENSSL` (see `cmake/OpenSSL.cmake`):

| Value            | Behaviour |
|------------------|-----------|
| `AUTO` (default) | Prefer the system installation; fall back to `third_party/openssl`. |
| `ON`             | Always build the submodule statically. |
| `OFF`            | Require a system OpenSSL; fail configuration otherwise. |

### 8.1 Forcing the submodule

```bash
git submodule update --init --recursive third_party/openssl
cmake --preset dev -DRFP_USE_SUBMODULE_OPENSSL=ON
```

The submodule build uses `ExternalProject_Add` and exposes `OpenSSL::Crypto` and `OpenSSL::SSL` as imported targets. Note that `libssl` is built but not linked — it simply appears as a transitive artefact.

### 8.2 Forcing the system OpenSSL

```bash
cmake --preset dev -DRFP_USE_SUBMODULE_OPENSSL=OFF
```

Configuration fails fast if CMake cannot locate a suitable OpenSSL 3.x.

### 8.3 Verifying which backend was used

After configuration:

```bash
grep -E "OpenSSL|RFP_USE_SUBMODULE" build/dev/CMakeCache.txt
```

At runtime, `rfp_cli --version` prints the linked `libcrypto` build, and the crypto module exposes a backend string through `CryptoRegistry`.

---

## 9. Running tests

### 9.1 CTest

```bash
ctest --preset dev                       # all tests
ctest --preset dev --output-on-failure   # show output only on failure
ctest --preset dev -j 4                  # parallel
ctest --preset dev -R 'Cipher|Hash|Kdf'  # regex filter
ctest --preset dev -E 'Slow'             # exclude by regex
ctest --preset dev --rerun-failed        # re-run only failing tests
```

Because tests are registered with `gtest_discover_tests`, each GoogleTest case is a standalone CTest entry and can be selected individually.

### 9.2 GoogleTest directly

The individual test binaries live next to the libraries:

```
build/dev/tests/core/rfp_core_tests
build/dev/tests/crypto/rfp_crypto_tests
build/dev/tests/stego/rfp_stego_tests
```

Useful filters:

```bash
./build/dev/tests/crypto/rfp_crypto_tests --gtest_filter='CipherTests.*'
./build/dev/tests/crypto/rfp_crypto_tests --gtest_list_tests
./build/dev/tests/crypto/rfp_crypto_tests --gtest_repeat=10 --gtest_shuffle
```

### 9.3 Running a single test under a debugger

```bash
gdb --args ./build/dev/tests/stego/rfp_stego_tests \
    --gtest_filter='StegoRoundTripTests.*'
```

In VS Code, duplicate a launch configuration and set `args` accordingly.

### 9.4 Reproducing CI locally

```bash
cmake --preset ci
cmake --build --preset ci --parallel
ctest --preset ci --output-on-failure
```

The `ci` preset turns on `-Werror`, so warnings are treated as build failures — exactly as on the CI runners.

---

## 10. Coverage

### 10.1 Enabling

```bash
cmake -S . -B build/cov -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DRFP_BUILD_GUI=OFF \
      -DRFP_ENABLE_COVERAGE=ON
cmake --build build/cov --target coverage
```

Reports appear under `build/cov/coverage/`:

| File            | Purpose                                    |
|-----------------|--------------------------------------------|
| `coverage.txt`  | Text summary                               |
| `index.html`    | Browsable HTML with line annotations       |
| `coverage.xml`  | Cobertura XML (CI / Codecov / SonarQube)   |
| `coverage.json` | Machine‑readable summary (gcovr)           |

### 10.2 Backend selection

| Platform      | Preferred tool         | Fallback          |
|---------------|------------------------|-------------------|
| Linux / macOS | `gcovr`                | `lcov` + `genhtml`|
| Windows MSVC  | OpenCppCoverage        | —                 |

Install:

```bash
# Linux/macOS
pip install gcovr
# or
sudo apt install lcov

# Windows
choco install opencppcoverage
```

### 10.3 Focusing on a module

```bash
gcovr --root . \
      --filter 'src/crypto/.*' \
      --filter 'include/rfp/crypto/.*' \
      --exclude-throw-branches \
      --exclude-unreachable-branches \
      --exclude-noncode-lines \
      --print-summary
```

Defensive branches that are only reachable via fault injection into OpenSSL are marked with `GCOVR_EXCL_START/STOP` in the source and are documented there.

---

## 11. Sanitizers

Sanitizers are the cheapest way to catch memory and threading bugs. They are not enabled by default; pass the flags via CMake.

### 11.1 AddressSanitizer + UndefinedBehaviorSanitizer

```bash
cmake -S . -B build/asan -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DRFP_BUILD_GUI=OFF \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"

cmake --build build/asan --parallel
ctest --test-dir build/asan --output-on-failure
```

On success, the exit code is 0 and no diagnostics are printed. On failure, ASan prints a stack trace with the offending address.

### 11.2 ThreadSanitizer

```bash
cmake -S . -B build/tsan -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DRFP_BUILD_GUI=OFF \
      -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"

cmake --build build/tsan --parallel
ctest --test-dir build/tsan --output-on-failure
```

TSan is useful for verifying the thread‑safety of `DispersionCalculator` and the crypto registry.

### 11.3 MemorySanitizer (clang only)

```bash
cmake -S . -B build/msan -G Ninja \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_BUILD_TYPE=Debug \
      -DRFP_BUILD_GUI=OFF \
      -DCMAKE_CXX_FLAGS="-fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer"
```

MSan requires an instrumented C++ runtime; in practice use it inside the Dev Container where clang and libc++ are built with MSan support, or use the official LLVM Docker images.

### 11.4 Environment variables worth knowing

```bash
# Do not continue after the first error
export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1

# Print a symbolised report even when PIE
export ASAN_SYMBOLIZER_PATH=$(which llvm-symbolizer)

# ThreadSanitizer: fail on races
export TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1
```

---

## 12. Debugging recipes

### 12.1 Break on exception

GDB: `catch throw`, then `bt`.
LLDB: `breakpoint set -E c++`.
VS: *Debug → Windows → Exception Settings → C++ Exceptions → check “Thrown”*.

### 12.2 Logging

The core has an internal logging facility. Set the environment variable `RFP_LOG_LEVEL` to `trace`, `debug`, `info`, `warn`, `error` before launching. GUI builds additionally honour `QT_LOGGING_RULES` for Qt‑internal categories.

### 12.3 Core dump analysis (Linux/macOS)

```bash
ulimit -c unlimited
./build/dev/src/cli/rfp-cli self-test        # crashes → core.<pid>
gdb ./build/dev/src/cli/rfp-cli core.<pid>
(gdb) bt full
```

### 12.4 Windows minidump

Enable WER local dumps or attach a debugger. With VS: *Debug → Attach to Process* → *Debug → Save Dump As…*. Open the dump later with *File → Open → Dump File*.

### 12.5 Debugging OpenSSL

Build the crypto module in Debug and set a breakpoint on `EVP_CipherInit_ex` or `EVP_DigestFinal_ex`. With a system OpenSSL you will need symbols — on Ubuntu install `libssl3-dbgsym`; with vcpkg, `openssl` is built with debug info in the `debug` triplet.

### 12.6 Debugging the GUI's worker threads

The GUI pushes embedding/extraction work onto a `QThreadPool`. To catch issues:

- Set `QT_FATAL_WARNINGS=1` so any Qt warning aborts the process, then run under the debugger.
- Use *Debug → Windows → Threads* (VS) or `info threads` (GDB) to inspect all worker states during a hang.
- For data races, rebuild with TSan (see §11.2).

### 12.7 Reproducing a specific stego failure

Every parameter used for embedding is displayed by the GUI and printed by the CLI. To reproduce deterministically, capture:

- bits per channel,
- seed,
- channels,
- mode,
- window size,
- dispersion metric,
- threshold,
- shuffle flag,
- payload size.

The CLI self‑test accepts them all:

```bash
./build/dev/src/cli/rfp-cli self-test \
    --mode smart --threshold 50 --window 5 \
    --metric luminance --shuffle on \
    --bits 2 --seed 12345 --channels rgb \
    --input samples/carrier.png --payload samples/secret.bin
```

---

## 13. Common problems & fixes

| Symptom                                                     | Likely cause / fix                                                                                            |
|-------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------|
| `find_package(OpenSSL)` fails                               | Install `libssl-dev` / `openssl@3` / `openssl:x64-windows`, or configure with `-DRFP_USE_SUBMODULE_OPENSSL=ON`. |
| macOS: `Could not find OpenSSL`                             | Export `OPENSSL_ROOT_DIR=$(brew --prefix openssl@3)` and re‑run `cmake --preset dev`.                        |
| `Qt6Config.cmake not found`                                 | Add Qt's `lib/cmake` to `CMAKE_PREFIX_PATH` or use the *Qt x.y.z (MSVC 2022 64‑bit)* command prompt.          |
| Windows: `rfp-gui.exe` complains about missing Qt DLLs      | Run `windeployqt` on the executable (see §5.4).                                                                |
| Windows: missing `libcrypto-3-x64.dll`                      | Copy from vcpkg `installed\x64-windows\bin`, add to `PATH`, or build OpenSSL statically.                      |
| Linker error: `undefined reference to EVP_*`                | Wrong OpenSSL version (2.x picked up). Inspect `build/<preset>/CMakeCache.txt` for `OPENSSL_*` entries.       |
| Tests fail with `CryptoRegistry` mismatch                   | Different OpenSSL provider configuration; run `rfp-cli --version` to verify the linked backend.               |
| `-Werror` breaks the build after a compiler update          | Reproduce with `cmake --preset ci`; fix the warning or, as a last resort, add a targeted `#pragma`.           |
| Container GUI cannot connect to display                     | `xhost +local:docker` and pass `-e DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix`.                                 |
| Coverage target prints "no data"                            | Build with `-DRFP_ENABLE_COVERAGE=ON` and `CMAKE_BUILD_TYPE=Debug`; run tests via the `coverage` target.      |

---

## 14. CI parity checklist

Before pushing, run the same commands the CI uses locally:

```bash
# 1. Clean configure with CI preset
rm -rf build/ci
cmake --preset ci

# 2. Full build, warnings are errors
cmake --build --preset ci --parallel

# 3. Full test suite
ctest --preset ci --output-on-failure

# 4. Optional: sanitizers
cmake -S . -B build/asan -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug -DRFP_BUILD_GUI=OFF \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build/asan --parallel
ctest --test-dir build/asan --output-on-failure

# 5. Optional: coverage
cmake -S . -B build/cov -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug -DRFP_BUILD_GUI=OFF -DRFP_ENABLE_COVERAGE=ON
cmake --build build/cov --target coverage
```

If all five steps succeed on your machine, the CI matrix (Linux, Windows, macOS) will almost certainly succeed too.

