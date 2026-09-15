#include "PngIO.h"

#include "rfp/core/Error.h"

#include <png.h>

#include <cstdio>
#include <cstdint>
#include <vector>

namespace rfp::cli::png {

namespace {

using rfp::core::Byte;
using rfp::core::Error;
using rfp::core::ErrorCode;
using rfp::core::Result;

} // namespace

Result<rfp::stego::ImageBuffer> load(const std::string& path)
{
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp)
        return Error{ErrorCode::IoError, "Cannot open PNG: " + path};

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                             nullptr, nullptr, nullptr);
    if (!png) { std::fclose(fp); return Error{ErrorCode::DecodeError, "png_create_read_struct"}; }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        std::fclose(fp);
        return Error{ErrorCode::DecodeError, "png_create_info_struct"};
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(fp);
        return Error{ErrorCode::DecodeError, "libpng read error"};
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    const png_uint_32 w = png_get_image_width(png, info);
    const png_uint_32 h = png_get_image_height(png, info);
    const png_byte colorType = png_get_color_type(png, info);
    const png_byte bitDepth  = png_get_bit_depth(png, info);

    // Нормализуем всё к 8-bit RGB или RGBA.
    if (bitDepth == 16) png_set_strip_16(png);
    if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    const png_byte newColorType = png_get_color_type(png, info);
    const int channels = (newColorType == PNG_COLOR_TYPE_RGB_ALPHA) ? 4 : 3;

    rfp::stego::ImageBuffer image;
    image.width    = static_cast<std::uint32_t>(w);
    image.height   = static_cast<std::uint32_t>(h);
    image.channels = static_cast<std::uint8_t>(channels);
    image.pixels.resize(image.byteSize());

    const png_size_t rowBytes = png_get_rowbytes(png, info);
    if (rowBytes != static_cast<png_size_t>(w) * static_cast<png_size_t>(channels)) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(fp);
        return Error{ErrorCode::DecodeError, "Unexpected PNG row size"};
    }

    std::vector<png_bytep> rows(h);
    for (png_uint_32 y = 0; y < h; ++y)
        rows[y] = image.pixels.data() + static_cast<std::size_t>(y) * rowBytes;

    png_read_image(png, rows.data());
    png_read_end(png, nullptr);

    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(fp);

    if (!image.isValid())
        return Error{ErrorCode::InvalidImageBuffer, "Invalid PNG buffer"};
    return image;
}

Result<void> save(const rfp::stego::ImageBuffer& image, const std::string& path)
{
    if (!image.isValid())
        return Error{ErrorCode::InvalidImageBuffer, "Invalid image buffer"};
    if (image.channels != 3 && image.channels != 4)
        return Error{ErrorCode::UnsupportedFormat,
                     "PNG output requires 3 or 4 channels"};

    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp)
        return Error{ErrorCode::IoError, "Cannot create PNG: " + path};

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              nullptr, nullptr, nullptr);
    if (!png) { std::fclose(fp); return Error{ErrorCode::DecodeError, "png_create_write_struct"}; }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(fp);
        return Error{ErrorCode::DecodeError, "png_create_info_struct"};
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(fp);
        return Error{ErrorCode::DecodeError, "libpng write error"};
    }

    png_init_io(png, fp);

    const int colorType = (image.channels == 4)
                              ? PNG_COLOR_TYPE_RGBA
                              : PNG_COLOR_TYPE_RGB;
    png_set_IHDR(png, info,
                 static_cast<png_uint_32>(image.width),
                 static_cast<png_uint_32>(image.height),
                 8, colorType,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    const std::size_t rowBytes =
        static_cast<std::size_t>(image.width) * image.channels;
    for (std::uint32_t y = 0; y < image.height; ++y)
        png_write_row(png, image.pixels.data() + static_cast<std::size_t>(y) * rowBytes);

    png_write_end(png, nullptr);

    png_destroy_write_struct(&png, &info);
    std::fclose(fp);
    return {};
}

} // namespace rfp::cli::png
