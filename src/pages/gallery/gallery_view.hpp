#ifndef LILYGO_UI_GALLERY_PAGES_GALLERY_GALLERY_VIEW_HPP
#define LILYGO_UI_GALLERY_PAGES_GALLERY_GALLERY_VIEW_HPP

#include "pages/gallery/gallery_view_model.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class GalleryView {
public:
    explicit GalleryView(GalleryViewModel &view_model) noexcept;

    lv_obj_t *create(lv_obj_t *parent, const char *application_name);
    void destroy() noexcept;

private:
    struct ThumbnailWidget {
        GalleryView *owner = nullptr;
        std::size_t index  = 0;
        lv_obj_t *tile     = nullptr;
        lv_obj_t *canvas   = nullptr;
        std::vector<std::uint8_t> pixels;
    };
    struct AlbumWidget {
        GalleryView *owner                      = nullptr;
        GalleryViewModel::Collection collection = GalleryViewModel::Collection::all;
        std::vector<std::uint8_t> pixels;
    };

    static void size_changed_event(lv_event_t *event);
    static void content_size_changed_event(lv_event_t *event);
    static void filmstrip_layout_event(lv_event_t *event);
    static void layout_completed_event(lv_event_t *event);
    static void reload_event(lv_event_t *event);
    static void thumbnail_event(lv_event_t *event);
    static void library_event(lv_event_t *event);
    static void albums_event(lv_event_t *event);
    static void album_event(lv_event_t *event);
    static void grid_event(lv_event_t *event);
    static void previous_event(lv_event_t *event);
    static void next_event(lv_event_t *event);
    static void zoom_in_event(lv_event_t *event);
    static void zoom_out_event(lv_event_t *event);
    static void details_event(lv_event_t *event);
    static void delete_event(lv_event_t *event);
    static void cancel_delete_event(lv_event_t *event);
    static void confirm_delete_event(lv_event_t *event);
    static void close_details_event(lv_event_t *event);
    static void swipe_event(lv_event_t *event);

    void create_header_actions();
    void create_tabs();
    void create_viewer();
    void create_delete_confirmation();
    void rebuild_grid();
    void rebuild_albums();
    void rebuild_filmstrip();
    void resize_grid();
    void resize_albums();
    void show_grid();
    void show_albums();
    void show_viewer();
    void render_viewer();
    void set_viewer_zoom(float zoom);
    void position_viewer_canvas();
    void set_details_visible(bool visible);
    void update_header() noexcept;
    void update_navigation() noexcept;
    void apply_layout();

    GalleryViewModel &view_model_;
    lv_display_t *display_         = nullptr;
    lv_obj_t *surface_             = nullptr;
    lv_obj_t *header_              = nullptr;
    lv_obj_t *header_title_        = nullptr;
    lv_obj_t *header_count_        = nullptr;
    lv_obj_t *body_                = nullptr;
    lv_obj_t *grid_                = nullptr;
    lv_obj_t *albums_              = nullptr;
    lv_obj_t *tab_bar_             = nullptr;
    lv_obj_t *library_tab_         = nullptr;
    lv_obj_t *albums_tab_          = nullptr;
    lv_obj_t *viewer_              = nullptr;
    lv_obj_t *viewer_top_bar_      = nullptr;
    lv_obj_t *viewer_canvas_       = nullptr;
    lv_obj_t *viewer_date_         = nullptr;
    lv_obj_t *viewer_filename_     = nullptr;
    lv_obj_t *viewer_status_        = nullptr;
    lv_obj_t *viewer_zoom_controls_ = nullptr;
    lv_obj_t *viewer_zoom_label_    = nullptr;
    lv_obj_t *viewer_controls_      = nullptr;
    lv_obj_t *viewer_details_       = nullptr;
    lv_obj_t *viewer_details_body_  = nullptr;
    lv_obj_t *viewer_detail_text_   = nullptr;
    lv_obj_t *viewer_index_         = nullptr;
    lv_obj_t *filmstrip_            = nullptr;
    lv_obj_t *previous_button_      = nullptr;
    lv_obj_t *next_button_          = nullptr;
    lv_obj_t *delete_button_        = nullptr;
    lv_obj_t *delete_confirmation_  = nullptr;
    lv_obj_t *delete_error_         = nullptr;
    std::vector<ThumbnailWidget> thumbnails_;
    std::vector<ThumbnailWidget> filmstrip_thumbnails_;
    std::array<AlbumWidget, 4> album_widgets_{};
    std::vector<std::uint8_t> viewer_pixels_;
    std::string viewer_item_path_;
    lv_point_t press_point_{};
    std::int32_t pan_start_x_    = 0;
    std::int32_t pan_start_y_    = 0;
    std::int32_t viewer_pan_x_   = 0;
    std::int32_t viewer_pan_y_   = 0;
    float viewer_zoom_           = 1.0F;
    std::int32_t layout_width_    = -1;
    std::int32_t layout_height_   = -1;
    std::int32_t grid_scroll_y_   = 0;
    std::int32_t albums_scroll_y_ = 0;
    bool restore_scroll_          = false;
    bool showing_albums_          = false;
    bool albums_tab_active_       = false;
    bool applying_layout_         = false;
    bool swipe_pressed_           = false;
    bool pan_active_              = false;
    bool details_visible_         = false;
};

#endif
