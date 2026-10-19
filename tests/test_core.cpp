#include "pages/gallery/gallery_model.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

namespace {

std::string make_directory(const std::string &parent, const char *name)
{
    const auto path = parent + '/' + name;
    assert(::mkdir(path.c_str(), 0700) == 0);
    return path;
}

void write_file(const std::string &path, const std::string &contents, std::time_t modified)
{
    std::ofstream file(path, std::ios::binary);
    file << contents;
    file.close();
    assert(file.good());
    struct utimbuf times{modified, modified};
    assert(::utime(path.c_str(), &times) == 0);
}

void test_model()
{
    char root_template[] = "/tmp/lilygo-gallery-model-XXXXXX";
    const auto *created  = ::mkdtemp(root_template);
    assert(created);
    const std::string root = created;
    const auto photos      = make_directory(root, "photos");
    const auto screenshots = make_directory(root, "screenshots");
    const auto samples     = make_directory(root, "samples");

    const auto photo  = photos + "/camera-001.jpg";
    const auto thumb  = photos + "/camera-001.thumb.rgb565";
    const auto shot   = screenshots + "/screen-002.png";
    const auto sample = samples + "/sample.png";
    write_file(photo, "jpeg", 100);
    write_file(thumb, "thumb", 100);
    write_file(shot, "png", 200);
    write_file(sample, "png", 50);
    write_file(photos + "/notes.txt", "ignored", 300);

    GalleryModel model(photos, screenshots, samples);
    model.reload();
    assert(model.items().size() == 3);
    assert(model.items()[0].photo_path == shot);
    assert(model.items()[0].thumbnail_path == shot);
    assert(model.items()[0].source == GallerySource::screenshots);
    assert(model.items()[0].size_bytes == 3);
    assert(model.items()[1].photo_path == photo);
    assert(model.items()[1].thumbnail_path == thumb);
    assert(model.items()[1].source == GallerySource::camera);
    assert(model.items()[1].size_bytes == 4);
    assert(model.items()[2].photo_path == sample);
    assert(model.items()[2].read_only);
    assert(model.items()[2].source == GallerySource::samples);
    assert(model.items()[2].size_bytes == 3);

    const std::string updated_shot = "updated png data";
    write_file(shot, updated_shot, 200);
    write_file(sample, "", 50);
    model.reload();
    assert(model.items()[0].size_bytes == updated_shot.size());
    assert(model.items()[2].size_bytes == 0);

    GalleryModel shared_directory(photos, photos, photos);
    shared_directory.reload();
    assert(shared_directory.items().size() == 1);
    assert(shared_directory.items()[0].source == GallerySource::camera);
    assert(!shared_directory.items()[0].read_only);

    assert(model.selected_item() == &model.items()[0]);
    assert(!model.select_previous());
    assert(model.select_next());
    assert(model.selected_item()->filename == "camera-001.jpg");
    assert(model.select_next());
    assert(model.selected_item()->photo_path == sample);
    assert(!model.remove_selected());
    assert(::access(sample.c_str(), F_OK) == 0);
    assert(model.select_previous());

    assert(model.remove_selected());
    assert(::access(photo.c_str(), F_OK) != 0);
    assert(::access(thumb.c_str(), F_OK) != 0);
    assert(model.items().size() == 2);
    assert(model.selected_index() == 1);

    assert(std::remove(shot.c_str()) == 0);
    assert(std::remove(sample.c_str()) == 0);
    assert(std::remove((photos + "/notes.txt").c_str()) == 0);
    assert(::rmdir(photos.c_str()) == 0);
    assert(::rmdir(screenshots.c_str()) == 0);
    assert(::rmdir(samples.c_str()) == 0);
    assert(::rmdir(root.c_str()) == 0);
}

void test_development_samples_fallback()
{
    char root_template[] = "/tmp/lilygo-gallery-fallback-XXXXXX";
    const auto *created  = ::mkdtemp(root_template);
    assert(created);
    const std::string root = created;
    const auto photos      = make_directory(root, "photos");
    const auto screenshots = make_directory(root, "screenshots");
    const auto samples     = make_directory(root, "samples");
    const auto fallback    = make_directory(root, "fallback");
    const auto photo       = photos + "/camera.jpg";
    const auto shot        = screenshots + "/screen.png";
    const auto sample      = fallback + "/development.png";
    const auto ignored     = photos + "/notes.txt";
    write_file(photo, "jpeg", 100);
    write_file(sample, "png", 200);
    write_file(ignored, "not an image", 300);

    GalleryModel model(photos, screenshots, samples, fallback);
    model.reload();
    assert(model.items().size() == 1);
    assert(model.selected_item()->photo_path == photo);
    assert(model.selected_item()->source == GallerySource::camera);
    assert(!model.selected_item()->read_only);

    assert(std::remove(photo.c_str()) == 0);
    model.reload();
    assert(model.items().size() == 1);
    assert(model.selected_item()->photo_path == sample);
    assert(model.selected_item()->source == GallerySource::samples);
    assert(model.selected_item()->read_only);
    assert(!model.remove_selected());
    assert(::access(sample.c_str(), F_OK) == 0);

    write_file(shot, "png", 50);
    model.reload();
    assert(model.items().size() == 1);
    assert(model.selected_item()->photo_path == shot);
    assert(model.selected_item()->source == GallerySource::screenshots);
    assert(!model.selected_item()->read_only);

    assert(std::remove(shot.c_str()) == 0);
    assert(std::remove(sample.c_str()) == 0);
    assert(std::remove(ignored.c_str()) == 0);
    assert(::rmdir(photos.c_str()) == 0);
    assert(::rmdir(screenshots.c_str()) == 0);
    assert(::rmdir(samples.c_str()) == 0);
    assert(::rmdir(fallback.c_str()) == 0);
    assert(::rmdir(root.c_str()) == 0);
}

void test_default_camera_directory_uses_home()
{
    char root_template[] = "/tmp/lilygo-gallery-home-XXXXXX";
    const auto *created  = ::mkdtemp(root_template);
    assert(created);
    const std::string root   = created;
    const std::string camera = make_directory(root, "Camera");
    const std::string photo  = camera + "/capture.jpg";
    write_file(photo, "jpeg", 100);

    const char *previous_home = std::getenv("HOME");
    const bool had_home       = previous_home && *previous_home;
    const std::string old_home = had_home ? previous_home : std::string();
    assert(::setenv("HOME", root.c_str(), 1) == 0);

    assert(gallery_default_photo_directory() == camera);
    GalleryModel model;
    model.reload();
    assert(model.items().size() == 1);
    assert(model.items()[0].photo_path == photo);
    assert(model.items()[0].source == GallerySource::camera);

    if (had_home) {
        assert(::setenv("HOME", old_home.c_str(), 1) == 0);
    } else {
        assert(::unsetenv("HOME") == 0);
    }
    assert(std::remove(photo.c_str()) == 0);
    assert(::rmdir(camera.c_str()) == 0);
    assert(::rmdir(root.c_str()) == 0);
}

void test_default_screenshot_directory_uses_home()
{
    char root_template[] = "/tmp/lilygo-gallery-screenshots-home-XXXXXX";
    const auto *created  = ::mkdtemp(root_template);
    assert(created);
    const std::string root        = created;
    const std::string camera      = make_directory(root, "Camera");
    const std::string screenshots = make_directory(root, "screenshots");
    const std::string photo       = camera + "/capture.jpg";
    const std::string screenshot  = screenshots + "/screen.png";
    write_file(photo, "jpeg", 100);
    write_file(screenshot, "png", 200);

    const char *previous_home = std::getenv("HOME");
    const bool had_home       = previous_home && *previous_home;
    const std::string old_home = had_home ? previous_home : std::string();
    assert(::setenv("HOME", root.c_str(), 1) == 0);

    assert(gallery_default_screenshot_directory() == screenshots);
    GalleryModel model;
    model.reload();
    assert(model.items().size() == 2);
    assert(model.items()[0].photo_path == screenshot);
    assert(model.items()[0].source == GallerySource::screenshots);

    if (had_home) {
        assert(::setenv("HOME", old_home.c_str(), 1) == 0);
    } else {
        assert(::unsetenv("HOME") == 0);
    }
    assert(std::remove(photo.c_str()) == 0);
    assert(std::remove(screenshot.c_str()) == 0);
    assert(::rmdir(camera.c_str()) == 0);
    assert(::rmdir(screenshots.c_str()) == 0);
    assert(::rmdir(root.c_str()) == 0);
}

void test_pictures_directory_is_writable_gallery_source()
{
    char root_template[] = "/tmp/lilygo-gallery-pictures-home-XXXXXX";
    const auto *created  = ::mkdtemp(root_template);
    assert(created);
    const std::string root        = created;
    const std::string camera      = make_directory(root, "Camera");
    const std::string screenshots = make_directory(root, "screenshots");
    const std::string pictures    = make_directory(root, "Pictures");
    const std::string photo       = pictures + "/sample.jpg";
    write_file(photo, "jpeg", 100);

    const char *previous_home = std::getenv("HOME");
    const bool had_home       = previous_home && *previous_home;
    const std::string old_home = had_home ? previous_home : std::string();
    assert(::setenv("HOME", root.c_str(), 1) == 0);
    assert(gallery_default_pictures_directory() == pictures);

    GalleryModel model(camera, screenshots, {}, {}, pictures);
    model.reload();
    assert(model.items().size() == 1);
    assert(model.items()[0].photo_path == photo);
    assert(model.items()[0].source == GallerySource::pictures);
    assert(!model.items()[0].read_only);
    assert(model.remove_selected());
    assert(::access(photo.c_str(), F_OK) != 0);

    if (had_home) {
        assert(::setenv("HOME", old_home.c_str(), 1) == 0);
    } else {
        assert(::unsetenv("HOME") == 0);
    }
    assert(::rmdir(camera.c_str()) == 0);
    assert(::rmdir(screenshots.c_str()) == 0);
    assert(::rmdir(pictures.c_str()) == 0);
    assert(::rmdir(root.c_str()) == 0);
}

}  // namespace

int main()
{
    test_model();
    test_development_samples_fallback();
    test_default_camera_directory_uses_home();
    test_default_screenshot_directory_uses_home();
    test_pictures_directory_is_writable_gallery_source();
    return 0;
}
