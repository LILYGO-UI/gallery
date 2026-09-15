#include "pages/gallery/gallery_image.hpp"

#include "components/components.hpp"

#define LODEPNG_NO_COMPILE_CPP
#include <src/libs/lodepng/lodepng.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <csetjmp>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <sys/stat.h>

#if defined(GALLERY_HAS_JPEG)
#include <jpeglib.h>
#elif defined(GALLERY_HAS_LVGL_TJPGD)
#include <src/libs/tjpgd/tjpgd.h>
#endif

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

bool read_ppm_token(FILE *file, std::string &token)
{
    token.clear();
    int value = 0;
    while ((value = std::fgetc(file)) != EOF) {
        if (value == '#') {
            while ((value = std::fgetc(file)) != EOF && value != '\n') {
            }
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(value))) break;
    }
    if (value == EOF) return false;

    do {
        token.push_back(static_cast<char>(value));
        value = std::fgetc(file);
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

#if defined(GALLERY_HAS_JPEG)
struct JpegErrorManager {
    jpeg_error_mgr base{};
    std::jmp_buf jump{};
};

void jpeg_error_exit(j_common_ptr decoder)
{
    auto *error = reinterpret_cast<JpegErrorManager *>(decoder->err);
    std::longjmp(error->jump, 1);
}

bool read_jpeg(const std::string &path, SourceImage &image)
{
    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return false;

    jpeg_decompress_struct decoder{};
    JpegErrorManager error{};
    decoder.err                    = jpeg_std_error(&error.base);
    error.base.error_exit          = jpeg_error_exit;
    volatile bool decoder_created  = false;
    unsigned char *volatile pixels = nullptr;

    if (setjmp(error.jump) != 0) {
        std::free(const_cast<unsigned char *>(pixels));
        if (decoder_created) jpeg_destroy_decompress(&decoder);
        std::fclose(file);
        return false;
    }

    jpeg_create_decompress(&decoder);
    decoder_created = true;
    jpeg_stdio_src(&decoder, file);
    jpeg_read_header(&decoder, TRUE);
    decoder.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decoder);
    if (decoder.output_components != 3 || !source_size_is_valid(decoder.output_width, decoder.output_height, 3)) {
        jpeg_destroy_decompress(&decoder);
        std::fclose(file);
        return false;
    }

    const auto byte_count = static_cast<std::size_t>(decoder.output_width) * decoder.output_height * 3U;
    pixels                = static_cast<unsigned char *>(std::malloc(byte_count));
    if (!pixels) {
        jpeg_destroy_decompress(&decoder);
        std::fclose(file);
        return false;
    }
    while (decoder.output_scanline < decoder.output_height) {
        auto *row = const_cast<unsigned char *>(pixels) +
                    static_cast<std::size_t>(decoder.output_scanline) * decoder.output_width * 3U;
        JSAMPROW rows[] = {row};
        if (jpeg_read_scanlines(&decoder, rows, 1) != 1) {
            std::free(const_cast<unsigned char *>(pixels));
            jpeg_destroy_decompress(&decoder);
            std::fclose(file);
            return false;
        }
    }
    jpeg_finish_decompress(&decoder);

    image.width  = decoder.output_width;
    image.height = decoder.output_height;
    image.format = PixelFormat::rgb;
    image.pixels.assign(const_cast<unsigned char *>(pixels), const_cast<unsigned char *>(pixels) + byte_count);
    std::free(const_cast<unsigned char *>(pixels));
    jpeg_destroy_decompress(&decoder);
    std::fclose(file);
    return true;
}
#endif

#if defined(GALLERY_HAS_LVGL_TJPGD)
static_assert(JD_FORMAT == 0, "Gallery's TJPGD fallback requires LVGL's BGR888 output");

struct TjpgdRenderContext {
    FILE *file                 = nullptr;
    std::uint32_t target_width = 0;
    RenderMapping mapping{};
    std::vector<std::uint8_t> *target = nullptr;
};

std::size_t tjpgd_read(JDEC *decoder, std::uint8_t *buffer, std::size_t byte_count)
{
    auto *context = static_cast<TjpgdRenderContext *>(decoder->device);
    if (buffer) return std::fread(buffer, 1, byte_count, context->file);
    return std::fseek(context->file, static_cast<long>(byte_count), SEEK_CUR) == 0 ? byte_count : 0;
}

std::uint64_t divide_rounding_up(std::uint64_t numerator, std::uint64_t denominator)
{
    return (numerator + denominator - 1U) / denominator;
}

