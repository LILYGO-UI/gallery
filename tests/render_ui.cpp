#include "app.hpp"
#include "app_identity.h"
#include "pages/gallery/gallery_model.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr int kPartialRows                        = 80;
constexpr std::uint32_t kColorPageBackground      = 0xffffff;
constexpr std::uint32_t kColorContainerBackground = 0xffffff;
constexpr std::uint32_t kColorViewerBackground    = kColorPageBackground;

std::vector<lv_color32_t> framebuffer;
int display_width = 0;
lv_point_t pointer_position{};
lv_indev_state_t pointer_state = LV_INDEV_STATE_RELEASED;

void read_pointer(lv_indev_t *, lv_indev_data_t *data)
{
    data->point = pointer_position;
    data->state = pointer_state;
}

void move_pointer(lv_indev_t *pointer, int x, int y, lv_indev_state_t state)
{
    pointer_position = {x, y};
    pointer_state    = state;
    lv_tick_inc(25);
    lv_indev_read(pointer);
}

void tap(lv_indev_t *pointer, lv_obj_t *object)
{
    lv_obj_scroll_to_view_recursive(object, LV_ANIM_OFF);
    lv_obj_update_layout(lv_screen_active());
    lv_area_t area{};
    lv_obj_get_coords(object, &area);
    for (auto *parent = lv_obj_get_parent(object); parent; parent = lv_obj_get_parent(parent)) {
        lv_area_t outer{};
        lv_obj_get_coords(parent, &outer);
        area.x1 = std::max(area.x1, outer.x1);
        area.y1 = std::max(area.y1, outer.y1);
        area.x2 = std::min(area.x2, outer.x2);
        area.y2 = std::min(area.y2, outer.y2);
    }
    assert(area.x1 <= area.x2 && area.y1 <= area.y2);
    const int x = (area.x1 + area.x2) / 2;
    const int y = (area.y1 + area.y2) / 2;
    move_pointer(pointer, x, y, LV_INDEV_STATE_PRESSED);
    move_pointer(pointer, x, y, LV_INDEV_STATE_RELEASED);
    lv_obj_update_layout(lv_screen_active());
}

void swipe(lv_indev_t *pointer, lv_obj_t *object, int delta_x, int delta_y = 0)
{
    lv_obj_update_layout(lv_screen_active());
    lv_area_t area{};
    lv_obj_get_coords(object, &area);
    const int x       = (area.x1 + area.x2) / 2;
    const int y       = (area.y1 + area.y2) / 2;
    const int start_x = std::abs(delta_x) >= lv_area_get_width(&area) ? x : x - delta_x / 2;
    const int start_y = std::abs(delta_y) >= lv_area_get_height(&area) ? y : y - delta_y / 2;
    move_pointer(pointer, start_x, start_y, LV_INDEV_STATE_PRESSED);
    move_pointer(pointer, start_x + delta_x, start_y + delta_y, LV_INDEV_STATE_PRESSED);
    move_pointer(pointer, start_x + delta_x, start_y + delta_y, LV_INDEV_STATE_RELEASED);
    lv_obj_update_layout(lv_screen_active());
}

void flush_display(lv_display_t *display, const lv_area_t *area, std::uint8_t *pixels)
{
    const int width    = area->x2 - area->x1 + 1;
    const auto *source = reinterpret_cast<const lv_color32_t *>(pixels);
    for (int y = area->y1; y <= area->y2; ++y) {
        const auto source_offset = static_cast<std::size_t>(y - area->y1) * static_cast<std::size_t>(width);
        const auto destination_offset =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(display_width) + static_cast<std::size_t>(area->x1);
        std::copy_n(source + source_offset, width, framebuffer.begin() + destination_offset);
    }
    lv_display_flush_ready(display);
}

bool write_snapshot(const char *path, int width, int height)
{
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (const auto &pixel : framebuffer) {
        file.put(static_cast<char>(pixel.red));
        file.put(static_cast<char>(pixel.green));
        file.put(static_cast<char>(pixel.blue));
    }
    return file.good();
}

