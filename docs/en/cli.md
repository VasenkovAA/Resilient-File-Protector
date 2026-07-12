# Command‑line interface

The CLI tool `rfp-cli` is built alongside the GUI.

## Commands

- `--help` – show usage.
- `crc <text>` – compute CRC32 of the given text.
- `self-test [options]` – perform a round‑trip test on a synthetic image.

### Self‑test options

| Option | Values | Default |
|--------|--------|---------|
| `--mode` | `uniform`, `smart` | `uniform` |
| `--window` | `3,5,7,9,11,13` | `3` |
| `--metric` | `luminance`, `per-channel`, `sum` | `luminance` |
| `--threshold` | number | `0.0` |
| `--shuffle` | `on`, `off` | `on` |
| `--bits` | `1-4` | `1` |
| `--seed` | number | `0` |

Example:
```bash
./rfp-cli self-test --mode smart --threshold 50 --window 5 --metric luminance --shuffle on
