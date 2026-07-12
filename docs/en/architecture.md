# Architecture

R.F.P. is split into independent modules.

```text
Qt GUI  ───────┐
CLI     ───────┼── rfp_stego ── rfp_core
future API ────┘

future encryption module: rfp_crypto
```

The steganography library does not depend on Qt. It operates on a raw `ImageBuffer` with width, height, channel count and bytes.
Qt-specific image loading/saving is implemented only in `src/gui/QtImageAdapter.*`.

## Modules

- **rfp_core** – common utilities: byte buffers, CRC32, error/result types.
- **rfp_stego** – LSB steganography, capacity calculation, slot generation, dispersion-based smart selection.
- **rfp_crypto** – reserved for future encryption (stage 3).
- **rfp_gui** – Qt 6 application, loads/saves images, provides UI for all parameters.
- **rfp_cli** – command-line tool for testing and automation.

## Stego internals

The steganography engine builds a list of *slots* – each slot is a `(byteIndex, bitIndex)` pair that can hold one payload bit.
Slot ordering depends on the selected mode:

- **Uniform** – all pixels, channels and LSB positions are enumerated sequentially, then optionally shuffled with `seed`.
- **Smart** – slots are filtered and sorted by local dispersion. See `steganography.md` for details.

The `StegoSlots` module orchestrates slot generation, while `StegoDispersion` computes stable dispersion using integral images for speed.

## Why no image metadata?

The first project stage does not write EXIF fields, PNG text chunks or custom visible format headers.
Extraction depends on external parameters remembered by the user.

## Current payload format

At the current stage, the hidden payload is raw bytes only.
There is no embedded magic value, no embedded size and no embedded CRC field.

The GUI displays CRC32 after embedding and after extraction. The user can compare these values manually.
