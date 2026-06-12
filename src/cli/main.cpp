#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Crc32.h"
#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoEncoder.h"
#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void printHelp()
{
    std::cout
        << "R.F.P. - Resilient File Protector CLI\n"
        << "\n"
        << "Usage:\n"
        << "  rfp-cli --help\n"
        << "  rfp-cli crc <text>\n"
        << "  rfp-cli self-test\n"
        << "\n"
        << "Image file I/O is provided by the Qt GUI module in the current stage.\n";
}

int runSelfTest()
{
    rfp::stego::ImageBuffer image;
    image.width = 16;
    image.height = 16;
    image.channels = 4;
    image.pixels.assign(image.byteSize(), 0xAAU);

    rfp::stego::StegoParams params;
    params.bitsPerChannel = 1;
    params.seed = 42;

    const std::string text = "hello";
    auto encoded = rfp::stego::StegoEncoder::embedText(image, text, params);
    if (!encoded) {
        std::cerr << encoded.error().message << '\n';
        return EXIT_FAILURE;
    }

    auto decoded = rfp::stego::StegoDecoder::extractBytes(encoded.value(), text.size(), params);
    if (!decoded) {
        std::cerr << decoded.error().message << '\n';
        return EXIT_FAILURE;
    }

    const auto decodedText = rfp::core::bytesToString(decoded.value());
    if (decodedText != text) {
        std::cerr << "Round trip failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Self-test passed\n";
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc <= 1) {
        printHelp();
        return EXIT_SUCCESS;
    }

    const std::string command = argv[1];
    if (command == "--help" || command == "-h") {
        printHelp();
        return EXIT_SUCCESS;
    }

    if (command == "crc") {
        if (argc < 3) {
            std::cerr << "Missing text argument\n";
            return EXIT_FAILURE;
        }
        std::cout << rfp::core::crc32(std::string_view(argv[2])) << '\n';
        return EXIT_SUCCESS;
    }

    if (command == "self-test") {
        return runSelfTest();
    }

    std::cerr << "Unknown command: " << command << '\n';
    return EXIT_FAILURE;
}
