#include "app.hpp"

#include "app_identity.h"
#include "pages/gallery/gallery_view.hpp"

#include <cstdlib>
#include <memory>
#include <string>

namespace {

struct AppState {
    std::unique_ptr<GalleryViewModel> gallery_view_model;
    std::unique_ptr<GalleryView> gallery_view;
};

AppState app_state;

void app_open(const cm0_app_context_t *context)
{
    const auto *photo_override        = std::getenv("LILYGO_GALLERY_PHOTO_DIR");
    const auto *screenshot_override   = std::getenv("LILYGO_GALLERY_SCREENSHOT_DIR");
    const auto *sample_override       = std::getenv("LILYGO_GALLERY_SAMPLE_DIR");
    const std::string photo_directory = photo_override && *photo_override ? photo_override : "/root/photos";
    const std::string screenshot_directory =
        screenshot_override && *screenshot_override ? screenshot_override : "/root/screenshots";
    const std::string sample_directory =
        sample_override && *sample_override ? sample_override : "/usr/share/lilygo-ui-gallery/gallery";
    std::string fallback_directory;
#ifdef GALLERY_DEVELOPMENT_SAMPLE_DIR
    if (!sample_override || !*sample_override) fallback_directory = GALLERY_DEVELOPMENT_SAMPLE_DIR;
#endif

    app_state.gallery_view_model =
        std::make_unique<GalleryViewModel>(photo_directory, screenshot_directory, sample_directory, fallback_directory);
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
