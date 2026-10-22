#include "pages/gallery/gallery_image.hpp"

#include "components/components.hpp"

#define LODEPNG_NO_COMPILE_CPP
#include <src/libs/lodepng/lodepng.h>
#include <src/draw/lv_image_decoder.h>
#include <src/draw/lv_image_decoder_private.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <sys/stat.h>

namespace {

constexpr std::uint32_t kStoredThumbnailWidth  = 360;
constexpr std::uint32_t kStoredThumbnailHeight = 640;
constexpr std::uint64_t kMaximumSourcePixels   = 16U * 1024U * 1024U;
constexpr std::uint64_t kMaximumInputBytes     = 64U * 1024U * 1024U;
constexpr std::uint64_t kMaximumJpegInputBytes = 8U * 1024U * 1024U;

enum class PixelFormat { rgb, rgba, rgb565 };

struct SourceImage {
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
    PixelFormat format   = PixelFormat::rgb;
    std::vector<std::uint8_t> pixels;
};

struct RenderMapping {
    std::uint32_t draw_width     = 0;
    std::uint32_t draw_height    = 0;
    std::uint32_t offset_x       = 0;
    std::uint32_t offset_y       = 0;
    std::uint32_t source_x       = 0;
    std::uint32_t source_y       = 0;
    std::uint32_t visible_width  = 0;
    std::uint32_t visible_height = 0;
};

std::string lowercase_extension(const std::string &path)
{
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

void write_rgb565(std::uint8_t *target, unsigned red, unsigned green, unsigned blue)
{
    const auto value = static_cast<std::uint16_t>(((red & 0xf8U) << 8U) | ((green & 0xfcU) << 3U) | (blue >> 3U));
    target[0]        = static_cast<std::uint8_t>(value & 0xffU);
    target[1]        = static_cast<std::uint8_t>(value >> 8U);
}

void fill_rgb565(std::vector<std::uint8_t> &target, std::uint32_t color)
{
    const auto red   = (color >> 16U) & 0xffU;
    const auto green = (color >> 8U) & 0xffU;
    const auto blue  = color & 0xffU;
    for (std::size_t offset = 0; offset + 1 < target.size(); offset += 2) {
        write_rgb565(target.data() + offset, red, green, blue);
    }
}

bool source_size_is_valid(std::uint32_t width, std::uint32_t height, std::size_t bytes_per_pixel)
{
    if (width == 0 || height == 0 || static_cast<std::uint64_t>(width) * height > kMaximumSourcePixels) return false;
    const auto bytes = static_cast<std::uint64_t>(width) * height * bytes_per_pixel;
    return bytes <= kMaximumInputBytes && bytes <= std::numeric_limits<std::size_t>::max();
}

RenderMapping calculate_mapping(std::uint32_t source_width, std::uint32_t source_height, std::uint32_t target_width,
                                std::uint32_t target_height, GalleryImageFit fit)
{
    RenderMapping mapping{
        target_width, target_height, 0, 0, 0, 0, source_width, source_height,
    };

    if (fit == GalleryImageFit::contain) {
        if (static_cast<std::uint64_t>(source_width) * target_height >
            static_cast<std::uint64_t>(source_height) * target_width) {
            mapping.draw_height =
                std::max<std::uint32_t>(1, static_cast<std::uint64_t>(source_height) * target_width / source_width);
        } else {
            mapping.draw_width =
                std::max<std::uint32_t>(1, static_cast<std::uint64_t>(source_width) * target_height / source_height);
        }
        mapping.offset_x = (target_width - mapping.draw_width) / 2U;
        mapping.offset_y = (target_height - mapping.draw_height) / 2U;
    } else if (static_cast<std::uint64_t>(source_width) * target_height >
               static_cast<std::uint64_t>(source_height) * target_width) {
        mapping.visible_width =
            std::max<std::uint32_t>(1, static_cast<std::uint64_t>(source_height) * target_width / target_height);
        mapping.source_x = (source_width - mapping.visible_width) / 2U;
    } else {
        mapping.visible_height =
            std::max<std::uint32_t>(1, static_cast<std::uint64_t>(source_width) * target_height / target_width);
        mapping.source_y = (source_height - mapping.visible_height) / 2U;
    }
    return mapping;
}

bool read_stored_thumbnail(const std::string &path, SourceImage &image)
{
    auto *file = std::fopen(path.c_str(), "rb");
    if (!file) return false;

    image.width  = kStoredThumbnailWidth;
    image.height = kStoredThumbnailHeight;
    image.format = PixelFormat::rgb565;
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height * 2U);
    const auto bytes_read = std::fread(image.pixels.data(), 1, image.pixels.size(), file);
    const bool at_end     = std::fgetc(file) == EOF;
    std::fclose(file);
    if (bytes_read != image.pixels.size() || !at_end) {
        image = {};
        return false;
    }
    return true;
}

bool read_ppm_token(FILE *file, std::string &token, std::size_t *remaining = nullptr)
{
    token.clear();
    const auto next = [file, remaining]() {
        if (remaining) {
            if (*remaining == 0) return EOF;
            --*remaining;
        }
        return std::fgetc(file);
    };
    int value = 0;
    while ((value = next()) != EOF) {
        if (value == '#') {
            while ((value = next()) != EOF && value != '\n') {
            }
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(value))) break;
    }
    if (value == EOF) return false;

