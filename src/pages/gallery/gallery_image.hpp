#ifndef LILYGO_UI_GALLERY_PAGES_GALLERY_GALLERY_IMAGE_HPP
#define LILYGO_UI_GALLERY_PAGES_GALLERY_GALLERY_IMAGE_HPP

#include <cstdint>
#include <string>
#include <vector>

enum class GalleryImageFit { cover, contain };

// Read the original dimensions from the file header without decoding pixels.
// Both dimensions are zero when the header is unavailable or invalid.
[[nodiscard]] bool gallery_probe_image(const std::string &path, std::uint32_t &width, std::uint32_t &height);
[[nodiscard]] bool gallery_render_image(const std::string &path, std::uint32_t width, std::uint32_t height,
                                        GalleryImageFit fit, std::vector<std::uint8_t> &rgb565);
[[nodiscard]] bool gallery_is_bounded_baseline_jpeg(const std::string &path);

#endif
