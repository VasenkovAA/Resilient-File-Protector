# Architecture

R.F.P. is split into independent modules.

```text
Qt GUI  ───────┐
CLI     ───────┼── rfp_stego ──┐
future API ────┘               │
                               ├── rfp_core
Qt GUI  ───────┐               │
CLI     ───────┼── rfp_payload ┤
future API ────┘        │      │
                        │      │
                     rfp_crypto ┘
                        │
                     libcrypto (OpenSSL 3.x)
```

The steganography and cryptography libraries do not depend on Qt. They operate
on raw byte buffers (`ImageBuffer`, `ByteBuffer`). Qt‑specific image
loading/saving is implemented only in `src/gui/QtImageAdapter.*`. The CLI uses
libpng + a built‑in PPM/PGM codec, so it does not pull in Qt either.

## Modules

- **rfp_core** – common utilities: byte buffers, CRC32, error/result types.
- **rfp_stego** – LSB steganography, capacity calculation, slot generation, dispersion-based smart selection.
- **rfp_crypto** – OpenSSL 3.x wrapper: ciphers, KDFs, hashes, CSPRNG, secure memory. No Qt.
- **rfp_payload** – encrypted payload wrapper (`RFP1` format). Depends on `rfp_crypto` and `rfp_core`. Self‑describing: cipher/KDF/iterations/salt/IV travel with the ciphertext.
- **rfp_gui** – Qt 6 application, loads/saves images, provides UI for all parameters, encryption panel on both tabs.
- **rfp_cli** – command-line tool for scripting and automation; supports PNG and PPM/PGM natively.

## Stego internals

The steganography engine builds a list of *slots* – each slot is a `(byteIndex, bitIndex)` pair that can hold one payload bit.
Slot ordering depends on the selected mode:

- **Uniform** – all pixels, channels and LSB positions are enumerated sequentially, then optionally shuffled with `seed`.
- **Smart** – slots are filtered and sorted by local dispersion. See `steganography.md` for details.

The `StegoSlots` module orchestrates slot generation, while `StegoDispersion` computes stable dispersion using integral images for speed.

## Payload format

The stego engine treats the payload as raw bytes. Optionally, the caller can
wrap it before embedding:

```
[ 4-byte size header ]   ← rfp_gui / rfp_cli, optional
[ RFP1 blob | plaintext ] ← rfp_payload, optional
```

The `RFP1` blob is self‑describing:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | magic `RFP1` |
| 4 | 1 | version (currently 1) |
| 5 | 1 | cipher id |
| 6 | 1 | KDF id |
| 7 | 1 | flags |
| 8 | 4 | KDF iterations (big‑endian) |
| 12 | 1 | salt length |
| 13 | 1 | IV length |
| 14 | 2 | reserved |
| 16 | N | salt |
| 16+N | M | IV |
| ... | K | ciphertext |
| ... | T | authentication tag (AEAD only) |

With AES‑256‑GCM + PBKDF2‑SHA256 at 100 000 iterations (salt 16 B, IV 12 B,
tag 16 B), the overhead is **60 bytes**.

## Why no image metadata?

The project does not write EXIF fields, PNG text chunks or custom visible
format headers. Extraction depends on:

- the steganography parameters (remembered by the user, or transferred via
  Copy/Paste params),
- whether a size header was embedded,
- if applicable, the password.

The `RFP1` blob carries the encryption configuration; the image itself stays
byte‑for‑byte a normal raster file at the metadata level.

## CRC32

The GUI displays CRC32 of the payload before embed and after extract. It is a
convenience check for the user, not a cryptographic integrity guarantee — that
role belongs to the AEAD tag inside the `RFP1` blob when encryption is enabled.
```

---

## 8. `docs/en/faq.md`

Файла ещё нет в `docs/en/`, но он объявлен в `docs.qrc` — создаём.

```markdown
# Frequently asked questions

**Q: Extraction fails — what should I check?**
A: Make sure **all** steganography parameters match what was used for embedding
(bits, channels, seed, mode, window, metric, threshold, shuffle) **and** that
the size‑header mode agrees. If the payload is encrypted, use the same password.
The `RFP1` blob carries cipher/KDF/iterations, so those do not need to be
specified on extraction.

**Q: Can I use JPEG?**
A: No. JPEG is lossy and recompression destroys the LSB changes. Use PNG (lossless).

**Q: My payload is too large — what now?**
A: Increase bits per channel, enable more colour channels, or use a larger
image. With encryption enabled, remember that the cipher adds ~60 bytes of
overhead.

**Q: Is the data encrypted?**
A: Only if you enable encryption. On the Embed tab, tick *Encrypt payload
before embedding* and enter a password; the same on the Extract tab (or pass
`--password` / `--password-stdin` to the CLI). Defaults are AES‑256‑GCM +
PBKDF2‑HMAC‑SHA256 at 100 000 iterations. **Without encryption**, the payload
is plain bytes hidden in the image — anyone who knows the steganography
parameters can read it.

**Q: Where is the password stored?**
A: Nowhere. It is held only in memory for the duration of the operation and is
never written to `QSettings`, `Copy params` output or the `RFP1` blob. **There
is no recovery if you lose it.**

**Q: Why does the size header matter?**
A: When enabled, the 4‑byte big‑endian length prefix lets extraction read the
correct number of bytes automatically. When disabled, you must supply the
payload size manually. The mode must match between embed and extract — the GUI
shows it in the Extract tab, and Copy/Paste params carries it along.

**Q: How do I verify the embed?**
A: The GUI shows CRC32 of the original payload after embed and CRC32 of the
extracted payload after extract. They should match. For encrypted payloads,
the AEAD tag will also fail to verify if the ciphertext or header was tampered
with.

**Q: Can I use the CLI for real file operations?**
A: Yes. The CLI supports PNG and binary PPM/PGM natively and can perform the
full embed/extract cycle with optional encryption, size header, smart mode and
shuffling:

```bash
printf 'pw\n' | rfp-cli embed cover.png stego.png --text "hello" --password-stdin
printf 'pw\n' | rfp-cli extract stego.png --password-stdin
```

**Q: The CLI complains about an "Invalid payload size in header".**
A: The stego parameters or the header mode do not match between embed and
extract. Try `--header off` on both ends (with an explicit `--payload-size`),
or reset the stego options to the defaults used at embed time.

**Q: Can I read a GUI‑produced file with the CLI, or vice versa?**
A: Yes. Both share the same stego engine and the same `RFP1` payload format.
Use **Copy params** in the GUI to get a string that reproduces the CLI
parameters exactly.