void write_photo(const std::string &path, unsigned red, unsigned green, unsigned blue)
{
    constexpr int width  = 96;
    constexpr int height = 72;
    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            file.put(static_cast<char>(std::min(255U, red + static_cast<unsigned>(x))));
            file.put(static_cast<char>(std::min(255U, green + static_cast<unsigned>(y))));
            file.put(static_cast<char>(blue));
        }
    }
    file.close();
    assert(file.good());
}

void copy_file(const std::string &source, const std::string &destination)
{
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(destination, std::ios::binary);
    output << input.rdbuf();
    output.close();
    assert(input.good() || input.eof());
    assert(output.good());
}

bool colors_differ(const lv_color32_t &left, const lv_color32_t &right)
{
    return left.red != right.red || left.green != right.green || left.blue != right.blue || left.alpha != right.alpha;
}

bool is_viewer_background(const lv_color32_t &color)
{
    return color.red == ((kColorViewerBackground >> 16U) & 0xffU) &&
           color.green == ((kColorViewerBackground >> 8U) & 0xffU) && color.blue == (kColorViewerBackground & 0xffU);
}

void resize_display(lv_display_t *display, lv_obj_t *root, int width, int height)
{
    display_width = width;
    framebuffer.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), {});
    lv_display_set_resolution(display, width, height);
    lv_obj_update_layout(root);
    lv_refr_now(display);
}

void assert_within_display(lv_obj_t *object, int width, int height)
{
    lv_area_t area{};
    lv_obj_get_coords(object, &area);
    assert(area.x1 >= 0 && area.y1 >= 0);
    assert(area.x2 < width && area.y2 < height);
    assert(area.x2 >= area.x1 && area.y2 >= area.y1);
}

}  // namespace

