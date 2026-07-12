#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Crc32.h"
#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoEncoder.h"
#include "rfp/stego/StegoParams.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>

namespace {

void printHelp() {
  std::cout << "R.F.P. - Resilient File Protector CLI\n"
            << "\n"
            << "Usage:\n"
            << "  rfp-cli --help\n"
            << "  rfp-cli crc <text>\n"
            << "  rfp-cli self-test [options]\n"
            << "\n"
            << "Options for self-test:\n"
            << "  --mode <uniform|smart>          (default: uniform)\n"
            << "  --window <3|5|7|9|11|13>        (default: 3)\n"
            << "  --metric <luminance|per-channel|sum> (default: luminance)\n"
            << "  --threshold <value>             (default: 0.0)\n"
            << "  --shuffle <on|off>              (default: on)\n"
            << "  --bits <1-4>                    (default: 1)\n"
            << "  --seed <number>                 (default: 0)\n"
            << "\n"
            << "Image file I/O is provided by the Qt GUI module in the current "
               "stage.\n";
}

template <typename T>
std::optional<T> parseEnum(const char *str,
                           const std::pair<const char *, T> *mapping,
                           size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (std::strcmp(str, mapping[i].first) == 0) {
      return mapping[i].second;
    }
  }
  return std::nullopt;
}

int runSelfTest(int argc, char **argv) {
  rfp::stego::StegoParams params;
  params.mode = rfp::stego::SlotSelectionMode::Uniform;
  params.windowSize = 3;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.dispersionThreshold = 0.0;
  params.applyShuffleAfterSort = true;

  for (int i = 2; i < argc; ++i) {
    if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
      ++i;
      std::string val = argv[i];
      if (val == "uniform")
        params.mode = rfp::stego::SlotSelectionMode::Uniform;
      else if (val == "smart")
        params.mode = rfp::stego::SlotSelectionMode::Smart;
      else {
        std::cerr << "Unknown mode\n";
        return EXIT_FAILURE;
      }
    } else if (std::strcmp(argv[i], "--window") == 0 && i + 1 < argc) {
      ++i;
      params.windowSize = std::atoi(argv[i]);
    } else if (std::strcmp(argv[i], "--metric") == 0 && i + 1 < argc) {
      ++i;
      std::string val = argv[i];
      if (val == "luminance")
        params.metric = rfp::stego::DispersionMetric::Luminance;
      else if (val == "per-channel")
        params.metric = rfp::stego::DispersionMetric::PerChannel;
      else if (val == "sum")
        params.metric = rfp::stego::DispersionMetric::Sum;
      else {
        std::cerr << "Unknown metric\n";
        return EXIT_FAILURE;
      }
    } else if (std::strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) {
      ++i;
      params.dispersionThreshold = std::atof(argv[i]);
    } else if (std::strcmp(argv[i], "--shuffle") == 0 && i + 1 < argc) {
      ++i;
      std::string val = argv[i];
      if (val == "on")
        params.applyShuffleAfterSort = true;
      else if (val == "off")
        params.applyShuffleAfterSort = false;
      else {
        std::cerr << "Unknown shuffle value\n";
        return EXIT_FAILURE;
      }
    } else if (std::strcmp(argv[i], "--bits") == 0 && i + 1 < argc) {
      ++i;
      params.bitsPerChannel = static_cast<std::uint8_t>(std::atoi(argv[i]));
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      ++i;
      params.seed = static_cast<std::uint32_t>(std::atoi(argv[i]));
    } else {
      std::cerr << "Unknown option: " << argv[i] << "\n";
      return EXIT_FAILURE;
    }
  }

  rfp::stego::ImageBuffer image;
  image.width = 16;
  image.height = 16;
  image.channels = 4;
  image.pixels.assign(image.byteSize(), 0xAAU);

  const std::string text = "hello";
  auto encoded = rfp::stego::StegoEncoder::embedText(image, text, params);
  if (!encoded) {
    std::cerr << encoded.error().message << '\n';
    return EXIT_FAILURE;
  }

  auto decoded = rfp::stego::StegoDecoder::extractBytes(encoded.value(),
                                                        text.size(), params);
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

int main(int argc, char **argv) {
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
    return runSelfTest(argc, argv);
  }

  std::cerr << "Unknown command: " << command << '\n';
  return EXIT_FAILURE;
}