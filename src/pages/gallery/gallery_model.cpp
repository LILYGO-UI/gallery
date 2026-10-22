#include "pages/gallery/gallery_model.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <utility>

namespace {

bool is_regular_file(const std::string &path, struct stat *information = nullptr)
{
    struct stat local_information{};
    auto *result = information ? information : &local_information;
    return ::stat(path.c_str(), result) == 0 && S_ISREG(result->st_mode);
}

std::string lowercase_extension(const std::string &name)
{
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos) return {};

    std::string extension = name.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

bool is_supported_image(const std::string &name)
{
    const auto extension = lowercase_extension(name);
    return extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".ppm";
}

bool is_supported_video(const std::string &name)
{
    // Camera writes H.264 in an MP4 container. Keep the scanner limited to
    // formats the device camera actually produces until another decoder is
    // added; accepting an arbitrary container would make an item appear in
    // the library without a player that can open it.
    return lowercase_extension(name) == ".mp4";
}

bool is_incomplete_video(const std::string &name)
{
    constexpr char suffix[] = ".part.mp4";
    constexpr std::size_t suffix_length = sizeof(suffix) - 1;
    if (name.size() < suffix_length) return false;
    const auto start = name.size() - suffix_length;
    for (std::size_t offset = 0; offset < suffix_length; ++offset) {
        if (std::tolower(static_cast<unsigned char>(name[start + offset])) !=
            static_cast<unsigned char>(suffix[offset]))
            return false;
    }
    return true;
}

std::string camera_thumbnail_path(const std::string &directory, const std::string &name)
{
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos) return {};
    return directory + '/' + name.substr(0, dot) + ".thumb.rgb565";
}

}  // namespace

std::string gallery_default_photo_directory()
{
    const char *camera_output = std::getenv("CAMERA_OUTPUT_DIR");
    if (camera_output && *camera_output) return camera_output;
    const char *home = std::getenv("HOME");
    if (!home || !*home) return {};
    return std::string(home) + "/Camera";
}

std::string gallery_default_screenshot_directory()
{
    const char *home = std::getenv("HOME");
    if (!home || !*home) return {};
    return std::string(home) + "/screenshots";
}

std::string gallery_default_pictures_directory()
{
    const char *home = std::getenv("HOME");
    if (!home || !*home) return {};
    return std::string(home) + "/Pictures";
}

std::string gallery_media_thumbnail_path(const std::string &media_path)
{
    const auto slash = media_path.find_last_of('/');
    const auto dot   = media_path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return {};
    return media_path.substr(0, dot) + ".thumb.rgb565";
}

GalleryModel::GalleryModel(std::string photo_directory, std::string screenshot_directory, std::string sample_directory,
                           std::string fallback_directory, std::string pictures_directory)
    : photo_directory_(photo_directory.empty() ? gallery_default_photo_directory() : std::move(photo_directory)),
      screenshot_directory_(screenshot_directory.empty() ? gallery_default_screenshot_directory()
                                                          : std::move(screenshot_directory)),
      sample_directory_(std::move(sample_directory)),
      fallback_directory_(std::move(fallback_directory)),
      pictures_directory_(pictures_directory.empty() && photo_directory.empty() && screenshot_directory.empty()
                              ? gallery_default_pictures_directory()
                              : std::move(pictures_directory))
{
}

void GalleryModel::reload()
{
    items_.clear();
    scan_directory(photo_directory_, GallerySource::camera, false);
    if (!pictures_directory_.empty() && pictures_directory_ != photo_directory_ &&
        pictures_directory_ != screenshot_directory_) {
        scan_directory(pictures_directory_, GallerySource::pictures, false);
    }
    if (screenshot_directory_ != photo_directory_ && screenshot_directory_ != pictures_directory_) {
        scan_directory(screenshot_directory_, GallerySource::screenshots, false);
    }
    if (!sample_directory_.empty() && sample_directory_ != photo_directory_ &&
        sample_directory_ != screenshot_directory_ && sample_directory_ != pictures_directory_) {
        scan_directory(sample_directory_, GallerySource::samples, true);
    }

    if (items_.empty() && !fallback_directory_.empty()) {
        scan_directory(fallback_directory_, GallerySource::samples, true);
    }

    std::sort(items_.begin(), items_.end(), [](const GalleryItem &left, const GalleryItem &right) {
        if (left.modified_time != right.modified_time) return left.modified_time > right.modified_time;
        return left.photo_path > right.photo_path;
    });
    if (items_.size() > maximum_items) items_.resize(maximum_items);
    if (items_.empty()) {
        selected_index_ = 0;
    } else if (selected_index_ >= items_.size()) {
        selected_index_ = items_.size() - 1;
    }
}

const std::vector<GalleryItem> &GalleryModel::items() const noexcept
{
    return items_;
}

std::size_t GalleryModel::selected_index() const noexcept
{
    return selected_index_;
}

const GalleryItem *GalleryModel::selected_item() const noexcept
{
    return items_.empty() ? nullptr : &items_[selected_index_];
}

bool GalleryModel::select(std::size_t index) noexcept
{
    if (index >= items_.size()) return false;
    selected_index_ = index;
    return true;
}

bool GalleryModel::select_previous() noexcept
{
    if (items_.empty() || selected_index_ == 0) return false;
    --selected_index_;
    return true;
}

bool GalleryModel::select_next() noexcept
{
    if (items_.empty() || selected_index_ + 1 >= items_.size()) return false;
    ++selected_index_;
    return true;
}

bool GalleryModel::remove_selected() noexcept
{
    const auto *item = selected_item();
    if (!item || item->read_only) return false;

    const auto photo_path     = item->photo_path;
    auto thumbnail_path = item->thumbnail_path;
    if (item->kind == GalleryMediaKind::video && thumbnail_path == photo_path)
        thumbnail_path = gallery_media_thumbnail_path(photo_path);
    if (std::remove(photo_path.c_str()) != 0 && errno != ENOENT) return false;
    if (thumbnail_path != photo_path) std::remove(thumbnail_path.c_str());

    reload();
    return true;
}

void GalleryModel::scan_directory(const std::string &directory, GallerySource source, bool read_only)
{
    auto *folder = ::opendir(directory.c_str());
    if (!folder) return;

    while (const auto *entry = ::readdir(folder)) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || is_incomplete_video(name)) continue;
        const bool image = is_supported_image(name);
        const bool video = !image && is_supported_video(name);
        if (!image && !video) continue;
        const auto media_kind = video ? GalleryMediaKind::video : GalleryMediaKind::photo;

        const auto photo_path = directory + '/' + name;
        struct stat information{};
        if (!is_regular_file(photo_path, &information)) continue;

        auto thumbnail_path = photo_path;
        if (media_kind == GalleryMediaKind::photo && source != GallerySource::screenshots) {
            const auto stored_thumbnail = camera_thumbnail_path(directory, name);
            if (is_regular_file(stored_thumbnail)) thumbnail_path = stored_thumbnail;
        } else if (media_kind == GalleryMediaKind::video) {
            const auto stored_thumbnail = camera_thumbnail_path(directory, name);
            if (is_regular_file(stored_thumbnail)) thumbnail_path = stored_thumbnail;
        }
        const auto size_bytes = information.st_size >= 0 ? static_cast<std::uint64_t>(information.st_size) : 0;
        items_.push_back({photo_path, thumbnail_path, name, static_cast<std::int64_t>(information.st_mtime), read_only,
                          source, size_bytes, media_kind});
    }
    ::closedir(folder);
}
