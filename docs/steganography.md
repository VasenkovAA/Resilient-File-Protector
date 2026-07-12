# Steganography notes

The current algorithm uses LSB replacement in selected image channels.

For every payload bit, the algorithm selects one low bit in one raster channel and replaces that bit with the payload bit.

## Slot selection modes

Two modes are available:

### Uniform (default)

- All available slots (pixels × enabled channels × bitsPerChannel) are taken in a fixed order:
  1. Pixels: row-major (left to right, top to bottom)
  2. Channels: R, G, B, A (only enabled)
  3. Bits: from LSB (bit 0) to `bitsPerChannel-1`
- If `seed != 0`, the entire list is shuffled with `std::mt19937(seed)`.
- This mode is fully backward-compatible with older versions.

### Smart (dispersion-based)

This mode attempts to hide data in areas with high local texture, making changes less noticeable.

The algorithm:

1. For each pixel and channel, compute **stable dispersion**:
   - Take the pixel value in that channel and **zero out the lower `bitsPerChannel` bits** (e.g., for `bitsPerChannel=2`, set `pixel = (pixel >> 2) << 2`).
   - This ensures that embedding does not alter the computed dispersion, so extraction can reproduce the same slot ordering.
   - Dispersion is calculated over a square window (size 3,5,7,9,11,13) centered on the pixel, using one of three metrics:
     - **Luminance** – convert RGB to Y = 0.299R + 0.587G + 0.114B, compute variance of Y in the window.
     - **Per‑channel** – for each channel separately, compute variance of that channel's values in the window.
     - **Sum** – sum of per‑channel variances (over all enabled channels) for each pixel.

2. **Filter** – only slots whose dispersion ≥ `threshold` are kept.

3. **Sort** – remaining slots are sorted in descending order of dispersion (highest dispersion first).

4. **Shuffle** – if `seed != 0` and `applyShuffleAfterSort` is enabled, the sorted list is shuffled using `std::mt19937(seed)`.

5. **Take first `requiredBits`** slots.

## Parameters for extraction

For successful extraction, the user must remember and reuse **all** parameters that affect slot order:

- Mode (`Uniform` or `Smart`)
- `bitsPerChannel`
- Enabled channels (R/G/B/A)
- `seed`
- Payload size (bytes) – needed to know how many bits to read
- **If Smart mode was used:**
  - `windowSize`
  - `dispersionMetric`
  - `dispersionThreshold`
  - `applyShuffleAfterSort` (on/off)

The GUI displays all these parameters in the status bar after embedding, making it easy to record them.

## Why PNG first?

PNG is lossless, so low-level pixel changes can survive saving/loading. Lossy formats such as JPEG are not suitable for this simple LSB method because recompression may destroy hidden bits.
