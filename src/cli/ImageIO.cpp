#include "ImageIO.h"
#include "PngIO.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace rfp::cli {

namespace {

using rfp::core::Byte;
using rfp::core::Error;
using rfp::core::ErrorCode;
using rfp::core::Result;

// ---------------- PPM / PGM (unchanged) ----------------

void skipWs(std::istream& in) {
    while (in.good()) {
        const int c = in.peek();
        if (c == '#') { std::string line; std::getline(in, line); }
        else if (std::isspace(c)) { in.get(); }
        else break;
    }
}

bool readUint(std::istream& in, std::uint32_t& out) {
    skipWs(in);
    in >> out;
    return static_cast<bool>(in);
}

Result<rfp::stego::ImageBuffer> loadPnm(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return Error{ErrorCode::IoError, "Cannot open file: " + path};

    char magic[2] = {0, 0};
    in.read(magic, 2);
    if (!in || magic[0] != 'P')
        return Error{ErrorCode::UnsupportedFormat, "Not a PNM file"};

    std::uint32_t w = 0, h = 0, maxVal = 0;
    if (!readUint(in, w) || !readUint(in, h) || !readUint(in, maxVal))
        return Error{ErrorCode::UnsupportedFormat, "Bad PNM header"};
    if (maxVal != 255)
        return Error{ErrorCode::UnsupportedFormat, "Only maxVal=255 supported"};

    in.get(); // single whitespace

    rfp::stego::ImageBuffer image;
    image.width    = w;
    image.height   = h;
    image.channels = 3;
    image.pixels.resize(image.byteSize());

    if (magic[1] == '6') {
        in.read(reinterpret_cast<char*>(image.pixels.data()),
                static_cast<std::streamsize>(image.pixels.size()));
        if (!in) return Error{ErrorCode::IoError, "Truncated PPM data"};
    } else if (magic[1] == '5') {
        std::vector<Byte> gray(static_cast<std::size_t>(w) * h);
        in.read(reinterpret_cast<char*>(gray.data()),
                static_cast<std::streamsize>(gray.size()));
        if (!in) return Error{ErrorCode::IoError, "Truncated PGM data"};
        for (std::size_t i = 0; i < gray.size(); ++i) {
            image.pixels[i * 3 + 0] = gray[i];
            image.pixels[i * 3 + 1] = gray[i];
            image.pixels[i * 3 + 2] = gray[i];
        }
    } else {
        return Error{ErrorCode::UnsupportedFormat,
                     "Unsupported PNM magic (only P5/P6)"};
    }

    if (!image.isValid())
        return Error{ErrorCode::InvalidImageBuffer, "Invalid image after load"};
    return image;
}

Result<void> savePnm(const rfp::stego::ImageBuffer& image,
                     const std::string& path) {
    if (!image.isValid())
        return Error{ErrorCode::InvalidImageBuffer, "Invalid image buffer"};

    std::ofstream out(path, std::ios::binary);
    if (!out) return Error{ErrorCode::IoError, "Cannot create file: " + path};

    out << "P6\n" << image.width << ' ' << image.height << "\n255\n";

    if (image.channels == 3) {
        out.write(reinterpret_cast<const char*>(image.pixels.data()),
                  static_cast<std::streamsize>(image.pixels.size()));
    } else if (image.channels == 4) {
        const auto pixelCount =
            static_cast<std::size_t>(image.width) * image.height;
        std::vector<Byte> rgb(pixelCount * 3);
        for (std::size_t i = 0; i < pixelCount; ++i) {
            rgb[i * 3 + 0] = image.pixels[i * 4 + 0];
            rgb[i * 3 + 1] = image.pixels[i * 4 + 1];
            rgb[i * 3 + 2] = image.pixels[i * 4 + 2];
        }
        out.write(reinterpret_cast<const char*>(rgb.data()),
                  static_cast<std::streamsize>(rgb.size()));
    } else {
        return Error{ErrorCode::UnsupportedFormat,
                     "Unsupported channel count for PPM export"};
    }

    if (!out) return Error{ErrorCode::IoError, "Write failed"};
    return {};
}

// ---------------- Format detection ----------------

enum class Format { Unknown, Png, Pnm };

Format detectFormat(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return Format::Unknown;

    unsigned char buf[8] = {0};
    in.read(reinterpret_cast<char*>(buf), 8);
    const auto n = in.gcount();

    if (n >= 8 &&
        buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G' &&
        buf[4] == 0x0D && buf[5] == 0x0A && buf[6] == 0x1A && buf[7] == 0x0A) {
        return Format::Png;
    }
    if (n >= 2 && buf[0] == 'P' && (buf[1] == '5' || buf[1] == '6')) {
        return Format::Pnm;
    }
    return Format::Unknown;
}

std::string toLowerExt(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

} // namespace

Result<rfp::stego::ImageBuffer> loadImage(const std::string& path) {
    switch (detectFormat(path)) {
    case Format::Png: return png::load(path);
    case Format::Pnm: return loadPnm(path);
    case Format::Unknown:
        break;
    }
    return Error{ErrorCode::UnsupportedFormat,
                 "Unrecognised image format (only PNG and PPM/PGM)"};
}

Result<void> saveImage(const rfp::stego::ImageBuffer& image,
                       const std::string& path) {
    const std::string ext = toLowerExt(path);
    if (ext == ".ppm" || ext == ".pgm" || ext == ".pnm") {
        return savePnm(image, path);
    }
    // Default: PNG (also for ".png" and anything unknown).
    return png::save(image, path);
}

} // namespace rfp::cli