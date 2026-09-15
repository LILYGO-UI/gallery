#include "pages/gallery/gallery_image.hpp"

#include <lvgl.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

int main(int argc, char **argv)
{
    assert(argc == 5);
    lv_init();

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