    do {
        if (token.size() >= 32) return false;
        token.push_back(static_cast<char>(value));
        value = next();
    } while (value != EOF && !std::isspace(static_cast<unsigned char>(value)));
    return !token.empty();
}

bool parse_unsigned(const std::string &text, std::uint32_t &value)
{
    if (text.empty()) return false;
    char *end         = nullptr;
    errno             = 0;
    const auto parsed = std::strtoul(text.c_str(), &end, 10);
    if (errno != 0 || end != text.c_str() + text.size() || parsed > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool read_ppm(const std::string &path, SourceImage &image)
{
    auto *file = std::fopen(path.c_str(), "rb");
    if (!file) return false;

    std::string magic;
    std::string width_text;
    std::string height_text;
    std::string maximum_text;
    std::uint32_t width     = 0;
    std::uint32_t height    = 0;
    std::uint32_t maximum   = 0;
    const bool header_valid = read_ppm_token(file, magic) && read_ppm_token(file, width_text) &&
                              read_ppm_token(file, height_text) && read_ppm_token(file, maximum_text) &&
                              magic == "P6" && parse_unsigned(width_text, width) &&
                              parse_unsigned(height_text, height) && parse_unsigned(maximum_text, maximum) &&
                              maximum == 255 && source_size_is_valid(width, height, 3);
    if (!header_valid) {
        std::fclose(file);
        return false;
    }

    image.width  = width;
    image.height = height;
    image.format = PixelFormat::rgb;
    image.pixels.resize(static_cast<std::size_t>(width) * height * 3U);
    const auto bytes_read = std::fread(image.pixels.data(), 1, image.pixels.size(), file);
    std::fclose(file);
    if (bytes_read != image.pixels.size()) {
        image = {};
        return false;
    }
    return true;
}

bool read_png(const std::string &path, SourceImage &image)
{
    unsigned char *decoded = nullptr;
    unsigned width         = 0;
    unsigned height        = 0;
    const auto result      = lodepng_decode32_file(&decoded, &width, &height, path.c_str());
    // LVGL's LodePNG returns an owned draw buffer containing the RGBA pixels.
    auto *buffer = reinterpret_cast<lv_draw_buf_t *>(decoded);
    if (result != 0 || !decoded || !source_size_is_valid(width, height, 4)) {
        if (buffer) lv_draw_buf_destroy(buffer);
        return false;
    }

    image.width          = width;
    image.height         = height;
    image.format         = PixelFormat::rgba;
    const auto row_bytes = static_cast<std::size_t>(width) * 4U;
    image.pixels.resize(row_bytes * height);
    for (unsigned row = 0; row < height; ++row) {
        std::memcpy(image.pixels.data() + row * row_bytes, buffer->data + row * buffer->header.stride, row_bytes);
    }
    lv_draw_buf_destroy(buffer);
    return true;
}

#if defined(GALLERY_HAS_LVGL_LIBJPEG_TURBO)
bool read_jpeg(const std::string &path, SourceImage &image)
{
    lv_image_decoder_dsc_t decoder{};
    lv_image_decoder_args_t args{};
    args.no_cache = true;
    if (lv_image_decoder_open(&decoder, path.c_str(), &args) != LV_RESULT_OK || !decoder.decoded) return false;

    const auto *buffer = decoder.decoded;
    const auto valid = buffer->header.cf == LV_COLOR_FORMAT_RGB888 &&
                       source_size_is_valid(buffer->header.w, buffer->header.h, 3);
    if (!valid) {
        lv_image_decoder_close(&decoder);
        return false;
    }

    image.width  = buffer->header.w;
    image.height = buffer->header.h;
    image.format = PixelFormat::rgb;
    const auto row_bytes = static_cast<std::size_t>(image.width) * 3U;
    image.pixels.resize(row_bytes * image.height);
    for (std::uint32_t row = 0; row < image.height; ++row) {
        const auto *source = buffer->data + row * buffer->header.stride;
        auto *target       = image.pixels.data() + row * row_bytes;
        for (std::uint32_t column = 0; column < image.width; ++column) {
            target[column * 3U]     = source[column * 3U + 2U];
            target[column * 3U + 1U] = source[column * 3U + 1U];
            target[column * 3U + 2U] = source[column * 3U];
        }
    }
    lv_image_decoder_close(&decoder);
    return true;
}
#endif

bool load_source(const std::string &path, SourceImage &image)
{
    const auto extension = lowercase_extension(path);
    if (extension == ".rgb565") return read_stored_thumbnail(path, image);
    if (extension == ".png") return read_png(path, image);
    if (extension == ".ppm") return read_ppm(path, image);
#if defined(GALLERY_HAS_LVGL_LIBJPEG_TURBO)
    if (extension == ".jpg" || extension == ".jpeg") return read_jpeg(path, image);
#endif
    return false;
}

void read_pixel(const SourceImage &source, std::uint32_t x, std::uint32_t y, unsigned &red, unsigned &green,
                unsigned &blue)
{
    const auto pixel_index = static_cast<std::size_t>(y) * source.width + x;
    if (source.format == PixelFormat::rgb565) {
        const auto offset = pixel_index * 2U;
        const auto value  = static_cast<std::uint16_t>(source.pixels[offset]) |
                           static_cast<std::uint16_t>(source.pixels[offset + 1]) << 8U;
        red   = ((value >> 11U) & 0x1fU) * 255U / 31U;
        green = ((value >> 5U) & 0x3fU) * 255U / 63U;
        blue  = (value & 0x1fU) * 255U / 31U;
        return;
    }

    const auto components = source.format == PixelFormat::rgba ? 4U : 3U;
    const auto offset     = pixel_index * components;
    red                   = source.pixels[offset];
    green                 = source.pixels[offset + 1];
    blue                  = source.pixels[offset + 2];
    if (components == 4U) {
        const auto alpha      = source.pixels[offset + 3];
        const auto background = components::color_viewer_background;
        red                   = (red * alpha + ((background >> 16U) & 0xffU) * (255U - alpha)) / 255U;
        green                 = (green * alpha + ((background >> 8U) & 0xffU) * (255U - alpha)) / 255U;
        blue                  = (blue * alpha + (background & 0xffU) * (255U - alpha)) / 255U;
    }
}

void scale_image(const SourceImage &source, std::uint32_t target_width, std::uint32_t target_height,
                 GalleryImageFit fit, std::vector<std::uint8_t> &target)
{
    const auto mapping = calculate_mapping(source.width, source.height, target_width, target_height, fit);

    for (std::uint32_t y = 0; y < mapping.draw_height; ++y) {
        const auto mapped_y =
            std::min(source.height - 1,
                     mapping.source_y + static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) *
                                                                   mapping.visible_height / mapping.draw_height));
        for (std::uint32_t x = 0; x < mapping.draw_width; ++x) {
            const auto mapped_x =
                std::min(source.width - 1,
                         mapping.source_x + static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) *
                                                                       mapping.visible_width / mapping.draw_width));
            unsigned red   = 0;
            unsigned green = 0;
            unsigned blue  = 0;
            read_pixel(source, mapped_x, mapped_y, red, green, blue);
            const auto output_index =
                (static_cast<std::size_t>(mapping.offset_y + y) * target_width + mapping.offset_x + x) * 2U;
            write_rgb565(target.data() + output_index, red, green, blue);
        }
    }
}

