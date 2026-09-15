# Командная строка

Утилита `rfp-cli` собирается вместе с GUI и использует те же библиотеки
`rfp_core`, `rfp_stego`, `rfp_crypto`, `rfp_payload`, но **без зависимости от
Qt**: для работы с изображениями применяются libpng и встроенный парсер
PPM/PGM.

Путь к бинарю: `build/bin/rfp-cli`.

## Команды

| Команда | Назначение |
|---------|------------|
| `help`, `--help`, `-h` | Показать справку. |
| `--version`, `-v` | Версия. |
| `crc <текст>` | CRC32 от строки UTF-8. |
| `hash <текст> [--algo <имя>]` | Хеш текста. `SHA-256` (по умолчанию), `SHA-512`, `SHA3-256`, `SHA3-512`, `BLAKE2b`. |
| `encrypt <текст> [pw-опции] [cipher-опции] [--hex]` | Создать `RFP1`-контейнер (Base64, либо hex с `--hex`). |
| `decrypt <строка> [pw-опции] [--hex]` | Расшифровать `RFP1`-контейнер. |
| `embed <вход> <выход> --text <текст> [pw-опции] [--header on\|off] [stego-опции]` | Встроить текст в PNG или PPM/PGM. |
| `extract <вход> [pw-опции] [--header on\|off] [--payload-size N] [stego-опции]` | Извлечь текст из PNG или PPM/PGM. |
| `self-test [stego-опции]` | Тест «туда-обратно» на синтетическом изображении. |
| `crypto-info` | Версия OpenSSL и доступные алгоритмы. |

## Опции пароля

Используются в `encrypt`, `decrypt`, `embed`, `extract`:

| Опция | Действие |
|-------|----------|
| `--password <pw>` | Пароль в командной строке (виден в `ps(1)` — не рекомендуется). |
| `--password-stdin` | Читать пароль со stdin, отбрасывая завершающий `\n` или `\r\n`. Рекомендуется. |
| *(переменная `RFP_PASSWORD`)* | Запасной вариант, если ни одна из опций не задана. |

Порядок поиска: `--password-stdin` > `--password` > `RFP_PASSWORD`.

## Опции шифрования

| Опция | Значения | По умолчанию |
|-------|----------|--------------|
| `--cipher <имя>` | `AES-128-GCM`, `AES-256-GCM`, `ChaCha20-Poly1305`, `AES-256-CBC`, `AES-256-CTR` | `AES-256-GCM` |
| `--kdf <имя>` | `PBKDF2-HMAC-SHA256`, `PBKDF2-HMAC-SHA512`, `scrypt` | `PBKDF2-HMAC-SHA256` |
| `--iterations <N>` | положительное целое | `100000` |
| `--hex` | *(только encrypt / decrypt)* | выкл. |

## Опции заголовка размера

| Опция | Значения | По умолчанию |
|-------|----------|--------------|
| `--header <on\|off>` | `on`, `off`, `true`, `false`, `1`, `0` | `on` |
| `--payload-size <N>` | байты | обязательно при `--header off` в extract |

При `--header on` перед данными пишется 4-байтная big-endian длина; при
извлечении она читается автоматически. При `--header off` размер задаётся
пользователем через `--payload-size`.

## Опции стеганографии

| Опция | Значения | По умолчанию |
|-------|----------|--------------|
| `--bits` | `1..4` | `1` |
| `--seed` | `0` = последовательно | `0` |
| `--channels` | напр. `RGB`, `RGBA` | `RGB` |
| `--mode` | `uniform`, `smart` | `uniform` |
| `--window` | `3,5,7,9,11,13` | `5` |
| `--metric` | `luminance`, `per-channel`, `sum` | `luminance` |
| `--threshold` | число | `0.0` |
| `--shuffle` | `on`, `off` | `off` |

## Форматы изображений

- **PNG** — нативно (libpng). Формат определяется по magic bytes при загрузке; запись в PNG происходит, если расширение выходного файла — `.png` (или любое, кроме `.ppm` / `.pgm` / `.pnm`).
- **PPM (P6)** и **PGM (P5)** — нативно, встроенный кодек. Maxval должен быть 255.
- Любые другие форматы отклоняются. Конвертируйте через ImageMagick:

  ```bash
  convert photo.jpg photo.png
  ```

## Примеры

### Хеш и CRC

```bash
$ rfp-cli crc "123456789"
3421780262

$ rfp-cli hash "abc"
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
```

### Шифрование без изображения

```bash
$ rfp-cli encrypt "Top secret" --password hunter2
RFP1...base64...

$ rfp-cli encrypt "Top secret" --password hunter2 --hex
5246503101...

$ CIPHER=$(rfp-cli encrypt "Top secret" --password hunter2)
$ rfp-cli decrypt "$CIPHER" --password hunter2
Top secret

# Рекомендуется: пароль через stdin
$ printf 'hunter2\n' | rfp-cli encrypt "Top secret" --password-stdin
$ printf 'hunter2\n' | rfp-cli decrypt "$CIPHER" --password-stdin

# Или через переменную окружения
$ export RFP_PASSWORD=hunter2
$ rfp-cli encrypt "Top secret"
```

### Полный цикл с изображением

```bash
# Встраивание (заголовок on, шифрование on)
$ printf 'hunter2\n' | rfp-cli embed cover.png stego.png \
      --text "Top secret" --password-stdin
Embedded 76 bytes (encrypted) into stego.png

# Извлечение (размер определяется автоматически, данные расшифровываются)
$ printf 'hunter2\n' | rfp-cli extract stego.png --password-stdin
Top secret
```

### Без заголовка — размер вручную

```bash
$ rfp-cli embed cover.png raw.png --text "hello" --header off
Embedded 5 bytes (no header) — use --payload-size 5 with --header off on extract

$ rfp-cli extract raw.png --header off --payload-size 5
hello
```

С шифрованием размер — это длина всего `RFP1`-блоба. Embed выводит это число в stderr:

```bash
$ rfp-cli embed cover.png raw_enc.png \
      --text "secret" --password pw --header off
Embedded 70 bytes (encrypted) (no header) — use --payload-size 66 with --header off on extract
```

### Умный режим

```bash
$ rfp-cli embed cover.png smart.png --text "text" --password pw \
      --mode smart --window 5 --metric luminance --threshold 40

$ rfp-cli extract smart.png --password pw \
      --mode smart --window 5 --metric luminance --threshold 40
```

### Самотестирование

```bash
$ rfp-cli self-test \
    --mode smart --threshold 50 --window 5 \
    --metric luminance --shuffle on \
    --bits 2 --seed 12345

Self-test passed
```

### Информация о бэкенде

```bash
$ rfp-cli crypto-info
Backend: OpenSSL 3.0.13 ...
```
