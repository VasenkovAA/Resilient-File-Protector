#include "ImageIO.h"

#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Crc32.h"
#include "rfp/crypto/CryptoRegistry.h"
#include "rfp/crypto/Encoding.h"
#include "rfp/crypto/Hash.h"
#include "rfp/payload/PayloadCrypto.h"
#include "rfp/stego/Capacity.h"
#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoEncoder.h"
#include "rfp/stego/StegoParams.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr const char* kCliVersion = "0.4.0";

// ---------------------------------------------------------------------------
//  Help
// ---------------------------------------------------------------------------
void printHelp() {
    std::cout <<
        "R.F.P. - Resilient File Protector CLI " << kCliVersion << "\n"
                                                                   "\n"
                                                                   "Usage:\n"
                                                                   "  rfp-cli <command> [options]\n"
                                                                   "\n"
                                                                   "Commands:\n"
                                                                   "  help | --help | -h        Show this help.\n"
                                                                   "  --version | -v            Print version.\n"
                                                                   "  crc <text>                CRC32 of text.\n"
                                                                   "  hash <text> [--algo <name>]\n"
                                                                   "                            Hash of text. SHA-256 (default) | SHA-512 |\n"
                                                                   "                            SHA3-256 | SHA3-512 | BLAKE2b.\n"
                                                                   "  encrypt <text> [pw-opts] [--cipher <n>] [--kdf <n>]\n"
                                                                   "                 [--iterations <N>] [--hex]\n"
                                                                   "                            Produce RFP1 payload (Base64 by default).\n"
                                                                   "  decrypt <encoded> [pw-opts] [--hex]\n"
                                                                   "                            Decrypt an RFP1 payload.\n"
                                                                   "  embed <in.(png|ppm)> <out.(png|ppm)> --text <text> [pw-opts]\n"
                                                                   "        [--header on|off] [stego-opts]\n"
                                                                   "                            Embed text into image; encrypts if a\n"
                                                                   "                            password is provided.\n"
                                                                   "  extract <in.(png|ppm)> [pw-opts] [--header on|off]\n"
                                                                   "        [--payload-size N] [stego-opts]\n"
                                                                   "                            Extract text from image; decrypts if a\n"
                                                                   "                            password is provided.\n"
                                                                   "  self-test [stego-opts]    In-memory round-trip smoke test.\n"
                                                                   "  crypto-info               Print backend and available algorithms.\n"
                                                                   "\n"
                                                                   "Password options (encrypt / decrypt / embed / extract):\n"
                                                                   "  --password <pw>           Inline password (visible in ps(1) — avoid).\n"
                                                                   "  --password-stdin          Read password from stdin (recommended).\n"
                                                                   "                            Trailing newline / CR is stripped.\n"
                                                                   "                            Falls back to env RFP_PASSWORD if neither\n"
                                                                   "                            --password nor --password-stdin is set.\n"
                                                                   "\n"
                                                                   "Encryption options:\n"
                                                                   "  --cipher <name>           AES-128-GCM | AES-256-GCM |\n"
                                                                   "                            ChaCha20-Poly1305 | AES-256-CBC |\n"
                                                                   "                            AES-256-CTR   (default: AES-256-GCM).\n"
                                                                   "  --kdf <name>              PBKDF2-HMAC-SHA256 (default) |\n"
                                                                   "                            PBKDF2-HMAC-SHA512 | scrypt.\n"
                                                                   "  --iterations <N>          KDF iterations (default: 100000).\n"
                                                                   "\n"
                                                                   "Steganography options:\n"
                                                                   "  --bits <1-4>              Bits per channel (default: 1).\n"
                                                                   "  --seed <N>                0 = sequential (default: 0).\n"
                                                                   "  --channels <RGB[A]>       e.g. RGB (default) or RGBA.\n"
                                                                   "  --mode uniform|smart      default: uniform.\n"
                                                                   "  --window <3|5|7|9|11|13>  smart-mode window size (default: 5).\n"
                                                                   "  --metric luminance|per-channel|sum\n"
                                                                   "  --threshold <value>       smart-mode threshold.\n"
                                                                   "  --shuffle on|off          default: off.\n"
                                                                   "\n"
                                                                   "Size header (optional 4-byte big-endian length prefix):\n"
                                                                   "  --header on|off           Write (embed) / expect (extract) a header\n"
                                                                   "                            with the exact payload length. Default: on.\n"
                                                                   "                            --header on  + extract: size is auto-read.\n"
                                                                   "                            --header off + extract: --payload-size N\n"
                                                                   "                            is REQUIRED (N = length of the embedded\n"
                                                                   "                            payload; for encrypted data, length of\n"
                                                                   "                            the RFP1 blob).\n"
                                                                   "  --payload-size <N>        Only used with --header off on extract.\n"
                                                                   "\n"
                                                                   "Image formats: PNG and binary PPM/PGM are supported natively.\n"
                                                                   "Format is detected on load and chosen by extension on save.\n"
                                                                   "\n"
                                                                   "Examples:\n"
                                                                   "  # Embed with header, encrypted:\n"
                                                                   "  printf 'pw\\n' | rfp-cli embed cover.png stego.png \\\n"
                                                                   "      --text 'hello' --password-stdin\n"
                                                                   "  printf 'pw\\n' | rfp-cli extract stego.png --password-stdin\n"
                                                                   "\n"
                                                                   "  # Embed without header, encrypted, then extract with explicit size:\n"
                                                                   "  printf 'pw\\n' | rfp-cli embed cover.png raw.png \\\n"
                                                                   "      --text 'hello' --password-stdin --header off\n"
                                                                   "  #   ... embed prints 'Embedded N bytes (encrypted) (no header)';\n"
                                                                   "  #       use that N in --payload-size below.\n"
                                                                   "  printf 'pw\\n' | rfp-cli extract raw.png \\\n"
                                                                   "      --password-stdin --header off --payload-size N\n";
}