bool is_start_of_frame_marker(unsigned char marker)
{
    switch (marker) {
        case 0xc0:
        case 0xc1:
        case 0xc2:
        case 0xc3:
        case 0xc5:
        case 0xc6:
        case 0xc7:
        case 0xc9:
        case 0xca:
        case 0xcb:
        case 0xcd:
        case 0xce:
        case 0xcf:
            return true;
        default:
            return false;
    }
}

bool probe_png(FILE *file, std::uint32_t &width, std::uint32_t &height)
{
    constexpr std::array<unsigned char, 8> signature{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    std::array<unsigned char, 33> header{};
    if (std::fread(header.data(), 1, header.size(), file) != header.size() ||
        !std::equal(signature.begin(), signature.end(), header.begin()) ||
        std::memcmp(header.data() + 8, "\0\0\0\x0dIHDR", 8) != 0)
        return false;
    const auto read_u32 = [](const unsigned char *bytes) {
        return (static_cast<std::uint32_t>(bytes[0]) << 24U) | (static_cast<std::uint32_t>(bytes[1]) << 16U) |
               (static_cast<std::uint32_t>(bytes[2]) << 8U) | bytes[3];
    };
    width  = read_u32(header.data() + 16);
    height = read_u32(header.data() + 20);
    return width > 0 && height > 0 && width <= 0x7fffffffU && height <= 0x7fffffffU;
}

bool probe_ppm(FILE *file, std::uint32_t &width, std::uint32_t &height)
{
    std::string magic, width_text, height_text, maximum_text;
    std::uint32_t maximum = 0;
    std::size_t remaining = 64U * 1024U;
    return read_ppm_token(file, magic, &remaining) && read_ppm_token(file, width_text, &remaining) &&
           read_ppm_token(file, height_text, &remaining) && read_ppm_token(file, maximum_text, &remaining) &&
           magic == "P6" && parse_unsigned(width_text, width) &&
           parse_unsigned(height_text, height) && parse_unsigned(maximum_text, maximum) && maximum == 255 &&
           width > 0 && height > 0;
}

bool probe_jpeg(FILE *file, std::uint32_t &width, std::uint32_t &height)
{
    std::uint64_t remaining = kMaximumJpegInputBytes;
    const auto next = [&remaining, file]() {
        if (remaining == 0) return EOF;
        --remaining;
        return std::fgetc(file);
    };
    const int first  = next();
    const int second = next();
    if (first != 0xff || second != 0xd8) return false;

    while (remaining > 0) {
        int prefix = next();
        while (prefix != EOF && prefix != 0xff) prefix = next();
        if (prefix == EOF) return false;
        int marker = next();
        while (marker == 0xff) marker = next();
        if (marker == EOF || marker == 0xd9 || marker == 0xda) return false;
        if (marker == 0x00 || marker == 0xd8 || marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) continue;

        const int length_high = next();
        const int length_low  = next();
        if (length_high == EOF || length_low == EOF) return false;
        const auto length = static_cast<unsigned>((length_high << 8) | length_low);
        if (length < 2 || length - 2 > remaining) return false;
        if (is_start_of_frame_marker(static_cast<unsigned char>(marker))) {
            std::array<unsigned char, 5> frame{};
            if (length < 7 || std::fread(frame.data(), 1, frame.size(), file) != frame.size()) return false;
            height = (static_cast<std::uint32_t>(frame[1]) << 8U) | frame[2];
            width  = (static_cast<std::uint32_t>(frame[3]) << 8U) | frame[4];
            return width > 0 && height > 0;
        }
        if (std::fseek(file, static_cast<long>(length - 2), SEEK_CUR) != 0) return false;
        remaining -= length - 2;
    }
    return false;
}

}  // namespace

