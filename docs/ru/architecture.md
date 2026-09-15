# Архитектура

R.F.P. разделён на независимые модули:

- **rfp_core** – общие утилиты: байтовые буферы, CRC32, типы ошибок и результатов.
- **rfp_stego** – LSB-стеганография, расчёт ёмкости, генерация слотов, «умный» отбор на основе дисперсии.
- **rfp_crypto** – обёртка над OpenSSL 3.x: шифры, KDF, хеши, CSPRNG, безопасная память. Без Qt.
- **rfp_payload** – обёртка зашифрованной нагрузки (формат `RFP1`). Зависит от `rfp_crypto` и `rfp_core`. Самодостаточна: шифр/KDF/число итераций/соль/IV хранятся вместе с шифротекстом.
- **rfp_gui** – приложение на Qt 6: загрузка/сохранение изображений, интерфейс для всех параметров, панель шифрования на обеих вкладках.
- **rfp_cli** – консольная утилита для скриптов и автоматизации; поддерживает PNG и PPM/PGM нативно.

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

Стеганография и криптография не зависят от Qt и работают с сырыми буферами (`ImageBuffer`, `ByteBuffer`). Qt-специфичная загрузка/сохранение изображений реализована только в `src/gui/QtImageAdapter.*`. CLI использует libpng и встроенный кодек PPM/PGM — Qt не тянет.

## Внутреннее устройство стеганографии

Движок строит список *слотов* – каждый слот это пара `(индекс_байта, номер_бита)`, в которую можно записать один бит.

- **Режим Uniform** – слоты перечисляются последовательно (по пикселям, затем каналам, затем битам), при желании перемешиваются с помощью seed.
- **Режим Smart** – слоты фильтруются и сортируются по локальной дисперсии (вариативности в окрестности). Используются только слоты с дисперсией ≥ порога.

Дисперсия вычисляется по **стабильным** значениям пикселей (младшие биты обнулены), чтобы внедрение не меняло порядок.

## Формат нагрузки

Движок стеганографии работает с полезной нагрузкой как с сырыми байтами. Вызывающая сторона может обернуть её двумя необязательными слоями:

```
[ 4-байтный заголовок размера ]   ← GUI / CLI, необязательно
[ RFP1-контейнер | открытый текст ] ← rfp_payload, необязательно
```

Формат `RFP1` самодостаточен:

| Смещение | Размер | Поле |
|----------|--------|------|
| 0 | 4 | магия `RFP1` |
| 4 | 1 | версия (сейчас 1) |
| 5 | 1 | идентификатор шифра |
| 6 | 1 | идентификатор KDF |
| 7 | 1 | флаги |
| 8 | 4 | число итераций KDF (big-endian) |
| 12 | 1 | длина соли |
| 13 | 1 | длина IV |
| 14 | 2 | зарезервировано |
| 16 | N | соль |
| 16+N | M | IV |
| ... | K | шифротекст |
| ... | T | тег аутентификации (только AEAD) |

При AES-256-GCM + PBKDF2-SHA256 с 100 000 итераций (соль 16 Б, IV 12 Б, тег 16 Б) накладные расходы составляют **60 байт**.

## Отсутствие метаданных в изображении

Приложение не записывает EXIF-поля, текстовые блоки PNG или пользовательские заголовки. Извлечение зависит от:

- параметров стеганографии (запоминаются пользователем или переносятся через *Копировать/Вставить параметры*),
- того, был ли записан заголовок размера,
- при наличии шифрования — пароля.

Конфигурация шифрования хранится внутри `RFP1`-блоба; само изображение на уровне метаданных остаётся обычным растровым файлом.

## CRC32

GUI показывает CRC32 исходных данных до встраивания и CRC32 извлечённых после. Это удобная проверка для пользователя, а не криптографическая гарантия целостности — эта роль принадлежит AEAD-тегу внутри `RFP1`-блоба, если шифрование включено.



### Добавить новый контекст `CryptoPanel`

```xml
<context>
    <name>CryptoPanel</name>
    <message>
        <source>Encryption</source>
        <translation>Шифрование</translation>
    </message>
    <message>
        <source>Decryption</source>
        <translation>Расшифровка</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>Password:</source>
        <translation>Пароль:</translation>
    </message>
    <message>
        <source>Repeat password</source>
        <translation>Повторите пароль</translation>
    </message>
    <message>
        <source>Confirm:</source>
        <translation>Подтверждение:</translation>
    </message>
    <message>
        <source>Show / hide password</source>
        <translation>Показать / скрыть пароль</translation>
    </message>
    <message>
        <source>Cipher:</source>
        <translation>Шифр:</translation>
    </message>
    <message>
        <source>KDF:</source>
        <translation>KDF:</translation>
    </message>
    <message>
        <source>KDF iterations:</source>
        <translation>Итераций KDF:</translation>
    </message>
    <message>
        <source>Password is never written to disk or settings. Cipher, KDF and iterations are stored inside the encrypted payload.</source>
        <translation>Пароль никогда не записывается на диск или в настройки. Шифр, KDF и число итераций хранятся внутри зашифрованного контейнера.</translation>
    </message>
    <message>
        <source>Cipher, KDF and iterations are read from the payload — only the password is required.</source>
        <translation>Шифр, KDF и число итераций читаются из контейнера — требуется только пароль.</translation>
    </message>
</context>
```

После правки `.ts` пересоберите `.qm`:

```bash
lrelease src/gui/translations/rfp-gui_ru.ts -qm src/gui/translations/rfp-gui_ru.qm
```

или через Qt Creator: *File → Open* → выбрать `.ts` → *Release*.

---

## Что я НЕ трогал

- `docs/DeveloperGuide.md` — он описывает сборочные процессы, не функционал; всё, что там есть, осталось верным.
- `docs.qrc` — список файлов не изменился.
- `LICENSE`, `.gitignore`, `.gitmodules` — без изменений.
- Разделы `docs/ru/*` не-затронутых тем (например, базовая часть `steganography.md`) — оставил как было, только добавил новые подразделы.

## Быстрая проверка после применения

```bash
# Все markdown-файлы на месте и не пустые
for f in docs/en/*.md docs/ru/*.md README.md; do
    printf "%-40s %s\n" "$f" "$(wc -l < "$f") lines"
done

# Сборка .qm
lrelease src/gui/translations/rfp-gui_ru.ts \
         -qm  src/gui/translations/rfp-gui_ru.qm
```