// ---------------------------------------------------------------------------
//  Argument parser
// ---------------------------------------------------------------------------
struct Args {
    std::vector<std::string> positional;
    std::vector<std::pair<std::string, std::string>> options;

    [[nodiscard]] std::optional<std::string> get(std::string_view key) const {
        for (const auto& kv : options)
            if (kv.first == key) return kv.second;
        return std::nullopt;
    }
    [[nodiscard]] bool has(std::string_view key) const {
        for (const auto& kv : options)
            if (kv.first == key) return true;
        return false;
    }
};

Args parseArgs(int argc, char** argv, int start) {
    Args a;
    for (int i = start; i < argc; ++i) {
        std::string s = argv[i];
        if (s.size() >= 2 && s[0] == '-' && s[1] == '-') {
            const auto eq = s.find('=');
            if (eq != std::string::npos) {
                a.options.emplace_back(s.substr(2, eq - 2), s.substr(eq + 1));
            } else if (i + 1 < argc &&
                       !(argv[i + 1][0] == '-' && argv[i + 1][1] == '-')) {
                a.options.emplace_back(s.substr(2), argv[i + 1]);
                ++i;
            } else {
                a.options.emplace_back(s.substr(2), "");
            }
        } else {
            a.positional.push_back(s);
        }
    }
    return a;
}

// ---------------------------------------------------------------------------
//  Environment variable reader
// ---------------------------------------------------------------------------
// MSVC with /W4 emits C4996 on std::getenv ("This function or variable may
// be unsafe"). For reading a well-known environment variable this is a false
// positive: the returned pointer is copied into a std::string immediately and
// is never stored or dereferenced later, and this CLI never calls setenv() or
// putenv(). We suppress the warning locally instead of disabling the whole
// _CRT_SECURE_NO_WARNINGS class, which would hide genuine issues elsewhere.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4996)
#endif

std::optional<std::string> readEnv(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') return std::nullopt;
    return std::string(value);
}

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

