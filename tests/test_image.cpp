#include "pages/gallery/gallery_image.hpp"

#include <lvgl.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

int main(int argc, char **argv)
{
    assert(argc == 6);
    lv_init();

    for (int index = 1; index < argc; ++index) {
        std::uint32_t width = 0, height = 0;
        assert(gallery_probe_image(argv[index], width, height));
        assert(width == (index == 1 ? 128U : 320U));
        assert(height == (index == 1 ? 128U : 240U));
    }
    std::uint32_t width = 123, height = 456;
    assert(!gallery_probe_image("/path/does/not/exist.png", width, height));
    assert(width == 0 && height == 0);
    char corrupt_path[] = "/tmp/lilygo-gallery-invalid-XXXXXX.png";
    const int corrupt_file = ::mkstemps(corrupt_path, 4);
    assert(corrupt_file >= 0);
    constexpr char corrupt_data[] = "not a PNG image";
    assert(::write(corrupt_file, corrupt_data, sizeof(corrupt_data)) == static_cast<ssize_t>(sizeof(corrupt_data)));
    assert(::close(corrupt_file) == 0);
    width = 123;
    height = 456;
    assert(!gallery_probe_image(corrupt_path, width, height));
    assert(width == 0 && height == 0);
    assert(std::remove(corrupt_path) == 0);

    // Headers beyond the probe budget must not trigger unbounded scanning or
    // allocation, even when valid dimensions appear after the oversized data.
    for (const auto &invalid : std::vector<std::pair<std::string, std::string>>{
             {".ppm", "P6\n#" + std::string(128U * 1024U, 'x') + "\n320 240\n255\n"},
             {".ppm", "P6\n" + std::string(128U * 1024U, '9') + " 240\n255\n"},
             {".jpg", std::string("\xff\xd8", 2) + std::string(9U * 1024U * 1024U, '\0') +
                          std::string("\xff\xc0\0\x07\x08\0\xf0\x01\x40", 9)}}) {
        std::string path = "/tmp/lilygo-gallery-header-XXXXXX" + invalid.first;
        const int file = ::mkstemps(path.data(), static_cast<int>(invalid.first.size()));
        assert(file >= 0);
        assert(::write(file, invalid.second.data(), invalid.second.size()) ==
               static_cast<ssize_t>(invalid.second.size()));
        assert(::close(file) == 0);
        assert(!gallery_probe_image(path, width, height));
        assert(width == 0 && height == 0);
        assert(std::remove(path.c_str()) == 0);
    }

    std::vector<std::uint8_t> pixels;
    assert(gallery_render_image(argv[1], 96, 72, GalleryImageFit::cover, pixels));
    assert(pixels.size() == 96U * 72U * 2U);
    assert(std::any_of(pixels.begin(), pixels.end(), [](std::uint8_t value) { return value != 0; }));

    assert(!gallery_render_image("/path/does/not/exist.png", 40, 30, GalleryImageFit::contain, pixels));
    assert(pixels.size() == 40U * 30U * 2U);
    const auto background_low  = pixels[0];
    const auto background_high = pixels[1];
    assert(gallery_render_image(argv[1], 128, 128, GalleryImageFit::contain, pixels));
    // The app icon's transparent corner should use the same light background.
    assert(pixels[0] == background_low && pixels[1] == background_high);
    assert(!gallery_render_image(argv[1], 0, 30, GalleryImageFit::contain, pixels));
    assert(gallery_is_bounded_baseline_jpeg(argv[2]));
    assert(!gallery_is_bounded_baseline_jpeg(argv[1]));
    assert(gallery_render_image(argv[2], 96, 72, GalleryImageFit::contain, pixels));
    assert(pixels.size() == 96U * 72U * 2U);
    assert(std::any_of(pixels.begin(), pixels.end(), [](std::uint8_t value) { return value != 0; }));
    const auto sample_offset = (24U * 96U + 24U) * 2U;
    const auto sample        = static_cast<std::uint16_t>(pixels[sample_offset]) |
                        static_cast<std::uint16_t>(pixels[sample_offset + 1U]) << 8U;
    const auto red   = (sample >> 11U) & 0x1fU;
    const auto green = (sample >> 5U) & 0x3fU;
    const auto blue  = sample & 0x1fU;
    assert(green > red && green > blue);

    // The legacy TJPGD compatibility check rejects progressive JPEGs. They must
    // still render successfully through LVGL's libjpeg-turbo decoder.
    assert(!gallery_is_bounded_baseline_jpeg(argv[5]));
    assert(gallery_render_image(argv[5], 96, 72, GalleryImageFit::contain, pixels));
    assert(std::any_of(pixels.begin(), pixels.end(), [](std::uint8_t value) { return value != 0; }));

    // These lossless samples have identical pixels; compare the full decoded
    // output to catch corrupt rows and incorrect channel order in PNG loading.
    std::vector<std::uint8_t> png_pixels, ppm_pixels;
    for (const auto fit : {GalleryImageFit::cover, GalleryImageFit::contain}) {
        assert(gallery_render_image(argv[3], 189, 188, fit, png_pixels));
        assert(gallery_render_image(argv[4], 189, 188, fit, ppm_pixels));
        assert(png_pixels == ppm_pixels);
    }

    lv_deinit();
    return 0;
}
