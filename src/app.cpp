#include "app.hpp"

#include "app_identity.h"
#include "pages/gallery/gallery_view.hpp"

#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace {

struct AppState {
    std::unique_ptr<GalleryViewModel> gallery_view_model;
    std::unique_ptr<GalleryView> gallery_view;
};

AppState app_state;

bool is_importable_gallery_asset(const std::filesystem::path &path)
{
    auto extension = path.extension().string();
    for (auto &character : extension) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".ppm";
}

void import_packaged_samples(const std::string &pictures_directory)
{
    if (pictures_directory.empty()) return;

    const std::filesystem::path source_directory = "/usr/share/lilygo-ui-gallery/gallery";
    const std::filesystem::path target_directory = pictures_directory;
    const auto marker = target_directory / ".lilygo-ui-gallery-samples-imported";
    std::error_code error;
    if (!std::filesystem::is_directory(source_directory, error) || std::filesystem::exists(marker, error)) return;
    error.clear();
    std::filesystem::create_directories(target_directory, error);
    if (error) return;

    bool found_asset = false;
    for (const auto &entry : std::filesystem::directory_iterator(source_directory, error)) {
        if (error) break;
        if (!entry.is_regular_file(error) || !is_importable_gallery_asset(entry.path())) continue;
        found_asset = true;
        error.clear();
        std::filesystem::copy_file(entry.path(), target_directory / entry.path().filename(),
                                   std::filesystem::copy_options::skip_existing, error);
    }
    if (!found_asset || error) return;

    std::ofstream marker_file(marker);
    marker_file << "Imported by lilygo-ui-gallery\n";
}

void app_open(const cm0_app_context_t *context)
{
    const auto *photo_override        = std::getenv("LILYGO_GALLERY_PHOTO_DIR");
    const auto *screenshot_override   = std::getenv("LILYGO_GALLERY_SCREENSHOT_DIR");
    const auto *pictures_override     = std::getenv("LILYGO_GALLERY_PICTURES_DIR");
    const auto *sample_override       = std::getenv("LILYGO_GALLERY_SAMPLE_DIR");
    const std::string photo_directory =
        photo_override && *photo_override ? photo_override : gallery_default_photo_directory();
    const std::string screenshot_directory =
        screenshot_override && *screenshot_override ? screenshot_override : gallery_default_screenshot_directory();
    const std::string pictures_directory =
        pictures_override ? (pictures_override[0] ? pictures_override : "") : gallery_default_pictures_directory();
    const std::string sample_directory = sample_override && *sample_override ? sample_override : "";
    std::string fallback_directory;
#ifdef GALLERY_DEVELOPMENT_SAMPLE_DIR
    if (!sample_override || !*sample_override) fallback_directory = GALLERY_DEVELOPMENT_SAMPLE_DIR;
#endif

    if (!sample_override || !*sample_override) import_packaged_samples(pictures_directory);
    app_state.gallery_view_model =
        std::make_unique<GalleryViewModel>(photo_directory, screenshot_directory, sample_directory, fallback_directory,
                                           pictures_directory);
    app_state.gallery_view = std::make_unique<GalleryView>(*app_state.gallery_view_model);
    app_state.gallery_view->create(context->root, LILYGO_UI_GALLERY_APP_NAME);
}

void app_close()
{
    if (app_state.gallery_view) app_state.gallery_view->destroy();
    app_state.gallery_view.reset();
    app_state.gallery_view_model.reset();
}

const cm0_app_descriptor_t app_descriptor{
    CM0_APP_API_VERSION, sizeof(cm0_app_descriptor_t), LILYGO_UI_GALLERY_APP_ID, app_open, app_close,
};

}  // namespace

extern "C" const cm0_app_descriptor_t *cm0_app_get_descriptor()
{
    return &app_descriptor;
}