// ---------------------------------------------------------------------------
//  Password resolution
// ---------------------------------------------------------------------------
/// Precedence: --password-stdin  >  --password  >  env RFP_PASSWORD.
std::optional<std::string> resolvePassword(const Args& args) {
    if (args.has("password-stdin")) {
        std::string pw;
        if (!std::getline(std::cin, pw)) return std::nullopt;
        if (!pw.empty() && pw.back() == '\r') pw.pop_back();
        return pw;
    }
    if (auto p = args.get("password"); p && !p->empty()) return p;
    if (auto env = readEnv("RFP_PASSWORD"); env) return env;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
//  --header on|off  (default: on)
// ---------------------------------------------------------------------------
bool parseHeaderFlag(const Args& args) {
    if (auto s = args.get("header")) {
        if (*s == "off" || *s == "false" || *s == "0") return false;
        if (*s == "on"  || *s == "true"  || *s == "1") return true;
        std::cerr << "Invalid --header value: " << *s
                  << " (expected on|off)\n";
        std::exit(EXIT_FAILURE);
    }
    return true;
}

// ---------------------------------------------------------------------------
//  Enum resolution
// ---------------------------------------------------------------------------
std::optional<rfp::crypto::CipherId> resolveCipher(std::string_view n) {
    return rfp::crypto::CryptoRegistry::cipherByName(n);
}
std::optional<rfp::crypto::KdfId> resolveKdf(std::string_view n) {
    return rfp::crypto::CryptoRegistry::kdfByName(n);
}
std::optional<rfp::crypto::HashId> resolveHash(std::string_view n) {
    return rfp::crypto::CryptoRegistry::hashByName(n);
}

// ---------------------------------------------------------------------------
//  Params
// ---------------------------------------------------------------------------
rfp::stego::StegoParams makeStegoParams(const Args& args) {
    rfp::stego::StegoParams p;
    if (auto s = args.get("bits"))
        p.bitsPerChannel = static_cast<std::uint8_t>(std::stoi(*s));
    if (auto s = args.get("seed"))
        p.seed = static_cast<std::uint32_t>(std::stoul(*s));
    if (auto s = args.get("channels")) {
        p.useRedChannel   = s->find_first_of("Rr") != std::string::npos;
        p.useGreenChannel = s->find_first_of("Gg") != std::string::npos;
        p.useBlueChannel  = s->find_first_of("Bb") != std::string::npos;
        p.useAlphaChannel = s->find_first_of("Aa") != std::string::npos;
    }
    if (auto s = args.get("mode"))
        p.mode = (*s == "smart" || *s == "dispersion")
                     ? rfp::stego::SlotSelectionMode::Dispersion
                     : rfp::stego::SlotSelectionMode::Uniform;
    if (auto s = args.get("window"))
        p.windowSize = std::stoi(*s);
    if (auto s = args.get("metric")) {
        if (*s == "luminance")       p.metric = rfp::stego::DispersionMetric::Luminance;
        else if (*s == "per-channel" ||
                 *s == "perchannel") p.metric = rfp::stego::DispersionMetric::PerChannel;
        else if (*s == "sum")        p.metric = rfp::stego::DispersionMetric::Sum;
    }
    if (auto s = args.get("threshold"))
        p.dispersionThreshold = std::stod(*s);
    if (auto s = args.get("shuffle"))
        p.applyShuffleAfterSort = (*s == "on" || *s == "true" || *s == "1");
    return p;
}

rfp::payload::EncryptParams makeEncryptParams(const Args& args,
                                              const std::string& password) {
    rfp::payload::EncryptParams p;
    p.password = password;
    if (auto c = args.get("cipher"))
        if (auto id = resolveCipher(*c)) p.cipher = *id;
    if (auto k = args.get("kdf"))
        if (auto id = resolveKdf(*k)) p.kdf = *id;
    if (auto it = args.get("iterations"))
        p.iterations = static_cast<std::uint32_t>(std::stoul(*it));
    return p;
}

// ---------------------------------------------------------------------------
//  Commands
// ---------------------------------------------------------------------------
int cmdCrc(int argc, char** argv) {
    if (argc < 3) { std::cerr << "Usage: rfp-cli crc <text>\n"; return EXIT_FAILURE; }
    std::cout << rfp::core::crc32(std::string_view(argv[2])) << '\n';
    return EXIT_SUCCESS;
}

int cmdHash(int argc, char** argv) {
    if (argc < 3) { std::cerr << "Usage: rfp-cli hash <text> [--algo <name>]\n"; return EXIT_FAILURE; }
    Args args = parseArgs(argc, argv, 3);
    const std::string name = args.get("algo").value_or("SHA-256");
    const auto id = resolveHash(name);
    if (!id) { std::cerr << "Unknown hash algorithm: " << name << '\n'; return EXIT_FAILURE; }

    auto data = rfp::core::toBytes(argv[2]);
    auto r = rfp::crypto::Hash::digest(*id, data);
    if (!r) { std::cerr << "Hash failed: " << r.error().message << '\n'; return EXIT_FAILURE; }
    std::cout << rfp::crypto::hexEncode(r.value()) << '\n';
    return EXIT_SUCCESS;
}

int cmdEncrypt(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: rfp-cli encrypt <text> (--password <pw> | --password-stdin)\n";
        return EXIT_FAILURE;
    }
    Args args = parseArgs(argc, argv, 3);

    auto pw = resolvePassword(args);
    if (!pw) {
        std::cerr << "Missing password: use --password <pw>, --password-stdin, "
                     "or env RFP_PASSWORD\n";
        return EXIT_FAILURE;
    }
    if (auto c = args.get("cipher"); c && !resolveCipher(*c)) {
        std::cerr << "Unknown cipher: " << *c << '\n'; return EXIT_FAILURE;
    }
    if (auto k = args.get("kdf"); k && !resolveKdf(*k)) {
        std::cerr << "Unknown KDF: " << *k << '\n'; return EXIT_FAILURE;
    }

    const auto params = makeEncryptParams(args, *pw);
    auto plaintext = rfp::core::toBytes(argv[2]);
    auto r = rfp::payload::encrypt(plaintext, params);
    if (!r) { std::cerr << "Encryption failed: " << r.error().message << '\n'; return EXIT_FAILURE; }

    if (args.has("hex")) std::cout << rfp::crypto::hexEncode(r.value())    << '\n';
    else                 std::cout << rfp::crypto::base64Encode(r.value()) << '\n';

    // Handy hint on stderr — doesn't pollute stdout (which may be piped).
    if (!args.has("quiet")) {
        std::cerr << "[rfp] RFP1 blob size: " << r.value().size() << " bytes\n";
    }
    return EXIT_SUCCESS;
}

