# R.F.P. — Resilient File Protector

[![CI](https://github.com/VasenkovAA/Resilient-File-Protector/actions/workflows/ci.yml/badge.svg)](https://github.com/VasenkovAA/Resilient-File-Protector/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-see%20LICENSE-blue)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt 6](https://img.shields.io/badge/Qt-6.8%20LTS-green.svg)](https://www.qt.io/)
[![OpenSSL](https://img.shields.io/badge/OpenSSL-3.x-red.svg)](https://www.openssl.org/)

> Replace `OWNER/rfp` in the badge URLs with the actual repository path and confirm the workflow filename.

**R.F.P.** is a C++20 / Qt 6 project for hiding private data inside ordinary‑looking files.
The first stage focuses on steganography in raster images (primarily PNG), with an encryption layer built on **OpenSSL**.

---

## Features

- **LSB steganography** with two slot‑selection modes:
  - **Uniform** — sequential or shuffled (backward‑compatible).
  - **Smart** — dispersion‑based filtering and sorting for better visual concealment.
- **Cryptography** (`rfp_crypto`, OpenSSL 3.x):
  - Ciphers — AES‑GCM, ChaCha20‑Poly1305, AES‑CBC, AES‑CTR
  - KDF — PBKDF2‑HMAC‑SHA256/512, scrypt
  - Hash — SHA‑2, SHA‑3, BLAKE2b, HMAC
  - CSPRNG, secure memory wipes, constant‑time compare
  - Encoding — Base64, Hex
  - Algorithm registry with backend introspection
- **Encrypted payload wrapper** (`rfp::payload`): self‑describing `RFP1` blob
  (cipher, KDF, iterations, salt and IV travel with the ciphertext; only the
  password is needed on extraction).
- **CRC32 integrity check** of the hidden payload.
- **Qt 6 GUI** with live capacity estimation, encryption panel and full parameter control.
- **CLI utility** for scripting and automation: end‑to‑end file I/O with native
  PNG and binary PPM/PGM, `--password-stdin`, `--header on|off` — no Qt needed.
- **No metadata pollution** — the carrier remains a normal raster image (no EXIF, no PNG text chunks, no custom headers).

---

## Design decision

The application does **not** write anything into image metadata. Extraction requires the **same parameters** that were used during embedding; the GUI displays them after every operation so they can be recorded.

If the payload was encrypted, the cipher / KDF / iteration count and salt / IV live inside the `RFP1` blob itself — only the password needs to be reproduced on extraction.

---

## Requirements

- CMake 3.24+
- C++20 compiler (GCC 13+, Clang 17+, MSVC 2022)
- Ninja (or any generator)
- **OpenSSL 3.x** — system package, vcpkg, Homebrew, or the bundled submodule
- **libpng** (`libpng-dev` on Debian/Ubuntu) — required by the CLI
- **Qt 6.8 LTS** (`Core`, `Gui`, `Widgets`, `Concurrent`) — optional, only for the GUI target
- **GoogleTest** — for the test suite
- `gcovr` — optional, for coverage reports

---

## Quick start

```bash
git clone <repo> && cd rfp
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Available presets (`cmake --list-presets`):

| Preset      | Build type | GUI | CLI | Tests | `-Werror` | Purpose                          |
|-------------|------------|:---:|:---:|:-----:|:---------:|----------------------------------|
| `dev`       | Debug      | ✔  | ✔  | ✔    | ✗        | Day‑to‑day development           |
| `ci`        | Release    | ✔  | ✔  | ✔    | ✔        | Continuous integration           |
| `core-only` | Debug      | ✗  | ✔  | ✔    | ✗        | Headless builds without Qt       |

Override any preset value on the fly:

```bash
cmake --preset dev -DRFP_BUILD_GUI=OFF -DRFP_WARNINGS_AS_ERRORS=ON
```

### Top‑level CMake options

| Option | Default | Meaning |
|--------|:-------:|---------|
| `RFP_BUILD_GUI`              | `ON`   | Build the Qt 6 GUI application. |
| `RFP_BUILD_CLI`              | `ON`   | Build the command‑line utility. |
| `RFP_BUILD_TESTS`            | `ON`   | Build the GoogleTest suite. |
| `RFP_WARNINGS_AS_ERRORS`     | `OFF`  | Promote warnings to errors (`-Werror` / `/WX`). |
| `RFP_USE_SUBMODULE_OPENSSL`  | `AUTO` | OpenSSL source selection (see below). |
| `RFP_ENABLE_COVERAGE`        | `OFF`  | Instrument the build for coverage measurement. |

---

## Building

### Dev Container (recommended for VS Code)

The repository ships a `.devcontainer/` with a Dockerfile and `devcontainer.json` containing all dependencies (CMake, Ninja, GCC, Qt 6, GoogleTest, OpenSSL, libpng).

1. Open the repo in VS Code → **Reopen in Container**.
2. In the container terminal:

   ```bash
   cmake --preset dev
   cmake --build --preset dev
   ctest  --preset dev
   ```

### Docker / Podman

```bash
docker build -t rfp .
docker run -it --rm -v $(pwd):/workspace rfp bash
```

For the GUI inside the container, forward X11:

```bash
xhost +local:docker
docker run -it --rm -v $(pwd):/workspace \
  -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix rfp bash
```

### Native — Linux (Ubuntu 24.04)

```bash
sudo apt install -y build-essential cmake ninja-build \
    qt6-base-dev qt6-tools-dev libgl1-mesa-dev \
    libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 \
    libxcb-image0 libxcb-keysyms1 libxcb-render-util0 \
    libxcb-xinerama0 libxcb-xinput0 \
    libgtest-dev libssl-dev libpng-dev pkg-config git ca-certificates

cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

### Native — Windows

Required: Visual Studio 2022 (C++ workload), CMake 3.24+, Ninja, Qt 6.8 LTS, vcpkg (for GoogleTest, OpenSSL and libpng).

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install gtest:x64-windows openssl:x64-windows libpng:x64-windows

cmake --preset dev -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build --preset dev
ctest  --preset dev
```

Deploy Qt DLLs next to the GUI executable:

```powershell
windeployqt.exe build\dev\src\gui\Release\rfp-gui.exe --release --no-translations
```

If OpenSSL was built as a shared library, copy `libcrypto-3-x64.dll` alongside the binaries (or add its directory to `PATH`). A submodule build produces a static OpenSSL — no extra DLLs required.

### Native — macOS

```bash
brew install cmake ninja googletest openssl@3 libpng
export OPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
export PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig"

cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

Install Qt 6.8 LTS separately (official installer or `aqtinstall`).

---

## OpenSSL integration

`rfp_crypto` links against **`libcrypto` only** — `libssl` is intentionally **not** linked, since the project performs symmetric cryptography only and has no TLS component.

The integration logic lives in `cmake/OpenSSL.cmake` and is controlled by the CMake cache variable `RFP_USE_SUBMODULE_OPENSSL`:

| Value           | Behaviour |
|-----------------|-----------|
| `AUTO` *(default)* | Use system OpenSSL if found; otherwise build from `third_party/openssl`. |
| `ON`            | Always build OpenSSL from the `third_party/openssl` submodule (static). |
| `OFF`           | Always use system OpenSSL; fail if not found. |

When the submodule is used, OpenSSL is built statically via `ExternalProject_Add` and exposed as the imported targets `OpenSSL::Crypto` and `OpenSSL::SSL`.

**Recommended:** rely on the system package. Initialise the submodule only if needed:

```bash
git submodule update --init --recursive third_party/openssl
cmake --preset dev -DRFP_USE_SUBMODULE_OPENSSL=ON
```

---

## Steganography parameters

**Basic (both modes)**

- **Bits per channel** — number of LSBs to use (1–4).
- **Seed** — random seed for shuffling; `0` disables shuffle.
- **Channels** — which colour channels (R, G, B, A) are used.
- **Payload size** — number of bytes to read during extraction. If the
  embed wrote a 4‑byte size header (see below), the size is read from it
  automatically.

**Smart mode**

- **Window size** — 3, 5, 7, 9, 11, 13.
- **Dispersion metric** — `Luminance`, `Per‑channel`, or `Sum`.
- **Threshold** — minimum dispersion value; slots below it are discarded.
- **Apply shuffle after sorting** — shuffle the sorted list using the seed.

The GUI provides an **Auto** button that suggests a threshold (70th percentile of all dispersions) for the loaded image.

---

## Size header (optional)

Both GUI and CLI can prefix the payload with a 4‑byte big‑endian length field:

- **Write payload size header** in the GUI (Settings dialog) or `--header on` in the CLI.
- When enabled, extraction reads the size from the header instead of requiring
  the user to remember it.
- When disabled (`--header off` in the CLI, checkbox cleared in the GUI),
  extraction requires an explicit payload size (`--payload-size N` /
  the corresponding GUI field).
- **The mode must match between embed and extract.** The GUI mirrors the
  current setting in the Extract tab so it is visible at a glance; Copy/Paste
  params carries it along.

---

## Encryption

Optional and independent from steganography. Two GUI panels (**Encryption** on
the Embed tab, **Decryption** on the Extract tab) and the CLI `--password` /
`--password-stdin` / `RFP_PASSWORD` set of options enable it.

When enabled, the text is wrapped into a self‑describing `RFP1` payload before
being hidden in the image:

```
offset  size  field
------  ----  ----------------------------------------
     0     4  magic 'R','F','P','1'
     4     1  version (1)
     5     1  cipher id
     6     1  KDF id
     7     1  flags
     8     4  KDF iterations (big-endian)
    12     1  salt length
    13     1  IV length
    14     2  reserved
    16     N  salt
16 + N     M  IV
    ...   ...  ciphertext
    ...    T   authentication tag (AEAD only)
```

Only the password is required to decrypt — cipher, KDF, iteration count, salt
and IV travel inside the blob. The default configuration
(AES‑256‑GCM + PBKDF2‑HMAC‑SHA256, 100 000 iterations) adds **60 bytes** of
overhead.

**Recommendation:** always use AEAD (AES‑GCM or ChaCha20‑Poly1305). CBC/CTR
are available for compatibility only and provide confidentiality without
integrity.

---

## Running

### GUI

| Platform      | Path |
|---------------|------|
| Linux/macOS   | `build/dev/src/gui/rfp-gui` (`rfp-gui.app` on macOS) |
| Windows       | `build\dev\src\gui\Release\rfp-gui.exe` |

### CLI

```bash
./build/bin/rfp-cli --help
./build/bin/rfp-cli crypto-info
./build/bin/rfp-cli self-test --mode smart --threshold 50 --window 5 --metric luminance --shuffle on
```

End‑to‑end PNG + encryption example:

```bash
# Embed
printf 'hunter2\n' | ./build/bin/rfp-cli embed cover.png stego.png \
    --text "Top secret" --password-stdin

# Extract
printf 'hunter2\n' | ./build/bin/rfp-cli extract stego.png --password-stdin
```

---

## Tests

The suite is built on **GoogleTest** and registered with CTest via `gtest_discover_tests` — every test case appears as a separate CTest entry.

```bash
ctest --preset dev
ctest --preset dev --output-on-failure
ctest --preset dev -R 'Cipher|Hash|Kdf|Payload' -j 4
```

Covered areas:

- **Steganography** — round‑trips in both modes, capacity calculation, dispersion metrics, slot‑selection edge cases.
- **Cryptography** — reference vectors (NIST SP 800‑38D, RFC 8439, RFC 4231, RFC 4648, RFC 6070), auto‑IV, tampering detection, error paths, thread‑safety.
- **Payload wrapper** — round‑trips across all ciphers and KDFs, wrong‑password detection, tampered magic/ciphertext/iterations, wire‑format invariants, `isPayload()`.

### Coverage

```bash
cmake -S . -B build/cov -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DRFP_BUILD_GUI=OFF \
      -DRFP_ENABLE_COVERAGE=ON
cmake --build build/cov --target coverage
```

Reports land in `build/cov/coverage/` (`coverage.txt`, `index.html`, `coverage.xml`, `coverage.json`). Backend selection is automatic: `gcovr` (preferred) → `lcov` on Linux/macOS, `OpenCppCoverage` on Windows MSVC.

---

## Repository layout

```
.
├── .devcontainer/                 # reproducible dev environment
├── .github/workflows/             # CI for Linux / Windows / macOS
├── cmake/                         # CompilerWarnings, Coverage, OpenSSL, ProjectOptions
├── docs/
├── examples/
├── include/rfp/{core,crypto,payload,stego}
├── src/{cli,core,crypto,gui,payload,stego}
├── tests/{core,crypto,payload,stego}
└── third_party/openssl/           # optional submodule
```

---

## License

See [`LICENSE`](LICENSE).