int tjpgd_write(JDEC *decoder, void *bitmap, JRECT *rectangle)
{
    auto *context       = static_cast<TjpgdRenderContext *>(decoder->device);
    const auto &mapping = context->mapping;
    const auto left     = std::max<std::uint32_t>(rectangle->left, mapping.source_x);
    const auto right    = std::min<std::uint32_t>(rectangle->right, mapping.source_x + mapping.visible_width - 1U);
    const auto top      = std::max<std::uint32_t>(rectangle->top, mapping.source_y);
    const auto bottom   = std::min<std::uint32_t>(rectangle->bottom, mapping.source_y + mapping.visible_height - 1U);
    if (left > right || top > bottom) return 1;

    const auto destination_x_begin = static_cast<std::uint32_t>(divide_rounding_up(
        static_cast<std::uint64_t>(left - mapping.source_x) * mapping.draw_width, mapping.visible_width));
    const auto destination_x_end   = static_cast<std::uint32_t>(divide_rounding_up(
        static_cast<std::uint64_t>(right - mapping.source_x + 1U) * mapping.draw_width, mapping.visible_width));
    const auto destination_y_begin = static_cast<std::uint32_t>(divide_rounding_up(
        static_cast<std::uint64_t>(top - mapping.source_y) * mapping.draw_height, mapping.visible_height));
    const auto destination_y_end   = static_cast<std::uint32_t>(divide_rounding_up(
        static_cast<std::uint64_t>(bottom - mapping.source_y + 1U) * mapping.draw_height, mapping.visible_height));
    const auto block_width         = static_cast<std::uint32_t>(rectangle->right - rectangle->left + 1U);
    const auto *source             = static_cast<const std::uint8_t *>(bitmap);

    for (auto destination_y = destination_y_begin; destination_y < destination_y_end; ++destination_y) {
        const auto source_y =
            mapping.source_y + static_cast<std::uint32_t>(static_cast<std::uint64_t>(destination_y) *
                                                          mapping.visible_height / mapping.draw_height);
        for (auto destination_x = destination_x_begin; destination_x < destination_x_end; ++destination_x) {
            const auto source_x =
                mapping.source_x + static_cast<std::uint32_t>(static_cast<std::uint64_t>(destination_x) *
                                                              mapping.visible_width / mapping.draw_width);
            const auto source_offset =
                (static_cast<std::size_t>(source_y - rectangle->top) * block_width + source_x - rectangle->left) * 3U;
            const auto output_offset =
                (static_cast<std::size_t>(mapping.offset_y + destination_y) * context->target_width + mapping.offset_x +
                 destination_x) *
                2U;
            write_rgb565(context->target->data() + output_offset, source[source_offset + 2U],
                         source[source_offset + 1U], source[source_offset]);
        }
    }
    return 1;
}

bool render_jpeg_with_tjpgd(const std::string &path, std::uint32_t target_width, std::uint32_t target_height,
                            GalleryImageFit fit, std::vector<std::uint8_t> &target)
{
    if (!gallery_is_bounded_baseline_jpeg(path)) return false;

    auto *file = std::fopen(path.c_str(), "rb");
    if (!file) return false;

    std::array<std::uint8_t, 4096> workspace{};
    JDEC decoder{};
    TjpgdRenderContext context{};
    context.file         = file;
    context.target_width = target_width;
    context.target       = &target;

    const auto prepared = jd_prepare(&decoder, tjpgd_read, workspace.data(), workspace.size(), &context);
    if (prepared != JDR_OK || !source_size_is_valid(decoder.width, decoder.height, 3)) {
        std::fclose(file);
        return false;
    }

    context.mapping   = calculate_mapping(decoder.width, decoder.height, target_width, target_height, fit);
    const auto result = jd_decomp(&decoder, tjpgd_write, 0);
    std::fclose(file);
    return result == JDR_OK;
}
#endif

bool load_source(const std::string &path, SourceImage &image)
{
    const auto extension = lowercase_extension(path);
    if (extension == ".rgb565") return read_stored_thumbnail(path, image);
    if (extension == ".png") return read_png(path, image);
    if (extension == ".ppm") return read_ppm(path, image);
#if defined(GALLERY_HAS_JPEG)
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

}  // namespace

bool gallery_render_image(const std::string &path, std::uint32_t width, std::uint32_t height, GalleryImageFit fit,
                          std::vector<std::uint8_t> &rgb565)
{
    if (width == 0 || height == 0 || static_cast<std::uint64_t>(width) * height > 2U * 1024U * 1024U) return false;
    rgb565.resize(static_cast<std::size_t>(width) * height * 2U);
    fill_rgb565(rgb565, components::color_viewer_background);

#if defined(GALLERY_HAS_LVGL_TJPGD)
    const auto extension = lowercase_extension(path);
    if (extension == ".jpg" || extension == ".jpeg") {
        if (render_jpeg_with_tjpgd(path, width, height, fit, rgb565)) return true;
        fill_rgb565(rgb565, components::color_viewer_background);
        return false;
    }
#endif

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
