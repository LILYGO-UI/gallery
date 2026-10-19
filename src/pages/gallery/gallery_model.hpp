#ifndef LILYGO_UI_GALLERY_PAGES_GALLERY_GALLERY_MODEL_HPP
#define LILYGO_UI_GALLERY_PAGES_GALLERY_GALLERY_MODEL_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class GallerySource { camera, pictures, screenshots, samples };

// Returns the camera directory used when no explicit gallery directory is
// configured. The camera application stores captures in this same location.
[[nodiscard]] std::string gallery_default_photo_directory();

// Returns the screenshot directory used when no explicit gallery directory is
// configured. Screenshot capture stores images in this same location.
[[nodiscard]] std::string gallery_default_screenshot_directory();

// Returns the user's writable picture directory used for imported gallery
// samples and photos that are not camera captures.
[[nodiscard]] std::string gallery_default_pictures_directory();

struct GalleryItem {
    std::string photo_path;
    std::string thumbnail_path;
    std::string filename;
    std::int64_t modified_time = 0;
    bool read_only             = false;
    GallerySource source       = GallerySource::camera;
    std::uint64_t size_bytes   = 0;
};

class GalleryModel {
public:
    static constexpr std::size_t maximum_items = 24;

    explicit GalleryModel(std::string photo_directory      = {},
                          std::string screenshot_directory = {}, std::string sample_directory = {},
                          std::string fallback_directory = {}, std::string pictures_directory = {});

    void reload();
    [[nodiscard]] const std::vector<GalleryItem> &items() const noexcept;
    [[nodiscard]] std::size_t selected_index() const noexcept;
    [[nodiscard]] const GalleryItem *selected_item() const noexcept;
    [[nodiscard]] bool select(std::size_t index) noexcept;
    [[nodiscard]] bool select_previous() noexcept;
    [[nodiscard]] bool select_next() noexcept;
    [[nodiscard]] bool remove_selected() noexcept;

private:
    void scan_directory(const std::string &directory, GallerySource source, bool read_only);

    std::string photo_directory_;
    std::string screenshot_directory_;
    std::string sample_directory_;
    std::string fallback_directory_;
    std::string pictures_directory_;
    std::vector<GalleryItem> items_;
    std::size_t selected_index_ = 0;
};

#endif
