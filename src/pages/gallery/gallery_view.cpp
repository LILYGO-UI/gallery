#include "pages/gallery/gallery_view.hpp"

#include "components/components.hpp"
#include "pages/gallery/gallery_image.hpp"

#include <cm0/input.h>
#include <cm0/typography.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>

namespace {

constexpr std::int32_t kGridGap            = 2;
constexpr std::int32_t kViewerHeaderHeight = 76;
constexpr std::int32_t kFilmstripHeight    = 60;
constexpr std::int32_t kControlBarHeight   = 64;
constexpr std::int32_t kAlbumGap           = 20;
using Collection                           = GalleryViewModel::Collection;

void set_font(lv_obj_t *object, std::uint32_t size)
{
    if (const auto *font = lilygo_ui_font_get(size)) lv_obj_set_style_text_font(object, font, 0);
}

void style_transparent_container(lv_obj_t *object)
{
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_radius(object, 0, 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_pad_gap(object, 0, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *create_label(lv_obj_t *parent, const char *text, std::uint32_t size, std::uint32_t color)
{
    auto *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    set_font(label, size);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

lv_obj_t *create_button(lv_obj_t *parent, const char *text, std::uint32_t foreground, lv_event_cb_t callback,
                        void *user_data, const char *name)
{
    auto *button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_name_static(button, name);
    style_transparent_container(button);
    lv_obj_set_size(button, 48, 48);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(components::color_control_background), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_opa(button, LV_OPA_30, LV_STATE_DISABLED);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);
    if (text && *text) lv_obj_center(create_label(button, text, 22, foreground));
    return button;
}

// Small outline icons use LVGL primitives so they stay crisp without private fonts.
lv_obj_t *create_photo_icon(lv_obj_t *parent, bool album)
{
    auto *icon = lv_obj_create(parent);
    style_transparent_container(icon);
    lv_obj_set_size(icon, 28, 26);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    if (album) {
        auto *back = lv_obj_create(icon);
        style_transparent_container(back);
        lv_obj_set_size(back, 22, 18);
        lv_obj_set_pos(back, 3, 0);
        lv_obj_set_style_radius(back, 3, 0);
        lv_obj_set_style_border_width(back, 2, 0);
        lv_obj_set_style_border_color(back, lv_color_hex(components::color_muted_text), 0);
        lv_obj_clear_flag(back, LV_OBJ_FLAG_CLICKABLE);
    }
    auto *frame = lv_obj_create(icon);
    style_transparent_container(frame);
    lv_obj_set_size(frame, 28, 22);
    lv_obj_align(frame, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(frame, 4, 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(components::color_muted_text), 0);
    lv_obj_set_style_bg_color(frame, lv_color_hex(components::color_page_background), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE);
    static const lv_point_precise_t mountains[] = {{2, 15}, {9, 8}, {14, 13}, {18, 9}, {23, 15}};
    auto *line                                  = lv_line_create(frame);
    lv_line_set_points(line, mountains, 5);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(components::color_muted_text), 0);
    return icon;
}

void tint_tab(lv_obj_t *tab, bool selected)
{
    const auto color = lv_color_hex(selected ? components::color_navigation_accent : components::color_muted_text);
    // Icon shape and label are decorative children, all sharing the tab's state.
    auto *icon = lv_obj_get_child(tab, 0);
    for (std::uint32_t i = 0; i < lv_obj_get_child_count(icon); ++i) {
        auto *shape = lv_obj_get_child(icon, i);
        lv_obj_set_style_border_color(shape, color, 0);
        for (std::uint32_t j = 0; j < lv_obj_get_child_count(shape); ++j) {
            lv_obj_set_style_line_color(lv_obj_get_child(shape, j), color, 0);
        }
    }
    lv_obj_set_style_text_color(lv_obj_get_child(tab, 1), color, 0);
    if (selected)
        lv_obj_add_state(tab, LV_STATE_CHECKED);
    else
        lv_obj_remove_state(tab, LV_STATE_CHECKED);
}

const char *collection_title(Collection collection)
{
    switch (collection) {
        case Collection::camera:
            return "Camera";
        case Collection::screenshots:
            return "Screenshots";
        case Collection::samples:
            return "Samples";
        default:
            return "All Photos";
    }
}

bool belongs_to(const GalleryItem &item, Collection collection)
{
    return collection == Collection::all ||
           (collection == Collection::camera && item.source == GallerySource::camera) ||
           (collection == Collection::screenshots && item.source == GallerySource::screenshots) ||
           (collection == Collection::samples && item.source == GallerySource::samples);
}

std::string photo_date(std::int64_t timestamp, bool include_time = false)
{
    const auto time = static_cast<std::time_t>(timestamp);
    std::tm calendar{};
    if (!localtime_r(&time, &calendar)) return "Photo";
    char text[64]{};
    if (!std::strftime(text, sizeof(text), include_time ? "%b %e, %Y at %H:%M:%S" : "%b %e, %Y", &calendar))
        return "Photo";
    return text;
}

void set_hidden(lv_obj_t *object, bool hidden)
{
    if (hidden)
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

GalleryView::GalleryView(GalleryViewModel &view_model) noexcept : view_model_(view_model)
{
}

lv_obj_t *GalleryView::create(lv_obj_t *parent, const char *application_name)
{
    (void)application_name;
    surface_      = components::create_page(parent);
    header_       = components::create_header(surface_, "Library");
    header_title_ = lv_obj_find_by_name(header_, "app_title");
    create_header_actions();

    body_ = lv_obj_create(surface_);
    lv_obj_set_name_static(body_, "gallery_body");
    style_transparent_container(body_);
    lv_obj_set_width(body_, LV_PCT(100));
    lv_obj_set_flex_grow(body_, 1);

    grid_ = lv_obj_create(body_);
    lv_obj_set_name_static(grid_, "gallery_grid");
    style_transparent_container(grid_);
    lv_obj_set_size(grid_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_gap(grid_, kGridGap, 0);
    lv_obj_add_flag(grid_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(grid_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(grid_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_flex_flow(grid_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_event_cb(grid_, content_size_changed_event, LV_EVENT_SIZE_CHANGED, this);

    albums_ = lv_obj_create(body_);
    lv_obj_set_name_static(albums_, "gallery_albums");
    style_transparent_container(albums_);
    lv_obj_set_size(albums_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(albums_, 24, 0);
    lv_obj_set_style_pad_top(albums_, 4, 0);
    lv_obj_set_style_pad_gap(albums_, kAlbumGap, 0);
    lv_obj_add_flag(albums_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(albums_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_scroll_dir(albums_, LV_DIR_VER);
    lv_obj_set_flex_flow(albums_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(albums_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_event_cb(albums_, content_size_changed_event, LV_EVENT_SIZE_CHANGED, this);

    create_tabs();
    create_viewer();
    create_delete_confirmation();
    display_ = lv_obj_get_display(surface_);
    lv_display_add_event_cb(display_, layout_completed_event, LV_EVENT_UPDATE_LAYOUT_COMPLETED, this);
    lv_obj_add_event_cb(surface_, size_changed_event, LV_EVENT_SIZE_CHANGED, this);
    lv_obj_update_layout(surface_);
    apply_layout();
    rebuild_grid();
    show_grid();
    return surface_;
}

void GalleryView::destroy() noexcept
{
    if (display_) lv_display_remove_event_cb_with_user_data(display_, layout_completed_event, this);
    display_ = nullptr;
    if (surface_) lv_obj_delete(surface_);
    thumbnails_.clear();
    filmstrip_thumbnails_.clear();
    album_widgets_ = {};
    viewer_pixels_.clear();
    surface_ = header_ = header_title_ = header_count_ = body_ = grid_ = albums_ = nullptr;
    tab_bar_ = library_tab_ = albums_tab_ = nullptr;
    viewer_ = viewer_top_bar_ = viewer_canvas_ = viewer_date_ = viewer_filename_ = viewer_status_ = nullptr;
    viewer_controls_ = viewer_details_ = viewer_details_body_ = viewer_detail_text_ = nullptr;
    viewer_index_ = filmstrip_ = previous_button_ = next_button_ = delete_button_ = nullptr;
    delete_confirmation_ = delete_error_ = nullptr;
    layout_width_ = layout_height_ = -1;
    grid_scroll_y_ = albums_scroll_y_ = 0;
    restore_scroll_                   = false;
    showing_albums_ = albums_tab_active_ = applying_layout_ = swipe_pressed_ = details_visible_ = false;
}

void GalleryView::create_header_actions()
{
    lv_obj_set_width(header_title_, LV_PCT(75));
    lv_label_set_long_mode(header_title_, LV_LABEL_LONG_DOT);
    header_count_ = create_label(header_, "", 14, components::color_muted_text);
    lv_obj_set_name_static(header_count_, "gallery_count");
    lv_obj_align(header_count_, LV_ALIGN_BOTTOM_LEFT, 1, 0);
    auto *reload =
        create_button(header_, LV_SYMBOL_REFRESH, components::color_action, reload_event, this, "gallery_reload");
    lv_obj_set_style_bg_opa(reload, LV_OPA_COVER, 0);
    lv_obj_align(reload, LV_ALIGN_TOP_RIGHT, 0, 4);
}

void GalleryView::create_tabs()
{
    tab_bar_ = lv_obj_create(surface_);
    lv_obj_set_name_static(tab_bar_, "gallery_tab_bar");
    style_transparent_container(tab_bar_);
    lv_obj_set_size(tab_bar_, LV_PCT(100), 82);
    lv_obj_set_style_border_width(tab_bar_, 1, 0);
    lv_obj_set_style_border_side(tab_bar_, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(tab_bar_, lv_color_hex(components::color_border), 0);
    lv_obj_set_style_pad_top(tab_bar_, 6, 0);
    lv_obj_set_style_pad_bottom(tab_bar_, 10, 0);
    lv_obj_set_flex_flow(tab_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_bar_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    library_tab_ =
        create_button(tab_bar_, nullptr, components::color_action, library_event, this, "gallery_library_tab");
    albums_tab_ = create_button(tab_bar_, nullptr, components::color_action, albums_event, this, "gallery_albums_tab");
    for (auto *tab : {library_tab_, albums_tab_}) {
        lv_obj_set_size(tab, LV_PCT(40), 62);
        lv_obj_set_style_radius(tab, 14, 0);
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(tab, 5, 0);
        lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        create_photo_icon(tab, tab == albums_tab_);
        create_label(tab, tab == library_tab_ ? "Library" : "Albums", 14, components::color_muted_text);
    }
}

void GalleryView::create_viewer()
{
    // The viewer is a full-page overlay: library chrome never reduces the photo area.
    viewer_ = lv_obj_create(surface_);
    cm0_input_set_focus_scope(viewer_);
    lv_obj_set_name_static(viewer_, "gallery_viewer");
    style_transparent_container(viewer_);
    lv_obj_add_flag(viewer_, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(viewer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(viewer_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(viewer_, lv_color_hex(components::color_viewer_background), 0);
    lv_obj_set_style_bg_opa(viewer_, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(viewer_, LV_FLEX_FLOW_COLUMN);

    viewer_top_bar_ = lv_obj_create(viewer_);
    lv_obj_set_name_static(viewer_top_bar_, "gallery_viewer_header");
    style_transparent_container(viewer_top_bar_);
    lv_obj_set_size(viewer_top_bar_, LV_PCT(100), kViewerHeaderHeight);
    lv_obj_set_style_pad_hor(viewer_top_bar_, 12, 0);
    lv_obj_set_style_pad_column(viewer_top_bar_, 8, 0);
    lv_obj_set_flex_flow(viewer_top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(viewer_top_bar_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    create_button(viewer_top_bar_, LV_SYMBOL_LEFT, components::color_container_title, grid_event, this,
                  "gallery_back_to_grid");
    auto *metadata = lv_obj_create(viewer_top_bar_);
    style_transparent_container(metadata);
    lv_obj_set_height(metadata, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(metadata, 1);
    lv_obj_set_flex_flow(metadata, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(metadata, 4, 0);
    lv_obj_set_flex_align(metadata, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    viewer_date_ = create_label(metadata, "", 18, components::color_container_title);
    lv_obj_set_width(viewer_date_, LV_PCT(100));
    lv_obj_set_style_text_align(viewer_date_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(viewer_date_, LV_LABEL_LONG_DOT);
    viewer_filename_ = create_label(metadata, "", 14, components::color_muted_text);
    lv_obj_set_name_static(viewer_filename_, "gallery_viewer_filename");
    lv_obj_set_width(viewer_filename_, LV_PCT(100));
    lv_label_set_long_mode(viewer_filename_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(viewer_filename_, LV_TEXT_ALIGN_CENTER, 0);
    // A balancing spacer keeps the date centered on the photo.
    auto *spacer = lv_obj_create(viewer_top_bar_);
    style_transparent_container(spacer);
    lv_obj_set_size(spacer, 48, 48);

    auto *image_area = lv_obj_create(viewer_);
    lv_obj_set_name_static(image_area, "gallery_viewer_image_area");
    style_transparent_container(image_area);
    lv_obj_set_width(image_area, LV_PCT(100));
    lv_obj_set_flex_grow(image_area, 1);
    lv_obj_add_event_cb(image_area, content_size_changed_event, LV_EVENT_SIZE_CHANGED, this);
    lv_obj_add_flag(image_area, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(image_area, swipe_event, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(image_area, swipe_event, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(image_area, swipe_event, LV_EVENT_PRESS_LOST, this);
    viewer_canvas_ = lv_canvas_create(image_area);
    lv_obj_set_name_static(viewer_canvas_, "gallery_viewer_image");
    lv_obj_clear_flag(viewer_canvas_, LV_OBJ_FLAG_CLICKABLE);
    viewer_status_ = create_label(image_area, "Unable to open photo", 18, components::color_container_title);
    lv_obj_set_name_static(viewer_status_, "gallery_viewer_status");
    lv_obj_set_width(viewer_status_, LV_PCT(90));
    lv_obj_set_style_text_align(viewer_status_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(viewer_status_);

    filmstrip_ = lv_obj_create(viewer_);
    lv_obj_set_name_static(filmstrip_, "gallery_filmstrip");
    style_transparent_container(filmstrip_);
    lv_obj_set_size(filmstrip_, LV_PCT(100), kFilmstripHeight);
    lv_obj_set_style_pad_ver(filmstrip_, 4, 0);
    lv_obj_set_style_pad_hor(filmstrip_, 12, 0);
    lv_obj_set_style_pad_column(filmstrip_, 4, 0);
    lv_obj_set_flex_flow(filmstrip_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(filmstrip_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(filmstrip_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(filmstrip_, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(filmstrip_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(filmstrip_, content_size_changed_event, LV_EVENT_SIZE_CHANGED, this);
    lv_obj_add_event_cb(filmstrip_, filmstrip_layout_event, LV_EVENT_LAYOUT_CHANGED, this);

    auto *controls = viewer_controls_ = lv_obj_create(viewer_);
    lv_obj_set_name_static(controls, "gallery_viewer_controls");
    style_transparent_container(controls);
    lv_obj_set_size(controls, LV_PCT(100), kControlBarHeight);
    lv_obj_set_style_pad_hor(controls, 16, 0);
    lv_obj_set_style_pad_column(controls, 10, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    previous_button_ = create_button(controls, LV_SYMBOL_LEFT, components::color_container_title, previous_event, this,
                                     "gallery_previous");
    next_button_ =
        create_button(controls, LV_SYMBOL_RIGHT, components::color_container_title, next_event, this, "gallery_next");
    viewer_index_ = create_label(controls, "", 14, components::color_container_title);
    lv_obj_set_name_static(viewer_index_, "gallery_viewer_index");
    lv_obj_set_flex_grow(viewer_index_, 1);
    lv_obj_set_style_text_align(viewer_index_, LV_TEXT_ALIGN_CENTER, 0);
    delete_button_ = create_button(controls, LV_SYMBOL_TRASH, components::color_container_title, delete_event, this,
                                   "gallery_delete");

    // The sheet shares space with the image so both remain visible on short displays.
    viewer_details_ = lv_obj_create(viewer_);
    cm0_input_set_focus_scope(viewer_details_);
    lv_obj_set_name_static(viewer_details_, "gallery_viewer_details");
    style_transparent_container(viewer_details_);
    lv_obj_add_flag(viewer_details_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(viewer_details_, LV_PCT(100), LV_PCT(50));
    lv_obj_set_style_max_height(viewer_details_, 360, 0);
    lv_obj_set_style_bg_color(viewer_details_, lv_color_hex(components::color_control_background), 0);
    lv_obj_set_style_bg_opa(viewer_details_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(viewer_details_, 16, 0);
    lv_obj_set_style_pad_hor(viewer_details_, 20, 0);
    lv_obj_set_style_pad_ver(viewer_details_, 12, 0);
    lv_obj_set_flex_flow(viewer_details_, LV_FLEX_FLOW_COLUMN);
    auto *details_header = lv_obj_create(viewer_details_);
    lv_obj_set_name_static(details_header, "gallery_viewer_details_header");
    style_transparent_container(details_header);
    lv_obj_set_size(details_header, LV_PCT(100), 48);
    lv_obj_set_flex_flow(details_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(details_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(details_header, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(details_header, swipe_event, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(details_header, swipe_event, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(details_header, swipe_event, LV_EVENT_PRESS_LOST, this);
    auto *title = create_label(details_header, "Photo Details", 22, components::color_container_title);
    lv_obj_set_name_static(title, "gallery_viewer_details_title");
    lv_obj_set_flex_grow(title, 1);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    create_button(details_header, LV_SYMBOL_CLOSE, components::color_action, close_details_event, this,
                  "gallery_viewer_details_close");

    viewer_details_body_ = lv_obj_create(viewer_details_);
    lv_obj_set_name_static(viewer_details_body_, "gallery_viewer_details_body");
    style_transparent_container(viewer_details_body_);
    lv_obj_set_width(viewer_details_body_, LV_PCT(100));
    lv_obj_set_flex_grow(viewer_details_body_, 1);
    lv_obj_add_flag(viewer_details_body_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(viewer_details_body_, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scroll_dir(viewer_details_body_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(viewer_details_body_, LV_SCROLLBAR_MODE_AUTO);
    viewer_detail_text_ = create_label(viewer_details_body_, "", 14, components::color_container_title);
    lv_obj_set_name_static(viewer_detail_text_, "gallery_viewer_details_text");
    lv_obj_set_width(viewer_detail_text_, LV_PCT(100));
    lv_label_set_long_mode(viewer_detail_text_, LV_LABEL_LONG_WRAP);
}

void GalleryView::create_delete_confirmation()
{
    delete_confirmation_ = lv_obj_create(surface_);
    cm0_input_set_focus_scope(delete_confirmation_);
    lv_obj_set_name_static(delete_confirmation_, "gallery_delete_confirmation");
    style_transparent_container(delete_confirmation_);
    lv_obj_set_size(delete_confirmation_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(delete_confirmation_, lv_color_hex(components::color_viewer_overlay), 0);
    lv_obj_set_style_bg_opa(delete_confirmation_, LV_OPA_60, 0);
    lv_obj_add_flag(delete_confirmation_, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(delete_confirmation_, LV_OBJ_FLAG_HIDDEN);
    auto *sheet = lv_obj_create(delete_confirmation_);
    lv_obj_set_name_static(sheet, "gallery_delete_dialog");
    style_transparent_container(sheet);
    lv_obj_set_size(sheet, 400, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(sheet, 8, 0);
    lv_obj_set_flex_flow(sheet, LV_FLEX_FLOW_COLUMN);
    lv_obj_align(sheet, LV_ALIGN_BOTTOM_MID, 0, -12);

    auto *panel = lv_obj_create(sheet);
    style_transparent_container(panel);
    lv_obj_set_size(panel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(components::color_control_background), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 14, 0);
    lv_obj_set_style_pad_top(panel, 20, 0);
    lv_obj_set_style_pad_row(panel, 10, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    create_label(panel, "Delete this photo?", 18, components::color_container_title);
    auto *description =
        create_label(panel, "This photo will be permanently deleted.", 14, components::color_muted_text);
    lv_obj_set_width(description, LV_PCT(90));
    lv_obj_set_style_text_align(description, LV_TEXT_ALIGN_CENTER, 0);
    delete_error_ = create_label(panel, "Could not delete photo. Try again.", 14, components::color_destructive);
    lv_obj_set_name_static(delete_error_, "gallery_delete_error");
    lv_obj_set_width(delete_error_, LV_PCT(90));
    lv_obj_set_style_text_align(delete_error_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(delete_error_, LV_OBJ_FLAG_HIDDEN);
    auto *confirm = create_button(panel, "Delete Photo", components::color_destructive, confirm_delete_event, this,
                                  "gallery_delete_confirm");
    lv_obj_set_size(confirm, LV_PCT(100), 56);
    lv_obj_set_style_radius(confirm, 0, 0);
    lv_obj_set_style_border_width(confirm, 1, 0);
    lv_obj_set_style_border_side(confirm, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(confirm, lv_color_hex(components::color_border), 0);
    auto *cancel =
        create_button(sheet, "Cancel", components::color_action, cancel_delete_event, this, "gallery_delete_cancel");
    lv_obj_set_size(cancel, LV_PCT(100), 56);
    lv_obj_set_style_radius(cancel, 14, 0);
    lv_obj_set_style_bg_color(cancel, lv_color_hex(components::color_container_background), 0);
    lv_obj_set_style_bg_opa(cancel, LV_OPA_COVER, 0);
}

void GalleryView::rebuild_grid()
{
    if (!grid_) return;
    const auto scroll_y = lv_obj_get_scroll_y(grid_);
    lv_obj_clean(grid_);
    thumbnails_.clear();
    update_header();
    const auto &items = view_model_.items();
    if (items.empty()) {
        lv_obj_set_flex_align(grid_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        auto *empty = lv_obj_create(grid_);
        lv_obj_set_name_static(empty, "gallery_empty_state");
        style_transparent_container(empty);
        lv_obj_set_size(empty, LV_PCT(90), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_row(empty, 12, 0);
        lv_obj_set_flex_flow(empty, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(empty, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        create_label(empty, LV_SYMBOL_IMAGE, 48, components::color_muted_text);
        create_label(empty, "No Photos", 28, components::color_container_title);
        auto *hint = create_label(empty, "Photos will appear here when added.\nTap refresh to check again.", 14,
                                  components::color_muted_text);
        lv_obj_set_width(hint, LV_PCT(100));
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    lv_obj_set_flex_align(grid_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    thumbnails_.reserve(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        thumbnails_.push_back({this, index, nullptr, nullptr, {}});
        auto &thumbnail = thumbnails_.back();
        thumbnail.tile  = lv_button_create(grid_);
        const auto name = std::string("gallery_tile_") + std::to_string(index);
        lv_obj_set_name(thumbnail.tile, name.c_str());
        lv_obj_remove_style_all(thumbnail.tile);
        lv_obj_clear_flag(thumbnail.tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_opa(thumbnail.tile, LV_OPA_70, LV_STATE_PRESSED);
        lv_obj_add_event_cb(thumbnail.tile, thumbnail_event, LV_EVENT_CLICKED, &thumbnail);
        thumbnail.canvas      = lv_canvas_create(thumbnail.tile);
        const auto image_name = std::string("gallery_thumbnail_") + std::to_string(index);
        lv_obj_set_name(thumbnail.canvas, image_name.c_str());
        auto *placeholder = create_label(thumbnail.tile, LV_SYMBOL_IMAGE, 28, components::color_muted_text);
        lv_obj_center(placeholder);
        lv_obj_add_flag(placeholder, LV_OBJ_FLAG_HIDDEN);
    }
    auto *summary = lv_obj_create(grid_);
    style_transparent_container(summary);
    lv_obj_set_name_static(summary, "gallery_library_summary");
    lv_obj_set_size(summary, LV_PCT(100), 92);
    lv_obj_set_flex_flow(summary, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(summary, 6, 0);
    lv_obj_set_flex_align(summary, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    create_label(summary, collection_title(view_model_.collection()), 18, components::color_container_title);
    create_label(summary, "Sorted by most recent", 14, components::color_muted_text);
    resize_grid();
    lv_obj_update_layout(grid_);
    lv_obj_scroll_to_y(grid_, scroll_y, LV_ANIM_OFF);
}

void GalleryView::resize_grid()
{
    // Resize existing tiles after the grid receives its own dimensions. Keeping the
    // objects alive preserves focus and scroll state during a display rotation.
    const auto available = std::max<std::int32_t>(1, lv_obj_get_content_width(grid_));
    const auto columns   = available >= 1000 ? 6 : available >= 700 ? 4 : 3;
    const auto side      = std::max<std::int32_t>(1, (available - (columns - 1) * kGridGap) / columns);
    const auto remainder = std::max<std::int32_t>(0, available - columns * side - (columns - 1) * kGridGap);
    const auto &items    = view_model_.items();
    for (auto &thumbnail : thumbnails_) {
        if (thumbnail.index >= items.size()) continue;
        const auto width = side + (static_cast<std::int32_t>(thumbnail.index % columns) < remainder ? 1 : 0);
        lv_obj_set_size(thumbnail.tile, width, side);
        const bool loaded = gallery_render_image(items[thumbnail.index].thumbnail_path, width, side,
                                                 GalleryImageFit::cover, thumbnail.pixels);
        lv_canvas_set_buffer(thumbnail.canvas, thumbnail.pixels.data(), width, side, LV_COLOR_FORMAT_RGB565);
        lv_obj_invalidate(thumbnail.canvas);
        set_hidden(lv_obj_get_child(thumbnail.tile, 1), loaded);
    }
}

void GalleryView::rebuild_albums()
{
    const auto scroll_y = lv_obj_get_scroll_y(albums_);
    lv_obj_clean(albums_);
    const auto available = std::max<std::int32_t>(1, lv_obj_get_content_width(albums_));
    const auto columns   = available >= 700 ? 4 : 2;
    const auto width     = std::max<std::int32_t>(1, (available - (columns - 1) * kAlbumGap) / columns);
    const std::array<Collection, 4> collections = {Collection::all, Collection::camera, Collection::screenshots,
                                                   Collection::samples};
    const char *names[] = {"gallery_album_all", "gallery_album_camera", "gallery_album_screenshots",
                           "gallery_album_samples"};
    for (std::size_t i = 0; i < collections.size(); ++i) {
        auto &album      = album_widgets_[i];
        album.owner      = this;
        album.collection = collections[i];
        auto *card       = create_button(albums_, nullptr, components::color_action, album_event, &album, names[i]);
        lv_obj_set_size(card, width, width + 60);
        lv_obj_set_style_radius(card, 0, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card, 5, 0);
        auto *cover = lv_obj_create(card);
        style_transparent_container(cover);
        lv_obj_set_size(cover, width, width);
        lv_obj_clear_flag(cover, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(cover, 12, 0);
        lv_obj_set_style_clip_corner(cover, true, 0);
        lv_obj_set_style_bg_color(cover, lv_color_hex(components::color_control_background), 0);
        lv_obj_set_style_bg_opa(cover, LV_OPA_COVER, 0);
        const auto &items = view_model_.items();
        const auto found  = std::find_if(items.begin(), items.end(),
                                         [&](const auto &item) { return belongs_to(item, album.collection); });
        if (found != items.end()) {
            const bool loaded =
                gallery_render_image(found->thumbnail_path, width, width, GalleryImageFit::cover, album.pixels);
            auto *canvas = lv_canvas_create(cover);
            lv_canvas_set_buffer(canvas, album.pixels.data(), width, width, LV_COLOR_FORMAT_RGB565);
            if (!loaded) lv_obj_center(create_label(cover, LV_SYMBOL_IMAGE, 36, components::color_muted_text));
        } else {
            album.pixels.clear();
            lv_obj_center(create_label(cover, LV_SYMBOL_IMAGE, 36, components::color_muted_text));
        }
        auto *title = create_label(card, collection_title(album.collection), 18, components::color_container_title);
        lv_obj_set_width(title, LV_PCT(100));
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        const auto count = std::to_string(view_model_.collection_count(album.collection));
        create_label(card, count.c_str(), 14, components::color_muted_text);
    }
    lv_obj_update_layout(albums_);
    lv_obj_scroll_to_y(albums_, scroll_y, LV_ANIM_OFF);
}

void GalleryView::resize_albums()
{
    if (lv_obj_get_child_count(albums_) != album_widgets_.size()) return;
    const auto available = std::max<std::int32_t>(1, lv_obj_get_content_width(albums_));
    const auto columns   = available >= 700 ? 4 : 2;
    const auto width     = std::max<std::int32_t>(1, (available - (columns - 1) * kAlbumGap) / columns);
    for (std::size_t i = 0; i < album_widgets_.size(); ++i) {
        auto *card  = lv_obj_get_child(albums_, static_cast<std::int32_t>(i));
        auto *cover = lv_obj_get_child(card, 0);
        lv_obj_set_size(card, width, width + 60);
        lv_obj_set_size(cover, width, width);
        auto *canvas = lv_obj_get_child(cover, 0);
        if (!lv_obj_check_type(canvas, &lv_canvas_class)) continue;
        auto &album       = album_widgets_[i];
        const auto &items = view_model_.items();
        const auto found  = std::find_if(items.begin(), items.end(),
                                         [&](const auto &item) { return belongs_to(item, album.collection); });
        if (found == items.end()) continue;
        (void)gallery_render_image(found->thumbnail_path, width, width, GalleryImageFit::cover, album.pixels);
        lv_canvas_set_buffer(canvas, album.pixels.data(), width, width, LV_COLOR_FORMAT_RGB565);
        lv_obj_invalidate(canvas);
    }
}

void GalleryView::rebuild_filmstrip()
{
    lv_obj_clean(filmstrip_);
    filmstrip_thumbnails_.clear();
    const auto &items = view_model_.items();
    filmstrip_thumbnails_.reserve(items.size());
    const bool fits = items.size() * 42 <= static_cast<std::size_t>(std::max(0, lv_obj_get_content_width(filmstrip_)));
    lv_obj_set_flex_align(filmstrip_, fits ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    for (std::size_t i = 0; i < items.size(); ++i) {
        filmstrip_thumbnails_.push_back({this, i, nullptr, nullptr, {}});
        auto &thumbnail = filmstrip_thumbnails_.back();
        thumbnail.tile  = lv_button_create(filmstrip_);
        lv_obj_remove_style_all(thumbnail.tile);
        const auto name = std::string("gallery_filmstrip_") + std::to_string(i);
        lv_obj_set_name(thumbnail.tile, name.c_str());
        lv_obj_set_size(thumbnail.tile, 38, 50);
        lv_obj_set_style_radius(thumbnail.tile, 4, 0);
        lv_obj_set_style_clip_corner(thumbnail.tile, true, 0);
        lv_obj_set_style_border_color(thumbnail.tile, lv_color_hex(components::color_navigation_accent), 0);
        lv_obj_set_style_border_width(thumbnail.tile, 2, LV_STATE_CHECKED);
        lv_obj_set_style_border_post(thumbnail.tile, true, 0);
        lv_obj_set_style_opa(thumbnail.tile, LV_OPA_60, 0);
        lv_obj_set_style_opa(thumbnail.tile, LV_OPA_COVER, LV_STATE_CHECKED);
        lv_obj_add_event_cb(thumbnail.tile, thumbnail_event, LV_EVENT_CLICKED, &thumbnail);
        thumbnail.canvas = lv_canvas_create(thumbnail.tile);
        (void)gallery_render_image(items[i].thumbnail_path, 38, 50, GalleryImageFit::cover, thumbnail.pixels);
        lv_canvas_set_buffer(thumbnail.canvas, thumbnail.pixels.data(), 38, 50, LV_COLOR_FORMAT_RGB565);
        lv_obj_center(thumbnail.canvas);
    }
}

void GalleryView::show_grid()
{
    view_model_.show_grid();
    showing_albums_  = false;
    details_visible_ = false;
    set_hidden(viewer_, true);
    set_hidden(viewer_details_, true);
    set_hidden(albums_, true);
    set_hidden(grid_, false);
    set_hidden(header_, false);
    set_hidden(tab_bar_, false);
    set_hidden(delete_confirmation_, true);
    update_header();
    lv_obj_update_layout(surface_);
}

void GalleryView::show_albums()
{
    view_model_.set_collection(Collection::all);
    showing_albums_ = albums_tab_active_ = true;
    details_visible_                     = false;
    set_hidden(viewer_details_, true);
    set_hidden(viewer_, true);
    set_hidden(grid_, true);
    set_hidden(albums_, false);
    set_hidden(header_, false);
    set_hidden(tab_bar_, false);
    set_hidden(delete_confirmation_, true);
    update_header();
    lv_obj_update_layout(surface_);
    rebuild_albums();
}

void GalleryView::show_viewer()
{
    if (!view_model_.selected_item()) {
        show_grid();
        return;
    }
    set_hidden(header_, true);
    set_hidden(tab_bar_, true);
    set_hidden(grid_, true);
    set_hidden(albums_, true);
    set_hidden(viewer_, false);
    set_details_visible(false);
    lv_obj_update_layout(surface_);
    rebuild_filmstrip();
    render_viewer();
}

void GalleryView::render_viewer()
{
    const auto *item = view_model_.selected_item();
    if (!item || !viewer_canvas_) return;
    lv_obj_update_layout(surface_);
    auto *area        = lv_obj_get_parent(viewer_canvas_);
    const auto width  = std::max<std::int32_t>(1, lv_obj_get_content_width(area));
    const auto height = std::max<std::int32_t>(1, lv_obj_get_content_height(area));
    const bool loaded = gallery_render_image(item->photo_path, width, height, GalleryImageFit::contain, viewer_pixels_);
    lv_canvas_set_buffer(viewer_canvas_, viewer_pixels_.data(), width, height, LV_COLOR_FORMAT_RGB565);
    lv_obj_invalidate(viewer_canvas_);
    lv_label_set_text(viewer_date_, photo_date(item->modified_time).c_str());
    lv_label_set_text(viewer_filename_, item->filename.c_str());
    const auto source  = item->source == GallerySource::camera        ? "Camera"
                         : item->source == GallerySource::screenshots ? "Screenshots"
                                                                      : "Samples (read-only)";
    const auto details = "Filename: " + item->filename + "\n\nModified: " + photo_date(item->modified_time, true) +
                         "\nSize: " + std::to_string(item->size_bytes) + " bytes\nSource: " + source +
                         "\n\nLocation: " + item->photo_path;
    if (details != lv_label_get_text(viewer_detail_text_)) {
        lv_label_set_text(viewer_detail_text_, details.c_str());
        lv_obj_scroll_to_y(viewer_details_body_, 0, LV_ANIM_OFF);
    }
    set_hidden(viewer_status_, loaded);
    update_navigation();
}

void GalleryView::set_details_visible(bool visible)
{
    details_visible_ = visible;
    set_hidden(viewer_details_, !visible);
    set_hidden(viewer_controls_, visible);
    set_hidden(filmstrip_, visible || lv_obj_get_content_height(surface_) < 400);
    if (visible) lv_obj_scroll_to_y(viewer_details_body_, 0, LV_ANIM_OFF);
}

void GalleryView::update_header() noexcept
{
    if (!header_count_) return;
    const auto count = view_model_.items().size();
    lv_label_set_text_fmt(header_count_, "%u %s", static_cast<unsigned>(count), count == 1 ? "photo" : "photos");
    lv_label_set_text(header_title_, showing_albums_ ? "Albums"
                                     : view_model_.collection() == Collection::all
                                         ? "Library"
                                         : collection_title(view_model_.collection()));
    tint_tab(library_tab_, !albums_tab_active_);
    tint_tab(albums_tab_, albums_tab_active_);
}

void GalleryView::update_navigation() noexcept
{
    lv_label_set_text_fmt(viewer_index_, "%u / %u", static_cast<unsigned>(view_model_.selected_index() + 1),
                          static_cast<unsigned>(view_model_.items().size()));
    if (view_model_.can_select_previous())
        lv_obj_remove_state(previous_button_, LV_STATE_DISABLED);
    else
        lv_obj_add_state(previous_button_, LV_STATE_DISABLED);
    if (view_model_.can_select_next())
        lv_obj_remove_state(next_button_, LV_STATE_DISABLED);
    else
        lv_obj_add_state(next_button_, LV_STATE_DISABLED);
    const auto *item = view_model_.selected_item();
    if (item && !item->read_only)
        lv_obj_remove_state(delete_button_, LV_STATE_DISABLED);
    else
        lv_obj_add_state(delete_button_, LV_STATE_DISABLED);
    for (auto &thumbnail : filmstrip_thumbnails_) {
        if (thumbnail.index == view_model_.selected_index()) {
            lv_obj_add_state(thumbnail.tile, LV_STATE_CHECKED);
            lv_obj_scroll_to_view(thumbnail.tile, LV_ANIM_OFF);
        } else
            lv_obj_remove_state(thumbnail.tile, LV_STATE_CHECKED);
    }
}

void GalleryView::apply_layout()
{
    if (!surface_ || !delete_confirmation_ || applying_layout_) return;
    applying_layout_  = true;
    const auto width  = lv_obj_get_content_width(surface_);
    const auto height = lv_obj_get_content_height(surface_);
    if (width > 0 && height > 0 && (width != layout_width_ || height != layout_height_)) {
        if (layout_width_ > 0 && !restore_scroll_) {
            grid_scroll_y_   = lv_obj_get_scroll_y(grid_);
            albums_scroll_y_ = lv_obj_get_scroll_y(albums_);
            restore_scroll_  = true;
        }
        layout_width_      = width;
        layout_height_     = height;
        const bool compact = height < 600;
        lv_obj_set_height(header_, compact ? 104 : 124);
        lv_obj_set_height(tab_bar_, compact ? 72 : 82);
        // On short displays reclaim the filmstrip's space for the photo.
        set_hidden(filmstrip_, details_visible_ || height < 400);
        lv_obj_set_height(viewer_top_bar_, height < 400 ? 64 : kViewerHeaderHeight);
        auto *sheet = lv_obj_find_by_name(delete_confirmation_, "gallery_delete_dialog");
        lv_obj_set_width(sheet, std::min<std::int32_t>(400, std::max<std::int32_t>(1, width - 24)));
        lv_obj_set_style_max_height(sheet, std::max<std::int32_t>(1, height - 24), 0);
        lv_obj_add_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(sheet, LV_DIR_VER);
    }
    applying_layout_ = false;
}

void GalleryView::size_changed_event(lv_event_t *event)
{
    static_cast<GalleryView *>(lv_event_get_user_data(event))->apply_layout();
}

void GalleryView::content_size_changed_event(lv_event_t *event)
{
    auto *view   = static_cast<GalleryView *>(lv_event_get_user_data(event));
    auto *object = lv_event_get_target_obj(event);
    if (object == view->grid_)
        view->resize_grid();
    else if (object == view->albums_)
        view->resize_albums();
    else if (object == view->filmstrip_) {
        const bool fits = view->filmstrip_thumbnails_.size() * 42 <=
                          static_cast<std::size_t>(std::max(0, lv_obj_get_content_width(object)));
        lv_obj_set_flex_align(object, fits ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
    } else if (view->view_model_.mode() == GalleryViewModel::Mode::viewer && view->viewer_canvas_) {
        view->render_viewer();
    }
}

void GalleryView::filmstrip_layout_event(lv_event_t *event)
{
    auto *view       = static_cast<GalleryView *>(lv_event_get_user_data(event));
    const auto index = view->view_model_.selected_index();
    if (view->view_model_.mode() == GalleryViewModel::Mode::viewer && index < view->filmstrip_thumbnails_.size()) {
        lv_obj_scroll_to_view(view->filmstrip_thumbnails_[index].tile, LV_ANIM_OFF);
    }
}

void GalleryView::layout_completed_event(lv_event_t *event)
{
    auto *view = static_cast<GalleryView *>(lv_event_get_user_data(event));
    if (!view->restore_scroll_) return;
    view->restore_scroll_ = false;
    // LVGL may clamp scrolling while sibling sizes are still changing. Restore
    // after the whole layout settles, when the final content bounds are known.
    lv_obj_scroll_to_y(view->grid_, view->grid_scroll_y_, LV_ANIM_OFF);
    lv_obj_scroll_to_y(view->albums_, view->albums_scroll_y_, LV_ANIM_OFF);
}

void GalleryView::reload_event(lv_event_t *event)
{
    auto *view = static_cast<GalleryView *>(lv_event_get_user_data(event));
    view->view_model_.reload();
    if (view->showing_albums_) {
        view->update_header();
        view->rebuild_albums();
        return;
    }
    view->rebuild_grid();
    if (view->view_model_.mode() == GalleryViewModel::Mode::viewer)
        view->show_viewer();
    else
        view->show_grid();
}

void GalleryView::thumbnail_event(lv_event_t *event)
{
    auto *thumbnail = static_cast<ThumbnailWidget *>(lv_event_get_user_data(event));
    if (!thumbnail) return;
    auto *view       = thumbnail->owner;
    const auto index = thumbnail->index;
    if (view->view_model_.open(index)) {
        if (lv_obj_has_flag(view->viewer_, LV_OBJ_FLAG_HIDDEN))
            view->show_viewer();
        else
            view->render_viewer();
    }
}

void GalleryView::library_event(lv_event_t *event)
{
    auto *view = static_cast<GalleryView *>(lv_event_get_user_data(event));
    view->view_model_.set_collection(Collection::all);
    view->albums_tab_active_ = false;
    view->show_grid();
    view->rebuild_grid();
}

void GalleryView::albums_event(lv_event_t *event)
{
    static_cast<GalleryView *>(lv_event_get_user_data(event))->show_albums();
}

void GalleryView::album_event(lv_event_t *event)
{
    auto *album = static_cast<AlbumWidget *>(lv_event_get_user_data(event));
    auto *view  = album->owner;
    view->view_model_.set_collection(album->collection);
    view->show_grid();
    view->rebuild_grid();
    lv_obj_scroll_to_y(view->grid_, 0, LV_ANIM_OFF);
}

void GalleryView::grid_event(lv_event_t *event)
{
    static_cast<GalleryView *>(lv_event_get_user_data(event))->show_grid();
}

void GalleryView::close_details_event(lv_event_t *event)
{
    auto *view = static_cast<GalleryView *>(lv_event_get_user_data(event));
    view->set_details_visible(false);
    view->render_viewer();
}

void GalleryView::previous_event(lv_event_t *event)
{
    auto *view = static_cast<GalleryView *>(lv_event_get_user_data(event));
    if (view->view_model_.select_previous()) view->render_viewer();
}

void GalleryView::next_event(lv_event_t *event)
{
    auto *view = static_cast<GalleryView *>(lv_event_get_user_data(event));
    if (view->view_model_.select_next()) view->render_viewer();
}

void GalleryView::delete_event(lv_event_t *event)
{
    auto *view       = static_cast<GalleryView *>(lv_event_get_user_data(event));
    const auto *item = view->view_model_.selected_item();
    if (!item || item->read_only) return;
    set_hidden(view->delete_error_, true);
    set_hidden(view->delete_confirmation_, false);
    lv_obj_move_foreground(view->delete_confirmation_);
}

void GalleryView::cancel_delete_event(lv_event_t *event)
{
    auto *view = static_cast<GalleryView *>(lv_event_get_user_data(event));
    set_hidden(view->delete_confirmation_, true);
}

void GalleryView::confirm_delete_event(lv_event_t *event)
{
    auto *view = static_cast<GalleryView *>(lv_event_get_user_data(event));
    if (lv_obj_has_flag(view->delete_confirmation_, LV_OBJ_FLAG_HIDDEN)) return;
    if (!view->view_model_.remove_selected()) {
        set_hidden(view->delete_error_, false);
        return;
    }
    set_hidden(view->delete_confirmation_, true);
    view->rebuild_grid();
    if (view->view_model_.mode() == GalleryViewModel::Mode::viewer)
        view->show_viewer();
    else
        view->show_grid();
}

void GalleryView::swipe_event(lv_event_t *event)
{
    auto *view        = static_cast<GalleryView *>(lv_event_get_user_data(event));
    const auto code   = lv_event_get_code(event);
    const auto *input = lv_indev_active();
    if (code == LV_EVENT_PRESS_LOST) {
        view->swipe_pressed_ = false;
        return;
    }
    if (!input || view->view_model_.mode() != GalleryViewModel::Mode::viewer ||
        lv_obj_is_visible(view->delete_confirmation_))
        return;
    lv_point_t point{};
    lv_indev_get_point(input, &point);
    if (code == LV_EVENT_PRESSED) {
        view->press_point_   = point;
        view->swipe_pressed_ = true;
        return;
    }
    if (!view->swipe_pressed_) return;
    view->swipe_pressed_ = false;
    const auto delta_x   = point.x - view->press_point_.x;
    const auto delta_y   = point.y - view->press_point_.y;
    // Vertical swipes control the viewer itself. A downward swipe is the
    // gesture used to leave the preview; an upward swipe reveals metadata.
    if (std::abs(delta_y) >= 60 && std::abs(delta_y) > std::abs(delta_x)) {
        if (delta_y > 0) {
            view->show_grid();
        } else if (view->view_model_.selected_item()) {
            view->set_details_visible(true);
            view->render_viewer();
        }
        return;
    }
    if (std::abs(delta_x) < 60 || std::abs(delta_y) >= std::abs(delta_x)) return;
    if (delta_x > 0) {
        if (view->view_model_.select_previous()) view->render_viewer();
    } else if (view->view_model_.select_next())
        view->render_viewer();
}
