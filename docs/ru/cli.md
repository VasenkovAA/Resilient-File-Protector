# Командная строка

Утилита `rfp-cli` собирается вместе с GUI.

## Команды

- `--help` – показать справку.
- `crc <текст>` – вычислить CRC32 для заданного текста.
- `self-test [параметры]` – выполнить тест «туда-обратно» на синтетическом изображении.

### Параметры self-test

| Параметр | Значения | По умолчанию |
|----------|----------|--------------|
| `--mode` | `uniform`, `smart` | `uniform` |
| `--window` | `3,5,7,9,11,13` | `3` |
| `--metric` | `luminance`, `per-channel`, `sum` | `luminance` |
| `--threshold` | число | `0.0` |
| `--shuffle` | `on`, `off` | `on` |
| `--bits` | `1-4` | `1` |
| `--seed` | число | `0` |

Пример:
```bash
./rfp-cli self-test --mode smart --threshold 50 --window 5 --metric luminance --shuffle on
```

При успешном прохождении выводится `Self-test passed`.
