# Steganography notes

The current algorithm uses LSB replacement in selected image channels.

For every payload bit, the algorithm selects one low bit in one raster channel and replaces that bit with the payload bit.

Default parameters:

- RGB channels only;
- 1 least significant bit per channel;
- sequential placement;
- alpha channel disabled.

If `seed != 0`, the same set of writable bit positions is shuffled with `std::mt19937(seed)` before embedding and extraction.
The same seed must be used during extraction.

## Why PNG first?

PNG is lossless, so low-level pixel changes can survive saving/loading. Lossy formats such as JPEG are not suitable for this simple LSB method because recompression may destroy hidden bits.
