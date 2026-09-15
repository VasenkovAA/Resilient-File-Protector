# Command-line interface

The CLI tool `rfp-cli` is built alongside the GUI. It links the same
`rfp_core`, `rfp_stego`, `rfp_crypto` and `rfp_payload` libraries but has
**no Qt dependency** — image I/O uses libpng and a tiny built-in PPM/PGM
parser/serialiser.

Binary path: `build/bin/rfp-cli`.

## Commands

| Command | Purpose |
|---------|---------|
| `help`, `--help`, `-h` | Show usage. |
| `--version`, `-v` | Print version. |
| `crc <text>` | CRC32 of a UTF‑8 string. |
| `hash <text> [--algo <name>]` | Hash of text. `SHA-256` (default), `SHA-512`, `SHA3-256`, `SHA3-512`, `BLAKE2b`. |
| `encrypt <text> [pw-opts] [cipher-opts] [--hex]` | Produce an `RFP1` payload (Base64 by default, hex with `--hex`). |
| `decrypt <encoded> [pw-opts] [--hex]` | Decrypt an `RFP1` payload. |
| `embed <in> <out> --text <text> [pw-opts] [--header on\|off] [stego-opts]` | Embed text into a PNG or PPM/PGM image. |
| `extract <in> [pw-opts] [--header on\|off] [--payload-size N] [stego-opts]` | Extract text from a PNG or PPM/PGM image. |
| `self-test [stego-opts]` | Round‑trip smoke test on a synthetic image. |
| `crypto-info` | Print the OpenSSL backend version and available ciphers / KDFs / hashes. |

## Password options

Used by `encrypt`, `decrypt`, `embed`, `extract`:

| Option | Effect |
|--------|--------|
| `--password <pw>` | Inline password (visible in `ps(1)` — avoid in production). |
| `--password-stdin` | Read the password from stdin, stripping a trailing `\n` or `\r\n`. Recommended. |
| *(env `RFP_PASSWORD`)* | Fallback when neither of the above is set. |

Resolution order: `--password-stdin` > `--password` > `RFP_PASSWORD`.

## Encryption options

| Option | Values | Default |
|--------|--------|---------|
| `--cipher <name>` | `AES-128-GCM`, `AES-256-GCM`, `ChaCha20-Poly1305`, `AES-256-CBC`, `AES-256-CTR` | `AES-256-GCM` |
| `--kdf <name>` | `PBKDF2-HMAC-SHA256`, `PBKDF2-HMAC-SHA512`, `scrypt` | `PBKDF2-HMAC-SHA256` |
| `--iterations <N>` | positive integer | `100000` |
| `--hex` | *(encrypt / decrypt only)* switch to hex encoding | off |

## Size header options

| Option | Values | Default |
|--------|--------|---------|
| `--header <on\|off>` | `on`, `off`, `true`, `false`, `1`, `0` | `on` |
| `--payload-size <N>` | bytes | required if `--header off` on extract |

When `--header on`, the embed prefixes the payload with a 4‑byte big‑endian
length and the extract reads it automatically. With `--header off` the size
must be supplied via `--payload-size`.

## Steganography options

| Option | Values | Default |
|--------|--------|---------|
| `--bits` | `1..4` | `1` |
| `--seed` | `0` = sequential | `0` |
| `--channels` | e.g. `RGB`, `RGBA` | `RGB` |
| `--mode` | `uniform`, `smart` | `uniform` |
| `--window` | `3,5,7,9,11,13` | `5` |
| `--metric` | `luminance`, `per-channel`, `sum` | `luminance` |
| `--threshold` | number | `0.0` |
| `--shuffle` | `on`, `off` | `off` |

## Image formats

- **PNG** — native (libpng). Detected on load via magic bytes; written when the
  output extension is `.png` (or anything other than `.ppm` / `.pgm` / `.pnm`).
- **PPM (P6)** and **PGM (P5)** — native, built in. Maxval must be 255.
- Anything else is rejected on load. Convert with ImageMagick if needed:

  ```bash
  convert photo.jpg photo.png
  ```

## Examples

### Hash and CRC

```bash
$ rfp-cli crc "123456789"
3421780262

$ rfp-cli hash "abc"
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
```

### Pure encryption (no image)

```bash
$ rfp-cli encrypt "Top secret" --password hunter2
RFP1...base64...

$ rfp-cli encrypt "Top secret" --password hunter2 --hex
5246503101...

$ CIPHER=$(rfp-cli encrypt "Top secret" --password hunter2)
$ rfp-cli decrypt "$CIPHER" --password hunter2
Top secret

# Recommended: password from stdin
$ printf 'hunter2\n' | rfp-cli encrypt "Top secret" --password-stdin
$ printf 'hunter2\n' | rfp-cli decrypt "$CIPHER" --password-stdin

# Or via env
$ export RFP_PASSWORD=hunter2
$ rfp-cli encrypt "Top secret"
```

### End‑to‑end with an image

```bash
# Embed (header on, encryption on)
$ printf 'hunter2\n' | rfp-cli embed cover.png stego.png \
      --text "Top secret" --password-stdin
Embedded 76 bytes (encrypted) into stego.png

# Extract (auto‑detects size, decrypts)
$ printf 'hunter2\n' | rfp-cli extract stego.png --password-stdin
Top secret
```

### Header off — manual size

```bash
$ rfp-cli embed cover.png raw.png --text "hello" --header off
Embedded 5 bytes (no header) — use --payload-size 5 with --header off on extract

$ rfp-cli extract raw.png --header off --payload-size 5
hello
```

With encryption the payload‑size must be the size of the whole `RFP1` blob.
The embed prints that number on stderr:

```bash
$ rfp-cli embed cover.png raw_enc.png \
      --text "secret" --password pw --header off
Embedded 70 bytes (encrypted) (no header) — use --payload-size 66 with --header off on extract
```

### Smart mode

```bash
$ rfp-cli embed cover.png smart.png --text "text" --password pw \
      --mode smart --window 5 --metric luminance --threshold 40

$ rfp-cli extract smart.png --password pw \
      --mode smart --window 5 --metric luminance --threshold 40
```

### Self‑test

```bash
$ rfp-cli self-test \
    --mode smart --threshold 50 --window 5 \
    --metric luminance --shuffle on \
    --bits 2 --seed 12345

Self-test passed
```

### Backend info

```bash
$ rfp-cli crypto-info
Backend: OpenSSL 3.0.13 ...

Ciphers:
  AES-128-GCM  (key=16B, iv=12B, tag=16B)
  AES-256-GCM  (key=32B, iv=12B, tag=16B)
  ChaCha20-Poly1305  (key=32B, iv=12B, tag=16B)
  AES-256-CBC  (key=32B, iv=16B, tag=0B)
  AES-256-CTR  (key=32B, iv=16B, tag=0B)

KDFs:
  PBKDF2-HMAC-SHA256
  PBKDF2-HMAC-SHA512
  scrypt
  argon2id

Hashes:
  SHA-256  (32B)
  SHA-512  (64B)
  SHA3-256  (32B)
  SHA3-512  (64B)
  BLAKE2b  (64B)
```

## Compatibility with the GUI

The CLI and the GUI share the same stego engine and the same `RFP1` payload
format. A file produced by one can be read by the other, provided the
steganography parameters and (if applicable) the password are the same. The
**Copy params** button in the GUI produces a string that the CLI parameters
above can reproduce.