int cmdDecrypt(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: rfp-cli decrypt <encoded> (--password <pw> | --password-stdin) [--hex]\n";
        return EXIT_FAILURE;
    }
    Args args = parseArgs(argc, argv, 3);

    auto pw = resolvePassword(args);
    if (!pw) {
        std::cerr << "Missing password: use --password <pw>, --password-stdin, "
                     "or env RFP_PASSWORD\n";
        return EXIT_FAILURE;
    }

    const bool hex = args.has("hex");
    auto blob = hex ? rfp::crypto::hexDecode(argv[2])
                    : rfp::crypto::base64Decode(argv[2]);
    if (blob.empty()) { std::cerr << "Failed to decode input\n"; return EXIT_FAILURE; }

    rfp::payload::DecryptParams p;
    p.password = *pw;
    auto r = rfp::payload::decrypt(blob, p);
    if (!r) { std::cerr << "Decryption failed: " << r.error().message << '\n'; return EXIT_FAILURE; }

    std::cout.write(reinterpret_cast<const char*>(r.value().data()),
                    static_cast<std::streamsize>(r.value().size()));
    std::cout << '\n';
    return EXIT_SUCCESS;
}

int cmdEmbed(int argc, char** argv) {
    Args args = parseArgs(argc, argv, 2);
    if (args.positional.size() < 2) {
        std::cerr << "Usage: rfp-cli embed <in.(png|ppm)> <out.(png|ppm)> --text <text> [pw-opts]\n";
        return EXIT_FAILURE;
    }
    const auto textOpt = args.get("text");
    if (!textOpt) { std::cerr << "Missing --text <text>\n"; return EXIT_FAILURE; }

    const std::string& inputPath  = args.positional[0];
    const std::string& outputPath = args.positional[1];

    auto imgResult = rfp::cli::loadImage(inputPath);
    if (!imgResult) { std::cerr << "Load failed: " << imgResult.error().message << '\n'; return EXIT_FAILURE; }
    auto image = std::move(imgResult.value());

    auto raw = rfp::core::toBytes(*textOpt);
    rfp::core::ByteBuffer payload;

    // Password is optional for embed — determines whether to encrypt.
    auto pw = resolvePassword(args);
    if (pw) {
        if (auto c = args.get("cipher"); c && !resolveCipher(*c)) {
            std::cerr << "Unknown cipher: " << *c << '\n'; return EXIT_FAILURE;
        }
        if (auto k = args.get("kdf"); k && !resolveKdf(*k)) {
            std::cerr << "Unknown KDF: " << *k << '\n'; return EXIT_FAILURE;
        }
        const auto eparams = makeEncryptParams(args, *pw);
        auto enc = rfp::payload::encrypt(raw, eparams);
        if (!enc) { std::cerr << "Encryption failed: " << enc.error().message << '\n'; return EXIT_FAILURE; }
        payload = std::move(enc.value());
    } else {
        payload = std::move(raw);
    }

    const bool writeHeader = parseHeaderFlag(args);

    // Optional 4-byte big-endian length prefix.
    rfp::core::ByteBuffer framed;
    if (writeHeader) {
        if (payload.size() > 0xFFFFFFFFu) {
            std::cerr << "Payload too large for 4-byte size header\n";
            return EXIT_FAILURE;
        }
        const auto sz = static_cast<std::uint32_t>(payload.size());
        framed.reserve(4 + payload.size());
        framed.push_back(static_cast<rfp::core::Byte>((sz >> 24) & 0xFF));
        framed.push_back(static_cast<rfp::core::Byte>((sz >> 16) & 0xFF));
        framed.push_back(static_cast<rfp::core::Byte>((sz >>  8) & 0xFF));
        framed.push_back(static_cast<rfp::core::Byte>( sz        & 0xFF));
        framed.insert(framed.end(), payload.begin(), payload.end());
    } else {
        framed = std::move(payload);
    }

    const auto params = makeStegoParams(args);
    const auto capacity = rfp::stego::capacityBytes(image, params);
    if (framed.size() > capacity) {
        std::cerr << "Payload too large: need " << framed.size()
        << " bytes, capacity is " << capacity << " bytes\n";
        return EXIT_FAILURE;
    }

    auto embedResult = rfp::stego::StegoEncoder::embedBytes(image, framed, params);
    if (!embedResult) { std::cerr << "Embedding failed: " << embedResult.error().message << '\n'; return EXIT_FAILURE; }

    auto saveResult = rfp::cli::saveImage(embedResult.value(), outputPath);
    if (!saveResult) { std::cerr << "Save failed: " << saveResult.error().message << '\n'; return EXIT_FAILURE; }

    std::cout << "Embedded " << framed.size() << " bytes";
    if (pw) std::cout << " (encrypted)";
    if (!writeHeader) {
        std::cout << " (no header) — use --payload-size "
                  << payload.size() << " with --header off on extract";
    }
    std::cout << " into " << outputPath << '\n';
    return EXIT_SUCCESS;
}