bool gallery_probe_image(const std::string &path, std::uint32_t &width, std::uint32_t &height)
{
    width = height = 0;
    const auto extension = lowercase_extension(path);
    auto *file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::uint32_t source_width = 0, source_height = 0;
    bool valid = false;
    if (extension == ".png") valid = probe_png(file, source_width, source_height);
    else if (extension == ".ppm") valid = probe_ppm(file, source_width, source_height);
    else if (extension == ".jpg" || extension == ".jpeg") valid = probe_jpeg(file, source_width, source_height);
    std::fclose(file);
    if (valid) {
        width  = source_width;
        height = source_height;
    }
    return valid;
}

bool gallery_render_image(const std::string &path, std::uint32_t width, std::uint32_t height, GalleryImageFit fit,
                          std::vector<std::uint8_t> &rgb565)
{
    if (width == 0 || height == 0 || static_cast<std::uint64_t>(width) * height > 2U * 1024U * 1024U) return false;
    rgb565.resize(static_cast<std::size_t>(width) * height * 2U);
    fill_rgb565(rgb565, components::color_viewer_background);

    SourceImage source;
    if (!load_source(path, source)) return false;
    scale_image(source, width, height, fit, rgb565);
    return true;
}

bool gallery_is_bounded_baseline_jpeg(const std::string &path)
{
    const auto extension = lowercase_extension(path);
    if (extension != ".jpg" && extension != ".jpeg") return false;

    struct stat information{};
    if (::stat(path.c_str(), &information) != 0 || information.st_size < 0 ||
        static_cast<std::uint64_t>(information.st_size) > kMaximumJpegInputBytes) {
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    std::array<unsigned char, 2> bytes{};
    if (!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())) ||
        bytes[0] != 0xff || bytes[1] != 0xd8) {
        return false;
    }

    const auto read_u16 = [&file](std::uint16_t &value) {
        std::array<unsigned char, 2> encoded{};
        if (!file.read(reinterpret_cast<char *>(encoded.data()), static_cast<std::streamsize>(encoded.size()))) {
            return false;
        }
        value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(encoded[0]) << 8U) | encoded[1]);
        return true;
    };

    for (;;) {
        int prefix = file.get();
        while (prefix != std::char_traits<char>::eof() && prefix != 0xff) prefix = file.get();
        if (prefix == std::char_traits<char>::eof()) return false;

        int marker = file.get();
        while (marker == 0xff) marker = file.get();
        if (marker == std::char_traits<char>::eof() || marker == 0xd9 || marker == 0xda) return false;
        if (marker == 0x00 || marker == 0xd8 || marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) continue;

        std::uint16_t segment_length = 0;
        if (!read_u16(segment_length) || segment_length < 2) return false;
        if (is_start_of_frame_marker(static_cast<unsigned char>(marker))) {
            if (marker != 0xc0 || segment_length < 7) return false;
            std::array<unsigned char, 5> frame{};
            if (!file.read(reinterpret_cast<char *>(frame.data()), static_cast<std::streamsize>(frame.size()))) {
                return false;
            }
            const auto height = static_cast<std::uint32_t>((static_cast<std::uint16_t>(frame[1]) << 8U) | frame[2]);
            const auto width  = static_cast<std::uint32_t>((static_cast<std::uint16_t>(frame[3]) << 8U) | frame[4]);
            return source_size_is_valid(width, height, 3);
        }

        file.seekg(static_cast<std::streamoff>(segment_length - 2U), std::ios::cur);
        if (!file) return false;
    }
}
