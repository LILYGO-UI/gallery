#ifndef LILYGO_UI_GALLERY_PAGES_GALLERY_GALLERY_VIEW_MODEL_HPP
#define LILYGO_UI_GALLERY_PAGES_GALLERY_GALLERY_VIEW_MODEL_HPP

#include "components/lv_subject.hpp"
#include "pages/gallery/gallery_model.hpp"

#include <cstddef>
#include <string>

class GalleryViewModel {
public:
    enum class Mode : int { grid = 0, viewer = 1 };
    enum class Collection { all, camera, screenshots, samples };

    explicit GalleryViewModel(std::string photo_directory      = {},
                              std::string screenshot_directory = {}, std::string sample_directory = {},
                              std::string fallback_directory = {}, std::string pictures_directory = {});

    void reload();
    [[nodiscard]] const std::vector<GalleryItem> &items() const noexcept;
    [[nodiscard]] const GalleryItem *selected_item() const noexcept;
    [[nodiscard]] std::size_t selected_index() const noexcept;
    [[nodiscard]] Mode mode() const noexcept;
    [[nodiscard]] Collection collection() const noexcept;
    void set_collection(Collection collection);
    [[nodiscard]] std::size_t collection_count(Collection collection) const noexcept;
    [[nodiscard]] bool can_select_previous() const noexcept;
    [[nodiscard]] bool can_select_next() const noexcept;

    [[nodiscard]] lv_subject_t *item_count_subject() noexcept;
    [[nodiscard]] lv_subject_t *selected_index_subject() noexcept;
    [[nodiscard]] lv_subject_t *mode_subject() noexcept;

    [[nodiscard]] bool open(std::size_t index) noexcept;
    void show_grid() noexcept;
    [[nodiscard]] bool select_previous() noexcept;
    [[nodiscard]] bool select_next() noexcept;
    [[nodiscard]] bool remove_selected() noexcept;

private:
    void refresh_visible_items();
    void publish() noexcept;

    GalleryModel model_;
    std::vector<GalleryItem> visible_items_;
    std::size_t visible_selected_index_ = 0;
    Collection collection_              = Collection::all;
    Mode mode_                          = Mode::grid;
    IntSubject item_count_{0, 0, static_cast<std::int32_t>(GalleryModel::maximum_items)};
    IntSubject selected_index_{-1, -1, static_cast<std::int32_t>(GalleryModel::maximum_items - 1)};
    IntSubject mode_subject_{static_cast<int>(Mode::grid), static_cast<int>(Mode::grid),
                             static_cast<int>(Mode::viewer)};
};

#endif