int cmdExtract(int argc, char** argv) {
    Args args = parseArgs(argc, argv, 2);
    if (args.positional.empty()) {
        std::cerr << "Usage: rfp-cli extract <in.(png|ppm)> "
                     "[pw-opts] [--header on|off] [--payload-size N] [stego-opts]\n";
        return EXIT_FAILURE;
    }

    auto imgResult = rfp::cli::loadImage(args.positional[0]);
    if (!imgResult) {
        std::cerr << "Load failed: " << imgResult.error().message << '\n';
        return EXIT_FAILURE;
    }
    const auto& image = imgResult.value();
    const auto params = makeStegoParams(args);
    const bool hasHeader = parseHeaderFlag(args);

    // StegoDecoder always starts from slot 0. Read the WHOLE frame in one
    // call and slice it in memory — a second call with a different size would
    // re-read the header instead of reaching the payload.
    const std::size_t capacity = rfp::stego::capacityBytes(image, params);
    if (capacity == 0) {
        std::cerr << "Image has zero capacity for these parameters\n";
        return EXIT_FAILURE;
    }

    auto frameResult = rfp::stego::StegoDecoder::extractBytes(image, capacity, params);
    if (!frameResult) {
        std::cerr << "Extraction failed: " << frameResult.error().message << '\n';
        return EXIT_FAILURE;
    }
    const auto& frame = frameResult.value();

    // --- Decide payload boundaries ---
    std::size_t payloadStart = 0;
    std::size_t payloadSize  = 0;

    if (hasHeader) {
        if (frame.size() < 4) {
            std::cerr << "Frame too short to contain a size header\n";
            return EXIT_FAILURE;
        }
        payloadSize =
            (static_cast<std::uint32_t>(frame[0]) << 24) |
            (static_cast<std::uint32_t>(frame[1]) << 16) |
            (static_cast<std::uint32_t>(frame[2]) <<  8) |
            static_cast<std::uint32_t>(frame[3]);
        payloadStart = 4;

        if (payloadSize == 0 ||
            payloadStart + payloadSize > frame.size()) {
            std::cerr << "Invalid payload size in header: " << payloadSize
                      << " (capacity: " << frame.size() << " bytes). "
                                                           "Did you forget --header off?\n";
            return EXIT_FAILURE;
        }
    } else {
        const auto sizeOpt = args.get("payload-size");
        if (!sizeOpt) {
            std::cerr << "With --header off, --payload-size <N> is required\n";
            return EXIT_FAILURE;
        }
        payloadSize = static_cast<std::size_t>(std::stoul(*sizeOpt));
        if (payloadSize == 0 || payloadSize > frame.size()) {
            std::cerr << "Invalid --payload-size " << payloadSize
                      << " (capacity: " << frame.size() << " bytes)\n";
            return EXIT_FAILURE;
        }
    }

    rfp::core::ByteBuffer payload(
        frame.begin() + static_cast<std::ptrdiff_t>(payloadStart),
        frame.begin() + static_cast<std::ptrdiff_t>(payloadStart + payloadSize));

    // --- Optional decryption ---
    rfp::core::ByteBuffer plaintext;
    auto pw = resolvePassword(args);
    if (pw) {
        rfp::payload::DecryptParams dp;
        dp.password = *pw;
        auto dec = rfp::payload::decrypt(payload, dp);
        if (!dec) {
            std::cerr << "Decryption failed: " << dec.error().message << '\n';
            return EXIT_FAILURE;
        }
        plaintext = std::move(dec.value());
    } else {
        if (rfp::payload::isPayload(payload)) {
            std::cerr << "Warning: payload appears encrypted; "
                         "re-run with --password/--password-stdin to decrypt.\n";
        }
        plaintext = std::move(payload);
    }

    std::cout.write(reinterpret_cast<const char*>(plaintext.data()),
                    static_cast<std::streamsize>(plaintext.size()));
    std::cout << '\n';
    return EXIT_SUCCESS;
}

