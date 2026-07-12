
# Using the GUI

## Main window layout

- **Left panel**: image selection, steganography parameters, text payload, actions, preview controls.
- **Right panel**: image preview with different display modes.

## Step‑by‑step guide

### Embedding

1. **Input image** – select a PNG/BMP/TIFF file.
2. **Output image** – choose where to save the stego image.
3. **Adjust parameters** (or keep defaults).
4. **Type text** into the text box.
5. Click **Embed text**.
6. The status bar shows the embedded size and CRC32 – **copy the parameters** for future extraction.

### Extraction

1. **Input image** – load the stego image.
2. **Set exactly the same parameters** used during embedding (bits, channels, seed, mode, smart params).
3. **Payload size** – enter the original payload length (in bytes).
4. Click **Extract text** – the text appears in the text box.

## Preview modes

- **Original** – shows the loaded image.
- **Dispersion overlay** – visualises dispersion values (only in Smart mode).
- **Comparison** – shows original and modified side‑by‑side, highlights changed pixels.

The **Auto** button suggests a threshold (70th percentile of dispersions) for the loaded image.