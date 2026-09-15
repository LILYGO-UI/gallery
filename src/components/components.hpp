#ifndef LILYGO_UI_GALLERY_COMPONENTS_HPP
#define LILYGO_UI_GALLERY_COMPONENTS_HPP

#include <lvgl.h>

#include <cstdint>

namespace components {

inline constexpr std::uint32_t color_page_background      = 0xffffff;
inline constexpr std::uint32_t color_container_background = 0xffffff;
inline constexpr std::uint32_t color_container_title      = 0x1a1a1a;
inline constexpr std::uint32_t color_text                 = 0x000000;
inline constexpr std::uint32_t color_border               = 0xe5e5ea;
inline constexpr std::uint32_t color_action               = 0x007aff;
inline constexpr std::uint32_t color_muted_text           = 0x8e8e93;
inline constexpr std::uint32_t color_control_background   = 0xf2f2f7;
inline constexpr std::uint32_t color_navigation_accent    = 0x007aff;
inline constexpr std::uint32_t color_destructive          = 0xff3b30;
inline constexpr std::uint32_t color_viewer_background    = color_page_background;
inline constexpr std::uint32_t color_viewer_overlay       = 0x090b0e;

lv_obj_t *create_page(lv_obj_t *parent);
lv_obj_t *create_header(lv_obj_t *parent, const char *title);
lv_obj_t *create_card(lv_obj_t *parent);
lv_obj_t *create_button(lv_obj_t *parent, const char *text, lv_event_cb_t callback, void *user_data);

}  // namespace components

#endif
