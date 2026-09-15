#include "pages/gallery/gallery_view_model.hpp"

#include <algorithm>
#include <utility>

namespace {

bool belongs_to_collection(const GalleryItem &item, GalleryViewModel::Collection collection)
{
    switch (collection) {
        case GalleryViewModel::Collection::all:
            return true;
        case GalleryViewModel::Collection::camera:
            return item.source == GallerySource::camera;
        case GalleryViewModel::Collection::screenshots:
            return item.source == GallerySource::screenshots;
        case GalleryViewModel::Collection::samples:
            return item.source == GallerySource::samples;
    }
    return false;
}

}  // namespace

GalleryViewModel::GalleryViewModel(std::string photo_directory, std::string screenshot_directory,
                                   std::string sample_directory, std::string fallback_directory)
    : model_(std::move(photo_directory), std::move(screenshot_directory), std::move(sample_directory),
             std::move(fallback_directory))
{
    reload();
}

void GalleryViewModel::reload()
{
    model_.reload();
    refresh_visible_items();
    publish();
}

const std::vector<GalleryItem> &GalleryViewModel::items() const noexcept
{
    return visible_items_;
}

const GalleryItem *GalleryViewModel::selected_item() const noexcept
{
    return visible_items_.empty() ? nullptr : &visible_items_[visible_selected_index_];
}

std::size_t GalleryViewModel::selected_index() const noexcept
{
    return visible_selected_index_;
}

GalleryViewModel::Mode GalleryViewModel::mode() const noexcept
{
    return mode_;
}

GalleryViewModel::Collection GalleryViewModel::collection() const noexcept
{
    return collection_;
}

void GalleryViewModel::set_collection(Collection collection)
{
    collection_             = collection;
    visible_selected_index_ = 0;
    mode_                   = Mode::grid;
    refresh_visible_items();
    publish();
}

std::size_t GalleryViewModel::collection_count(Collection collection) const noexcept
{
    return static_cast<std::size_t>(
        std::count_if(model_.items().begin(), model_.items().end(),
                      [collection](const GalleryItem &item) { return belongs_to_collection(item, collection); }));
}

bool GalleryViewModel::can_select_previous() const noexcept
{
    return selected_item() && selected_index() > 0;
}

bool GalleryViewModel::can_select_next() const noexcept
{
    return selected_item() && selected_index() + 1 < items().size();
}

lv_subject_t *GalleryViewModel::item_count_subject() noexcept
{
    return item_count_.get();
}

lv_subject_t *GalleryViewModel::selected_index_subject() noexcept
{
    return selected_index_.get();
}

lv_subject_t *GalleryViewModel::mode_subject() noexcept
{
    return mode_subject_.get();
}

bool GalleryViewModel::open(std::size_t index) noexcept
{
    if (index >= visible_items_.size()) return false;
    visible_selected_index_ = index;
    mode_                   = Mode::viewer;
    publish();
    return true;
}

void GalleryViewModel::show_grid() noexcept
{
    mode_ = Mode::grid;
    publish();
}

bool GalleryViewModel::select_previous() noexcept
{
    const bool changed = can_select_previous();
    if (changed) --visible_selected_index_;
    publish();
    return changed;
}

bool GalleryViewModel::select_next() noexcept
{
    const bool changed = can_select_next();
    if (changed) ++visible_selected_index_;
    publish();
    return changed;
}

bool GalleryViewModel::remove_selected() noexcept
{
    const auto *item = selected_item();
    if (!item || item->read_only) return false;

    const auto &all_items = model_.items();
    const auto selected   = std::find_if(all_items.begin(), all_items.end(), [item](const GalleryItem &candidate) {
        return candidate.photo_path == item->photo_path;
    });
    if (selected == all_items.end() || !model_.select(static_cast<std::size_t>(selected - all_items.begin())))
        return false;
    if (!model_.remove_selected()) return false;
    refresh_visible_items();
    publish();
    return true;
}

void GalleryViewModel::refresh_visible_items()
{
    visible_items_.clear();
    for (const auto &item : model_.items()) {
        if (belongs_to_collection(item, collection_)) visible_items_.push_back(item);
    }
    if (visible_items_.empty()) {
        visible_selected_index_ = 0;
        mode_                   = Mode::grid;
    } else if (visible_selected_index_ >= visible_items_.size()) {
        visible_selected_index_ = visible_items_.size() - 1;
    }
}

void GalleryViewModel::publish() noexcept
{
    item_count_.set(static_cast<std::int32_t>(visible_items_.size()));
    selected_index_.set(visible_items_.empty() ? -1 : static_cast<std::int32_t>(visible_selected_index_));
    mode_subject_.set(static_cast<int>(mode_));
}
