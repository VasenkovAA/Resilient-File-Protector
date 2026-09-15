# Security & Stealth Recommendations

To achieve the best possible concealment and resist statistical detection, follow these guidelines when choosing steganography parameters.

## Choose the right image

- **Prefer photographs** with natural textures, gradients, and noise (e.g., outdoor scenes, landscapes).
  Avoid flat-coloured areas, computer‑generated graphics, or images with large uniform regions – they make LSB changes more visible.
- **Use images with at least 24‑bit colour** (RGB, 8 bits per channel).
  Higher colour depth provides more room for hiding without noticeable distortion.
- **Avoid lossy formats** – always use lossless PNG (or BMP/TIFF). JPEG recompression destroys embedded data.

## Encrypt the payload

LSB steganography hides the fact that data exists, but on its own it does **not**
protect the data if the image is suspected. **Always enable encryption**:

- Tick **Encrypt payload before embedding** in the GUI, or pass `--password` /
  `--password-stdin` to `rfp-cli embed`.
- The recommended defaults — **AES‑256‑GCM** + **PBKDF2‑HMAC‑SHA256** with
  100 000 iterations — are safe for text payloads of moderate size.
- For higher resistance against GPU/ASIC cracking, raise `iterations` or switch
  to **scrypt** (uses memory and is more expensive to parallelise).
- **Prefer AEAD ciphers** (AES‑GCM, ChaCha20‑Poly1305). They authenticate the
  ciphertext and detect tampering. CBC/CTR are available for compatibility only.

Passwords:

- Use a strong, unique password. Treat it like a vault master key — there is
  **no recovery**.
- Do not reuse the same password across unrelated images.
- In the CLI, prefer `--password-stdin` over `--password`; inline passwords are
  visible in `ps(1)`.

## Parameter selection

### 1. Bits per channel
- **Use 1 or 2 bits** for minimal visual distortion.
  With 1 bit, changes are virtually imperceptible. 2 bits may be acceptable on noisy images.
  **Avoid 3–4 bits** unless you need high capacity and can accept visible artefacts.

### 2. Channels
- **Enable only Red, Green, and Blue** channels.
  The Alpha channel is often unused or carries transparent information – modifying it can cause visual glitches when the image is viewed over different backgrounds.
- If you must use Alpha, ensure the image actually contains transparency and that the changes won't be noticeable.

### 3. Seed (shuffling)
- **Always use a non‑zero random seed** (e.g., 12345).
  Shuffling prevents predictable bit ordering, making it harder for an attacker to extract data without knowing the seed.
- The seed itself should be kept secret – treat it like a password.

### 4. Mode: Smart vs Uniform
- **Prefer Smart mode** for better visual concealment.
  Smart mode hides data in high‑texture areas where human vision is less sensitive to changes.
- Use the **Auto** button to suggest a threshold (70th percentile) – this typically gives a good balance between capacity and invisibility.

### 5. Smart mode fine‑tuning
- **Window size**: larger window (e.g., 7 or 9) smooths dispersion and may better identify truly textured regions.
- **Metric**: **Luminance** works well for natural images, as it corresponds to human brightness perception. **Per‑channel** or **Sum** can be better if you expect statistical analysis of individual channels.

### 6. Payload size
- **Do not exceed 30–50% of the available capacity**.
  Overfilling leaves less room for error and may create detectable patterns.
- If you need to hide large files, split them across multiple images.

## Size header

The optional 4‑byte size header is a convenience for the receiver, not a
security measure. It reveals the payload length in plain (though it is itself
embedded via LSBs). If you are worried about length‑based analysis, disable it
and transmit the payload size out of band.

## Additional precautions

- **Never reuse the same parameters** for different images. Vary the seed, threshold, and window size to avoid creating a fingerprint.
- **Keep the parameters and password secret** – they are your decryption key.
- **After embedding, save the parameters** (displayed in the GUI) in a secure place, along with the original image hash (CRC32) for verification.

## Detection resistance

Modern steganalysis tools look for:
- Unusual correlations in LSB planes
- Distortions in colour histograms
- Regular patterns caused by sequential embedding

By using Smart mode + shuffling + encryption + appropriate capacity, you
significantly reduce these risks.
