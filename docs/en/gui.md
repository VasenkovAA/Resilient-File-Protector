# Using the GUI

## Main window layout

- **Left panel**: image selection, payload, encryption panel, steganography parameters, actions.
- **Right panel**: image preview with different display modes.

The window is organised as two tabs — **Embed** and **Extract** — each with its own parameter set.

## Step‑by‑step guide

### Embedding

1. **Input image** – select a PNG/BMP/TIFF/PPM file.
2. **Output image** – choose where to save the stego image.
3. **Text payload** – type the text to hide.
4. **Encryption** (optional) – expand the panel, tick the checkbox, enter a
   password twice, pick a cipher and KDF. Defaults (`AES-256-GCM`,
   `PBKDF2-HMAC-SHA256`, 100 000 iterations) are safe.
5. **Adjust steganography parameters** (or keep defaults).
6. Click **Embed**.
7. The status bar shows the embedded size and CRC32. The **Usage** line above
   the parameters shows how much capacity the payload will consume, including
   the encryption overhead.

### Extraction

1. **Input image** – load the stego image.
2. **Set exactly the same steganography parameters** used during embedding
   (bits, channels, seed, mode, smart params).
3. **Payload size** — auto-detected if the embed wrote a size header. If not,
   enter the original payload length manually.
4. **Decryption** (optional, if the payload was encrypted) – expand the panel,
   tick the checkbox, enter the password.
5. Click **Extract** – the text appears in the text box.

### Copy / Paste parameters

Both tabs have **Copy params** / **Paste params** buttons. The exported string
contains every steganography parameter, the size‑header mode and the cipher/KDF
settings — but **never the password**. Use this to transfer a configuration from
the Embed tab to the Extract tab (or between runs) without transcription errors.

## Encryption panel

**Encryption (Embed tab)**:

- Checkbox to enable encryption.
- Password + Confirm fields with a reveal (👁) toggle.
- **Cipher** — AES‑128‑GCM, AES‑256‑GCM, ChaCha20‑Poly1305, AES‑256‑CBC, AES‑256‑CTR.
- **KDF** — PBKDF2‑HMAC‑SHA256/512, scrypt.
- **KDF iterations** — 1000 to 100 000 000; default 100 000.
- Short hint about password handling.

**Decryption (Extract tab)**:

- Checkbox and password field only. Cipher/KDF/iterations come from the payload.

Passwords are **never written to `QSettings`** and are cleared from the
confirmation field after each successful embed.

## Preview modes

- **Original** – shows the loaded image.
- **Dispersion overlay** – visualises dispersion values (only in Smart mode).
- **Comparison** – shows original and modified side‑by‑side, highlights changed pixels.

The **Auto** button suggests a threshold (70th percentile of dispersions) for the loaded image.

## Settings

Open **Settings** from the toolbar:

- **Theme** (Light / Dark).
- **Language** (English / Русский) – requires a restart.
- **Overlay opacity** for the dispersion preview.
- **Show preview** toggle and **Preview mode** selector.
- **Highlight changes**.
- **Write payload size header (4 bytes)** — see below.

### Size header

When enabled, the embed prefixes the payload with a 4‑byte big‑endian length
field. The Extract tab mirrors this setting in a read‑only checkbox; if the
header is absent, the *Payload size (bytes)* field becomes editable and you
must supply the length manually. This setting is included in *Copy params*.
