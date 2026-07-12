
# R.F.P. – Resilient File Protector

**R.F.P.** is a desktop application for hiding private data inside ordinary image files (PNG, BMP, TIFF).  
It uses **LSB steganography** – the least significant bits of pixel colour channels are replaced with payload bits.

## Quick start

1. **Load an image** – click *Browse…* next to *Input image*.
2. **Set parameters** – by default, 1 bit per channel, RGB channels enabled, uniform mode.
3. **Enter text** in the payload area.
4. **Click *Embed text*** – choose an output image path.
5. **To extract**, load the stego image, set the same parameters (including payload size), and click *Extract text*.

## Important

- The image is saved as a normal raster file – **no metadata** (EXIF, PNG chunks, etc.) are modified.
- Extraction requires **exactly the same parameters** that were used during embedding.
- After embedding, the GUI displays a summary of parameters and the CRC32 checksum – record these for later extraction.