#ifndef LILYGO_UI_GALLERY_PAGES_GALLERY_GALLERY_VIEW_HPP
#define LILYGO_UI_GALLERY_PAGES_GALLERY_GALLERY_VIEW_HPP

#include "pages/gallery/gallery_view_model.hpp"
#include "services/video_player.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
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
        lv_obj_t *play_icon = nullptr;
        std::vector<std::uint8_t> pixels;
        std::int32_t width  = 0;
        std::int32_t height = 0;
        bool rendered = false;
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
    static void video_toggle_event(lv_event_t *event);
    static void video_seek_event(lv_event_t *event);
    static void video_volume_event(lv_event_t *event);
    static void video_fullscreen_event(lv_event_t *event);
    static void playback_timer_event(lv_timer_t *timer);
    static void deferred_thumbnail_timer_event(lv_timer_t *timer);
    static void swipe_event(lv_event_t *event);

    void create_header_actions();
    void create_tabs();
    void create_viewer();
    void create_delete_confirmation();
    void rebuild_grid();
    void rebuild_albums();
    void rebuild_filmstrip();
    void resize_grid();
    void render_grid_thumbnail(ThumbnailWidget &thumbnail, std::int32_t width, std::int32_t height);
    void cancel_deferred_thumbnail_render() noexcept;
    void schedule_deferred_thumbnail_render();
    void render_deferred_thumbnail_batch();
    void resize_albums();
    void show_grid();
    void show_albums();
    void show_viewer();
    void render_viewer();
    bool render_item_thumbnail(const GalleryItem &item, std::int32_t width, std::int32_t height,
                               std::vector<std::uint8_t> &pixels);
    void set_viewer_zoom(float zoom);
    void position_viewer_canvas();
    void set_details_visible(bool visible);
    void update_details(const GalleryItem &item);
    void stop_video() noexcept;
    void render_video_frame();
    void update_video_controls() noexcept;
    void playback_tick();
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
    lv_obj_t *video_toggle_button_  = nullptr;
    lv_obj_t *video_toggle_icon_    = nullptr;
    lv_obj_t *video_timeline_       = nullptr;
    lv_obj_t *video_position_       = nullptr;
    lv_obj_t *video_seek_slider_    = nullptr;
    lv_obj_t *video_duration_       = nullptr;
    lv_obj_t *viewer_actions_       = nullptr;
    lv_obj_t *video_previous_button_ = nullptr;
    lv_obj_t *video_next_button_     = nullptr;
    lv_obj_t *video_details_button_ = nullptr;
    lv_obj_t *video_delete_button_  = nullptr;
    lv_obj_t *video_volume_icon_    = nullptr;
    lv_obj_t *video_volume_slider_  = nullptr;
    lv_obj_t *video_fullscreen_button_ = nullptr;
    lv_obj_t *video_fullscreen_icon_ = nullptr;
    lv_obj_t *viewer_zoom_controls_ = nullptr;
    lv_obj_t *viewer_zoom_label_    = nullptr;
    lv_obj_t *viewer_controls_      = nullptr;
    lv_obj_t *viewer_details_       = nullptr;
    lv_obj_t *viewer_details_title_ = nullptr;
    lv_obj_t *viewer_details_body_  = nullptr;
    lv_obj_t *viewer_detail_text_   = nullptr;
    lv_obj_t *viewer_index_         = nullptr;
    lv_obj_t *filmstrip_            = nullptr;
    lv_obj_t *previous_button_      = nullptr;
    lv_obj_t *next_button_          = nullptr;
    lv_obj_t *delete_button_        = nullptr;
    lv_obj_t *delete_confirmation_  = nullptr;
    lv_obj_t *delete_error_         = nullptr;
    lv_timer_t *playback_timer_     = nullptr;
    lv_timer_t *thumbnail_timer_    = nullptr;
    std::size_t deferred_thumbnail_index_ = 0;
    std::vector<ThumbnailWidget> thumbnails_;
    std::vector<ThumbnailWidget> filmstrip_thumbnails_;
    std::array<AlbumWidget, 4> album_widgets_{};
    std::vector<std::uint8_t> viewer_pixels_;
    std::vector<std::uint8_t> video_bgr_frame_;
    std::vector<std::uint8_t> video_frame_;
    std::unique_ptr<GalleryVideoPlayer> video_player_;
    std::unique_ptr<GalleryVideoPlayer> thumbnail_player_;
    std::unordered_set<std::string> video_thumbnail_failures_;
    GalleryVideoPlayback video_playback_{};
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
    bool video_fullscreen_        = false;
};

#endif