int cmdSelfTest(int argc, char** argv) {
    Args args = parseArgs(argc, argv, 2);
    const auto params = makeStegoParams(args);

    rfp::stego::ImageBuffer image;
    image.width = 16; image.height = 16; image.channels = 4;
    image.pixels.assign(image.byteSize(), 0xAAU);

    const std::string text = "hello";
    auto encoded = rfp::stego::StegoEncoder::embedText(image, text, params);
    if (!encoded) { std::cerr << encoded.error().message << '\n'; return EXIT_FAILURE; }

    auto decoded = rfp::stego::StegoDecoder::extractBytes(encoded.value(), text.size(), params);
    if (!decoded) { std::cerr << decoded.error().message << '\n'; return EXIT_FAILURE; }
    if (rfp::core::bytesToString(decoded.value()) != text) {
        std::cerr << "Round trip failed\n"; return EXIT_FAILURE;
    }
    std::cout << "Self-test passed\n";
    return EXIT_SUCCESS;
}

int cmdCryptoInfo() {
    std::cout << "Backend: " << rfp::crypto::CryptoRegistry::backendVersion() << "\n";
    std::cout << "\nCiphers:\n";
    for (const auto& c : rfp::crypto::CryptoRegistry::availableCiphers())
        std::cout << "  " << c.name
                  << "  (key=" << c.keySize
                  << "B, iv="  << c.ivSize
                  << "B, tag=" << c.tagSize << "B)\n";
    std::cout << "\nKDFs:\n";
    for (const auto& k : rfp::crypto::CryptoRegistry::availableKdfs())
        std::cout << "  " << k.name << "\n";
    std::cout << "\nHashes:\n";
    for (const auto& h : rfp::crypto::CryptoRegistry::availableHashes())
        std::cout << "  " << h.name << "  (" << h.digestSize << "B)\n";
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv) {
    if (argc <= 1) { printHelp(); return EXIT_SUCCESS; }

    const std::string command = argv[1];

    try {
        if (command == "--help" || command == "-h" || command == "help") {
            printHelp(); return EXIT_SUCCESS;
        }
        if (command == "--version" || command == "-v") {
            std::cout << "rfp-cli " << kCliVersion << '\n'; return EXIT_SUCCESS;
        }
        if (command == "crc")         return cmdCrc(argc, argv);
        if (command == "hash")        return cmdHash(argc, argv);
        if (command == "encrypt")     return cmdEncrypt(argc, argv);
        if (command == "decrypt")     return cmdDecrypt(argc, argv);
        if (command == "embed")       return cmdEmbed(argc, argv);
        if (command == "extract")     return cmdExtract(argc, argv);
        if (command == "self-test")   return cmdSelfTest(argc, argv);
        if (command == "crypto-info") return cmdCryptoInfo();

        std::cerr << "Unknown command: " << command << "\n\n";
        printHelp();
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}