int main(int argc, char **argv)
{
    assert(argc == 5);
    const int width  = std::atoi(argv[1]);
    const int height = std::atoi(argv[2]);
    assert(width >= 320 && height >= 240);

    char directory_template[] = "/tmp/lilygo-gallery-render-XXXXXX";
    const auto *created       = ::mkdtemp(directory_template);
    assert(created);
    const std::string directory  = created;
    const auto *sample_directory = std::getenv("LILYGO_GALLERY_RENDER_SAMPLE_DIR");
    const std::vector<std::string> photos =
        sample_directory && *sample_directory
            ? std::vector<std::string>{directory + "/sunrise.ppm", directory + "/forest.ppm", directory + "/night.ppm"}
            : std::vector<std::string>{directory + "/harbor.jpg", directory + "/garden.ppm", directory + "/sunset.ppm"};
    if (sample_directory && *sample_directory) {
        copy_file(std::string(sample_directory) + "/sunrise.ppm", photos[0]);
        copy_file(std::string(sample_directory) + "/forest.ppm", photos[1]);
        copy_file(std::string(sample_directory) + "/night.ppm", photos[2]);
    } else {
        copy_file(argv[4], photos[0]);
        write_photo(photos[1], 30, 130, 40);
        write_photo(photos[2], 160, 55, 25);
    }
    assert(::setenv("LILYGO_GALLERY_PHOTO_DIR", directory.c_str(), 1) == 0);
    assert(::setenv("LILYGO_GALLERY_SCREENSHOT_DIR", directory.c_str(), 1) == 0);
    assert(::setenv("LILYGO_GALLERY_SAMPLE_DIR", (directory + "/missing-samples").c_str(), 1) == 0);

    display_width = width;
    framebuffer.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    std::vector<lv_color32_t> draw_buffer(static_cast<std::size_t>(std::max(width, height)) * kPartialRows);

    lv_init();
    auto *display = lv_display_create(width, height);
    assert(display);
    lv_display_set_buffers(display, draw_buffer.data(), nullptr,
                           static_cast<std::uint32_t>(draw_buffer.size() * sizeof(lv_color32_t)),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush_display);
    lv_display_set_default(display);
    auto *pointer = lv_indev_create();
    assert(pointer);
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(pointer, display);
    lv_indev_set_read_cb(pointer, read_pointer);

    cm0_app_context_t context{};
    context.root           = lv_screen_active();
    const auto *descriptor = cm0_app_get_descriptor();
    descriptor->open(&context);
    lv_obj_update_layout(context.root);

    auto *surface     = lv_obj_find_by_name(context.root, "app_surface");
    auto *header      = lv_obj_find_by_name(context.root, "app_header");
    auto *grid        = lv_obj_find_by_name(context.root, "gallery_grid");
    auto *count       = lv_obj_find_by_name(context.root, "gallery_count");
    auto *tile        = lv_obj_find_by_name(context.root, "gallery_tile_0");
    auto *thumb       = lv_obj_find_by_name(context.root, "gallery_thumbnail_0");
    auto *tab_bar     = lv_obj_find_by_name(context.root, "gallery_tab_bar");
    auto *library_tab = lv_obj_find_by_name(context.root, "gallery_library_tab");
    auto *albums_tab  = lv_obj_find_by_name(context.root, "gallery_albums_tab");
    auto *albums      = lv_obj_find_by_name(context.root, "gallery_albums");
    assert(surface && header && grid && count && tile && thumb && tab_bar && library_tab && albums_tab && albums);
    assert(lv_color_eq(lv_obj_get_style_bg_color(surface, LV_PART_MAIN), lv_color_hex(kColorPageBackground)));
    assert(lv_color_eq(lv_obj_get_style_bg_color(header, LV_PART_MAIN), lv_color_hex(kColorContainerBackground)));
    assert(std::string(lv_label_get_text(count)) == "3 photos");
    assert(std::abs(lv_obj_get_width(tile) - lv_obj_get_height(tile)) <= 1);
    assert(lv_obj_get_width(thumb) == lv_obj_get_width(tile));
    assert(lv_obj_get_height(thumb) == lv_obj_get_height(tile));
    assert(lv_obj_is_visible(header) && lv_obj_is_visible(tab_bar));
    assert(!lv_obj_is_visible(albums));

    lv_tick_inc(20);
    lv_refr_now(display);
    const bool has_contrast = std::any_of(framebuffer.cbegin() + 1, framebuffer.cend(),
                                          [](const auto &pixel) { return colors_differ(pixel, framebuffer[0]); });
    assert(has_contrast);
    assert(write_snapshot(argv[3], width, height));

    lv_obj_send_event(albums_tab, LV_EVENT_CLICKED, nullptr);
    lv_obj_update_layout(context.root);
    assert(lv_obj_is_visible(albums) && !lv_obj_is_visible(grid));
    assert(lv_obj_is_visible(header) && lv_obj_is_visible(tab_bar));
    auto *samples_album = lv_obj_find_by_name(context.root, "gallery_album_samples");
    assert(samples_album);
    lv_refr_now(display);
    assert(write_snapshot((std::string(argv[3]) + ".albums.ppm").c_str(), width, height));
    lv_obj_send_event(samples_album, LV_EVENT_CLICKED, nullptr);
    lv_obj_update_layout(context.root);
    auto *empty = lv_obj_find_by_name(context.root, "gallery_empty_state");
    assert(empty && lv_obj_is_visible(empty));
    assert(std::string(lv_label_get_text(count)) == "0 photos");
    lv_obj_send_event(library_tab, LV_EVENT_CLICKED, nullptr);
    assert(std::string(lv_label_get_text(count)) == "3 photos");
    assert(!lv_obj_find_by_name(context.root, "gallery_empty_state"));

    for (const auto *album_name : {"gallery_album_camera", "gallery_album_screenshots", "gallery_album_all"}) {
        lv_obj_send_event(albums_tab, LV_EVENT_CLICKED, nullptr);
        auto *album = lv_obj_find_by_name(context.root, album_name);
        assert(album);
        // Hit the cover through LVGL to catch children that intercept the card's tap.
        auto *cover = lv_obj_get_child(album, 0);
        assert(cover);
        tap(pointer, cover);
        assert(lv_obj_is_visible(grid) && !lv_obj_is_visible(albums));
        const bool is_screenshots = std::string(album_name) == "gallery_album_screenshots";
        assert(std::string(lv_label_get_text(count)) == (is_screenshots ? "0 photos" : "3 photos"));
        assert((lv_obj_find_by_name(context.root, "gallery_empty_state") != nullptr) == is_screenshots);
    }
    lv_obj_send_event(library_tab, LV_EVENT_CLICKED, nullptr);
    tile = lv_obj_find_by_name(context.root, "gallery_tile_0");
    assert(tile);
    tap(pointer, lv_obj_find_by_name(context.root, "gallery_thumbnail_0"));
    auto *viewer       = lv_obj_find_by_name(context.root, "gallery_viewer");
    auto *index        = lv_obj_find_by_name(context.root, "gallery_viewer_index");
    auto *next         = lv_obj_find_by_name(context.root, "gallery_next");
    auto *previous     = lv_obj_find_by_name(context.root, "gallery_previous");
    auto *viewer_image = lv_obj_find_by_name(context.root, "gallery_viewer_image");
    assert(viewer && index && next && previous && viewer_image && lv_obj_is_visible(viewer));
    assert(!lv_obj_is_visible(header) && !lv_obj_is_visible(tab_bar) && !lv_obj_is_visible(grid));
    assert(lv_color_eq(lv_obj_get_style_bg_color(viewer, LV_PART_MAIN), lv_color_hex(kColorViewerBackground)));
    assert(std::string(lv_label_get_text(index)) == "1 / 3");
    assert(lv_obj_has_state(previous, LV_STATE_DISABLED));
    assert(!lv_obj_has_state(next, LV_STATE_DISABLED));

    auto *details        = lv_obj_find_by_name(context.root, "gallery_viewer_details");
    auto *details_text   = lv_obj_find_by_name(context.root, "gallery_viewer_details_text");
    auto *details_close  = lv_obj_find_by_name(context.root, "gallery_viewer_details_close");
    auto *details_header = lv_obj_find_by_name(context.root, "gallery_viewer_details_header");
    auto *details_body   = lv_obj_find_by_name(context.root, "gallery_viewer_details_body");
    auto *filename       = lv_obj_find_by_name(context.root, "gallery_viewer_filename");
    assert(details && details_text && details_close && details_header && details_body && filename);
    assert(!lv_obj_is_visible(details));
    swipe(pointer, viewer_image, 0, -40);
    assert(lv_obj_is_visible(viewer) && !lv_obj_is_visible(details));
    swipe(pointer, viewer_image, 0, 40);
    assert(lv_obj_is_visible(viewer) && !lv_obj_is_visible(grid));
    swipe(pointer, viewer_image, 0, -120);
    assert(lv_obj_is_visible(viewer) && lv_obj_is_visible(details));
    assert(std::string(lv_label_get_text(details_text)).find(lv_label_get_text(filename)) != std::string::npos);
    assert(std::string(lv_label_get_text(details_text)).find(directory) != std::string::npos);
    assert(std::string(lv_label_get_text(details_text)).find("bytes") != std::string::npos);
    const std::string first_details = lv_label_get_text(details_text);
    swipe(pointer, viewer_image, -120);
    assert(std::string(lv_label_get_text(index)) == "2 / 3");
    assert(lv_obj_is_visible(details));
    assert(std::string(lv_label_get_text(details_text)) != first_details);
    assert(std::string(lv_label_get_text(details_text)).find(lv_label_get_text(filename)) != std::string::npos);
    swipe(pointer, viewer_image, 120);
    assert(std::string(lv_label_get_text(index)) == "1 / 3");
    swipe(pointer, viewer_image, 0, 120);
    assert(lv_obj_is_visible(grid) && !lv_obj_is_visible(viewer));
    assert(lv_obj_is_visible(header) && lv_obj_is_visible(tab_bar));
    tap(pointer, lv_obj_find_by_name(context.root, "gallery_thumbnail_0"));
    assert(lv_obj_is_visible(viewer) && !lv_obj_is_visible(details));
    assert(std::string(lv_label_get_text(index)) == "1 / 3");

    lv_obj_send_event(next, LV_EVENT_CLICKED, nullptr);
    assert(std::string(lv_label_get_text(index)) == "2 / 3");
    assert(!lv_obj_has_state(previous, LV_STATE_DISABLED));
    lv_obj_send_event(next, LV_EVENT_CLICKED, nullptr);
    assert(std::string(lv_label_get_text(index)) == "3 / 3");
    assert(lv_obj_has_state(next, LV_STATE_DISABLED));
    lv_obj_send_event(previous, LV_EVENT_CLICKED, nullptr);
    assert(std::string(lv_label_get_text(index)) == "2 / 3");
    assert(!lv_obj_has_state(next, LV_STATE_DISABLED));

    auto *filmstrip_first = lv_obj_find_by_name(context.root, "gallery_filmstrip_0");
    auto *filmstrip_last  = lv_obj_find_by_name(context.root, "gallery_filmstrip_2");
    assert(filmstrip_first && filmstrip_last);
    tap(pointer, filmstrip_first);
    assert(std::string(lv_label_get_text(index)) == "1 / 3");
    assert(lv_obj_has_state(filmstrip_first, LV_STATE_CHECKED));
    tap(pointer, filmstrip_last);
    assert(std::string(lv_label_get_text(index)) == "3 / 3");
    assert(lv_obj_has_state(filmstrip_last, LV_STATE_CHECKED));
    assert(!lv_obj_has_state(filmstrip_first, LV_STATE_CHECKED));
    swipe(pointer, viewer_image, 120);
    assert(std::string(lv_label_get_text(index)) == "2 / 3");
    swipe(pointer, viewer_image, -120);
    assert(std::string(lv_label_get_text(index)) == "3 / 3");
    swipe(pointer, viewer_image, 120);
    assert(std::string(lv_label_get_text(index)) == "2 / 3");

    resize_display(display, context.root, height, width);
    assert(lv_obj_is_visible(viewer));
    assert(!lv_obj_is_visible(header) && !lv_obj_is_visible(tab_bar));
    assert(std::string(lv_label_get_text(index)) == "2 / 3");
    assert_within_display(viewer_image, height, width);
    resize_display(display, context.root, width, height);
    assert(std::string(lv_label_get_text(index)) == "2 / 3");
    assert_within_display(viewer_image, width, height);
    lv_refr_now(display);
    const auto &viewer_center =
        framebuffer[static_cast<std::size_t>(height / 2) * static_cast<std::size_t>(width) + width / 2];
    assert(!is_viewer_background(viewer_center));
    lv_area_t image_area{};
    lv_obj_get_coords(viewer_image, &image_area);
    const auto &letterbox = framebuffer[static_cast<std::size_t>(image_area.y1 + 1) * width + image_area.x1 + 1];
    assert(is_viewer_background(letterbox));
    assert(write_snapshot((std::string(argv[3]) + ".viewer.ppm").c_str(), width, height));

    swipe(pointer, viewer_image, 0, -120);
    assert(lv_obj_is_visible(details));
    assert_within_display(details, width, height);
    assert_within_display(details_close, width, height);
    lv_refr_now(display);
    assert(write_snapshot((std::string(argv[3]) + ".details.ppm").c_str(), width, height));
    const std::string selected_details = lv_label_get_text(details_text);
    resize_display(display, context.root, height, width);
    assert(lv_obj_is_visible(viewer) && lv_obj_is_visible(details));
    assert(std::string(lv_label_get_text(details_text)) == selected_details);
    assert_within_display(details, height, width);
    assert_within_display(details_close, height, width);
    resize_display(display, context.root, 320, height);
    assert(lv_obj_is_visible(details));
    assert_within_display(details, 320, height);
    assert_within_display(details_close, 320, height);

    // Exercise the details sheet alone on a short display with wrapping metadata.
    resize_display(display, context.root, 320, 240);
    assert(lv_obj_is_visible(viewer) && lv_obj_is_visible(details));
    assert_within_display(viewer_image, 320, 240);
    assert_within_display(details, 320, 240);
    assert_within_display(details_close, 320, 240);
    const std::string long_filename = "summer-holiday-" + std::string(180, 'x') + ".jpg";
    const std::string long_location =
        "/photos/" + std::string(140, 'a') + "/" + std::string(140, 'b') + "/" + long_filename;
    const std::string long_details = "Filename: " + long_filename +
                                     "\n\nModified: Sep 15, 2026\nSize: 12345 bytes"
                                     "\nSource: Camera\n\nLocation: " +
                                     long_location;
    lv_label_set_text(details_text, long_details.c_str());
    lv_obj_update_layout(context.root);
    assert(lv_obj_get_height(details_text) > lv_obj_get_content_height(details_body));
    assert(lv_obj_get_width(details_text) <= lv_obj_get_content_width(details_body));
    lv_obj_scroll_to_y(details_body, 0, LV_ANIM_OFF);
    swipe(pointer, details_body, 0, -20);
    assert(lv_obj_get_scroll_y(details_body) > 0);
    assert(lv_obj_is_visible(details) && lv_obj_is_visible(viewer));
    lv_obj_scroll_to_y(details_body, lv_obj_get_scroll_y(details_body) + lv_obj_get_scroll_bottom(details_body),
                       LV_ANIM_OFF);
    lv_obj_update_layout(context.root);
    assert(std::abs(lv_obj_get_scroll_bottom(details_body)) <= 1);
    lv_point_t last_letter{};
    lv_label_get_letter_pos(details_text, static_cast<std::uint32_t>(long_details.size() - 1), &last_letter);
    lv_area_t body_area{}, text_area{};
    lv_obj_get_coords(details_body, &body_area);
    lv_obj_get_coords(details_text, &text_area);
    assert(text_area.y1 + last_letter.y >= body_area.y1);
    assert(text_area.y1 + last_letter.y <= body_area.y2);
    assert_within_display(details_close, 320, 240);
    lv_refr_now(display);
    assert(write_snapshot((std::string(argv[3]) + ".details-compact.ppm").c_str(), 320, 240));
    tap(pointer, details_close);
    assert(lv_obj_is_visible(viewer) && !lv_obj_is_visible(details));
    assert_within_display(viewer_image, 320, 240);
    resize_display(display, context.root, width, height);
    assert(lv_obj_is_visible(viewer) && !lv_obj_is_visible(details));
    assert_within_display(viewer_image, width, height);
    assert(std::string(lv_label_get_text(index)) == "2 / 3");
    swipe(pointer, viewer_image, 0, -120);
    assert(lv_obj_is_visible(details));
    assert(std::string(lv_label_get_text(details_text)) == selected_details);
    assert(lv_obj_get_scroll_y(details_body) == 0);
    tap(pointer, details_close);
    assert(lv_obj_is_visible(viewer) && !lv_obj_is_visible(details));
    swipe(pointer, viewer_image, 0, -120);
    assert(lv_obj_is_visible(details));
    swipe(pointer, details_header, 0, 120);
    assert(lv_obj_is_visible(grid) && !lv_obj_is_visible(viewer));
    tap(pointer, lv_obj_find_by_name(context.root, "gallery_thumbnail_1"));
    assert(lv_obj_is_visible(viewer) && !lv_obj_is_visible(details));
    assert(std::string(lv_label_get_text(index)) == "2 / 3");

    auto *delete_button = lv_obj_find_by_name(context.root, "gallery_delete");
    auto *confirmation  = lv_obj_find_by_name(context.root, "gallery_delete_confirmation");
    auto *cancel        = lv_obj_find_by_name(context.root, "gallery_delete_cancel");
    auto *confirm       = lv_obj_find_by_name(context.root, "gallery_delete_confirm");
    assert(delete_button && confirmation && cancel && confirm);
    lv_obj_send_event(delete_button, LV_EVENT_CLICKED, nullptr);
    assert(!lv_obj_has_flag(confirmation, LV_OBJ_FLAG_HIDDEN));
    lv_refr_now(display);
    assert(write_snapshot((std::string(argv[3]) + ".delete.ppm").c_str(), width, height));
    lv_obj_send_event(cancel, LV_EVENT_CLICKED, nullptr);
    assert(lv_obj_has_flag(confirmation, LV_OBJ_FLAG_HIDDEN));
    lv_obj_send_event(delete_button, LV_EVENT_CLICKED, nullptr);
    lv_obj_send_event(confirm, LV_EVENT_CLICKED, nullptr);
    assert(std::string(lv_label_get_text(count)) == "2 photos");

    auto *back = lv_obj_find_by_name(context.root, "gallery_back_to_grid");
    assert(back);
    lv_obj_send_event(back, LV_EVENT_CLICKED, nullptr);
    lv_obj_update_layout(context.root);
    assert(lv_obj_is_visible(grid) && !lv_obj_is_visible(viewer));
    assert(lv_obj_is_visible(header) && lv_obj_is_visible(tab_bar));

    std::vector<std::string> additional_photos;
    for (int i = 0; i < 22; ++i) {
        additional_photos.push_back(directory + "/additional-" + std::to_string(i) + ".ppm");
        if (sample_directory && *sample_directory) {
            const char *sample_names[] = {"sunrise.ppm", "forest.ppm", "night.ppm"};
            copy_file(std::string(sample_directory) + "/" + sample_names[i % 3], additional_photos.back());
        } else {
            write_photo(additional_photos.back(), static_cast<unsigned>((i * 37) % 180),
                        static_cast<unsigned>((i * 61) % 180), static_cast<unsigned>((i * 83) % 256));
        }
    }
    auto *reload = lv_obj_find_by_name(context.root, "gallery_reload");
    assert(reload);
    lv_obj_send_event(reload, LV_EVENT_CLICKED, nullptr);
    lv_obj_update_layout(context.root);
    assert(std::string(lv_label_get_text(count)) == "24 photos");
    lv_refr_now(display);
    assert(write_snapshot((std::string(argv[3]) + ".full.ppm").c_str(), width, height));

    auto *last_tile   = lv_obj_find_by_name(context.root, "gallery_tile_23");
    auto *focus_group = lv_group_create();
    assert(last_tile && focus_group);
    lv_group_add_obj(focus_group, last_tile);
    lv_group_focus_obj(last_tile);
    lv_obj_scroll_to_view_recursive(last_tile, LV_ANIM_OFF);
    lv_obj_update_layout(context.root);
    assert(lv_obj_get_scroll_y(grid) > 0);
    resize_display(display, context.root, height, width);
    assert(lv_obj_is_visible(grid) && !lv_obj_is_visible(viewer));
    assert(lv_obj_find_by_name(context.root, "gallery_tile_23") == last_tile);
    assert(lv_group_get_focused(focus_group) == last_tile);
    assert(lv_obj_get_scroll_y(grid) > 0);
    resize_display(display, context.root, width, height);
    assert(lv_obj_find_by_name(context.root, "gallery_tile_23") == last_tile);
    assert(lv_group_get_focused(focus_group) == last_tile);
    assert(lv_obj_get_scroll_y(grid) > 0);

    tap(pointer, last_tile);
    assert(lv_obj_is_visible(viewer));
    assert(std::string(lv_label_get_text(index)) == "24 / 24");
    const auto selected_grid_scroll = lv_obj_get_scroll_y(grid);
    swipe(pointer, viewer_image, 0, -120);
    assert(lv_obj_is_visible(details));
    swipe(pointer, details_header, 0, 120);
    assert(lv_obj_is_visible(grid) && !lv_obj_is_visible(viewer));
    assert(lv_obj_get_scroll_y(grid) == selected_grid_scroll);
    tap(pointer, last_tile);
    assert(lv_obj_is_visible(viewer) && !lv_obj_is_visible(details));
    assert(std::string(lv_label_get_text(index)) == "24 / 24");
    resize_display(display, context.root, 320, height);
    auto *filmstrip  = lv_obj_find_by_name(context.root, "gallery_filmstrip");
    auto *last_frame = lv_obj_find_by_name(context.root, "gallery_filmstrip_23");
    assert(filmstrip && last_frame && lv_obj_is_visible(last_frame));
    assert(std::string(lv_label_get_text(index)) == "24 / 24");
    assert(lv_obj_has_state(last_frame, LV_STATE_CHECKED));
    assert_within_display(last_frame, 320, height);
    lv_area_t strip_area{}, frame_area{};
    lv_obj_get_coords(filmstrip, &strip_area);
    lv_obj_get_coords(last_frame, &frame_area);
    assert(frame_area.x1 >= strip_area.x1 && frame_area.x2 <= strip_area.x2);
    assert(frame_area.y1 >= strip_area.y1 && frame_area.y2 <= strip_area.y2);
    resize_display(display, context.root, width, height);
    assert(std::string(lv_label_get_text(index)) == "24 / 24");
    lv_group_delete(focus_group);

    descriptor->close();
#ifdef GALLERY_DEVELOPMENT_SAMPLE_DIR
    // Open the actual app without a sample override, as a developer would.
    const auto missing_photos = directory + "/missing-photos";
    assert(::setenv("LILYGO_GALLERY_PHOTO_DIR", missing_photos.c_str(), 1) == 0);
    assert(::setenv("LILYGO_GALLERY_SCREENSHOT_DIR", missing_photos.c_str(), 1) == 0);
    assert(::unsetenv("LILYGO_GALLERY_SAMPLE_DIR") == 0);
    GalleryModel expected_samples(missing_photos, missing_photos, "/usr/share/lilygo-ui-gallery/gallery",
                                  GALLERY_DEVELOPMENT_SAMPLE_DIR);
    expected_samples.reload();
    assert(!expected_samples.items().empty());
    descriptor->open(&context);
    lv_obj_update_layout(context.root);
    count = lv_obj_find_by_name(context.root, "gallery_count");
    assert(std::string(lv_label_get_text(count)) == std::to_string(expected_samples.items().size()) + " photos");
    lv_refr_now(display);
    assert(write_snapshot((std::string(argv[3]) + ".host-samples.ppm").c_str(), width, height));
    tap(pointer, lv_obj_find_by_name(context.root, "gallery_thumbnail_0"));
    filename = lv_obj_find_by_name(context.root, "gallery_viewer_filename");
    assert(std::string(lv_label_get_text(filename)) == expected_samples.selected_item()->filename);
    assert(lv_obj_has_state(lv_obj_find_by_name(context.root, "gallery_delete"), LV_STATE_DISABLED));
    descriptor->close();

    // Explicit sample overrides still allow an intentionally empty gallery.
    assert(::setenv("LILYGO_GALLERY_SAMPLE_DIR", missing_photos.c_str(), 1) == 0);
    descriptor->open(&context);
    lv_obj_update_layout(context.root);
    assert(lv_obj_is_visible(lv_obj_find_by_name(context.root, "gallery_empty_state")));
    descriptor->close();
#endif
    lv_indev_delete(pointer);
    lv_display_delete(display);
    lilygo_ui_fonts_deinit();
    lv_deinit();

    for (const auto &photo : photos) std::remove(photo.c_str());
    for (const auto &photo : additional_photos) assert(std::remove(photo.c_str()) == 0);
    assert(::rmdir(directory.c_str()) == 0);
    return 0;
}
