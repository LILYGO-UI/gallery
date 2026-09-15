#include "pages/gallery/gallery_view_model.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

namespace {

void write_ppm(const std::string &path, unsigned red, unsigned green, unsigned blue)
{
    std::ofstream file(path, std::ios::binary);
    file << "P6\n1 1\n255\n";
    file.put(static_cast<char>(red));
    file.put(static_cast<char>(green));
    file.put(static_cast<char>(blue));
    file.close();
    assert(file.good());
}

std::string make_directory(const std::string &parent, const char *name)
{
    const auto path = parent + '/' + name;
    assert(::mkdir(path.c_str(), 0700) == 0);
    return path;
}

void write_image(const std::string &path, std::time_t modified)
{
    write_ppm(path, 255, 100, 50);
    struct utimbuf times{modified, modified};
    assert(::utime(path.c_str(), &times) == 0);
}

void test_all_collection()
{
    char root_template[] = "/tmp/lilygo-gallery-view-model-XXXXXX";
    const auto *created  = ::mkdtemp(root_template);
    assert(created);
    const std::string root = created;
    const auto first       = root + "/first.ppm";
    const auto second      = root + "/second.ppm";
    write_ppm(first, 255, 0, 0);
    write_ppm(second, 0, 0, 255);

    {
        GalleryViewModel view_model(root, root);
        assert(view_model.collection() == GalleryViewModel::Collection::all);
        assert(view_model.items().size() == 2);
        assert(lv_subject_get_int(view_model.item_count_subject()) == 2);
        assert(lv_subject_get_int(view_model.selected_index_subject()) == 0);
        assert(lv_subject_get_int(view_model.mode_subject()) == static_cast<int>(GalleryViewModel::Mode::grid));

        assert(view_model.open(0));
        assert(view_model.mode() == GalleryViewModel::Mode::viewer);
        assert(lv_subject_get_int(view_model.mode_subject()) == static_cast<int>(GalleryViewModel::Mode::viewer));
        assert(view_model.can_select_next());
        assert(view_model.select_next());
        assert(!view_model.can_select_next());
        assert(view_model.can_select_previous());
        assert(view_model.select_previous());
        assert(!view_model.select_previous());
        assert(!view_model.open(4));

        view_model.show_grid();
        assert(view_model.mode() == GalleryViewModel::Mode::grid);
        assert(view_model.remove_selected());
        assert(view_model.items().size() == 1);
        assert(lv_subject_get_int(view_model.item_count_subject()) == 1);
    }
    std::remove(first.c_str());
    std::remove(second.c_str());
    assert(::rmdir(root.c_str()) == 0);
}

void test_filtered_collections()
{
    using Collection = GalleryViewModel::Collection;
    using Mode       = GalleryViewModel::Mode;

    char root_template[] = "/tmp/lilygo-gallery-collections-XXXXXX";
    const auto *created  = ::mkdtemp(root_template);
    assert(created);
    const std::string root    = created;
    const auto photos         = make_directory(root, "photos");
    const auto screenshots    = make_directory(root, "screenshots");
    const auto samples        = make_directory(root, "samples");
    const auto camera_new     = photos + "/new.ppm";
    const auto camera_old     = photos + "/old.ppm";
    const auto screenshot_new = screenshots + "/new.ppm";
    const auto screenshot_old = screenshots + "/old.ppm";
    const auto sample         = samples + "/sample.ppm";
    write_image(screenshot_new, 500);
    write_image(camera_new, 400);
    write_image(screenshot_old, 300);
    write_image(camera_old, 200);
    write_image(sample, 100);

    GalleryViewModel view_model(photos, screenshots, samples);
    assert(view_model.collection_count(Collection::all) == 5);
    assert(view_model.collection_count(Collection::camera) == 2);
    assert(view_model.collection_count(Collection::screenshots) == 2);
    assert(view_model.collection_count(Collection::samples) == 1);
    assert(view_model.open(4));

    view_model.set_collection(Collection::camera);
    assert(view_model.collection() == Collection::camera);
    assert(view_model.mode() == Mode::grid);
    assert(view_model.selected_index() == 0);
    assert(view_model.items().size() == 2);
    assert(view_model.items()[0].photo_path == camera_new);
    assert(view_model.items()[1].photo_path == camera_old);
    assert(lv_subject_get_int(view_model.item_count_subject()) == 2);
    assert(lv_subject_get_int(view_model.selected_index_subject()) == 0);
    assert(view_model.open(0));
    assert(!view_model.select_previous());
    assert(view_model.select_next());
    assert(view_model.selected_item()->photo_path == camera_old);
    assert(lv_subject_get_int(view_model.selected_index_subject()) == 1);
    assert(!view_model.can_select_next());
    assert(!view_model.select_next());
    assert(!view_model.open(2));
    assert(view_model.selected_item()->photo_path == camera_old);

    // The visible index differs from the index in the mixed library.
    assert(view_model.remove_selected());
    assert(::access(camera_old.c_str(), F_OK) != 0);
    assert(::access(screenshot_old.c_str(), F_OK) == 0);
    assert(view_model.collection() == Collection::camera);
    assert(view_model.items().size() == 1);
    assert(view_model.selected_index() == 0);
    assert(view_model.selected_item()->photo_path == camera_new);
    assert(view_model.mode() == Mode::viewer);
    assert(view_model.collection_count(Collection::all) == 4);
    assert(view_model.collection_count(Collection::camera) == 1);

    assert(view_model.remove_selected());
    assert(view_model.items().empty());
    assert(view_model.selected_item() == nullptr);
    assert(view_model.mode() == Mode::grid);
    assert(lv_subject_get_int(view_model.item_count_subject()) == 0);
    assert(lv_subject_get_int(view_model.selected_index_subject()) == -1);
    assert(!view_model.can_select_previous());
    assert(!view_model.can_select_next());
    assert(!view_model.select_previous());
    assert(!view_model.select_next());
    assert(!view_model.open(0));
    assert(!view_model.remove_selected());
    assert(view_model.collection_count(Collection::all) == 3);

    view_model.set_collection(Collection::screenshots);
    assert(view_model.items().size() == 2);
    assert(view_model.open(1));
    assert(view_model.selected_item()->photo_path == screenshot_old);
    assert(std::remove(screenshot_old.c_str()) == 0);
    view_model.reload();
    assert(view_model.collection() == Collection::screenshots);
    assert(view_model.selected_index() == 0);
    assert(view_model.selected_item()->photo_path == screenshot_new);
    assert(view_model.mode() == Mode::viewer);
    assert(lv_subject_get_int(view_model.item_count_subject()) == 1);
    assert(lv_subject_get_int(view_model.selected_index_subject()) == 0);
    assert(std::remove(screenshot_new.c_str()) == 0);
    view_model.reload();
    assert(view_model.collection() == Collection::screenshots);
    assert(view_model.items().empty());
    assert(view_model.mode() == Mode::grid);
    assert(view_model.collection_count(Collection::all) == 1);

    view_model.set_collection(Collection::samples);
    assert(view_model.open(0));
    assert(view_model.selected_item()->read_only);
    assert(!view_model.remove_selected());
    assert(::access(sample.c_str(), F_OK) == 0);
    assert(view_model.items().size() == 1);
    assert(view_model.mode() == Mode::viewer);
    view_model.set_collection(Collection::all);
    assert(view_model.mode() == Mode::grid);
    assert(view_model.items()[0].photo_path == sample);

    view_model.set_collection(Collection::camera);
    write_image(camera_new, 600);
    view_model.reload();
    assert(view_model.collection() == Collection::camera);
    assert(view_model.items().size() == 1);
    assert(view_model.selected_item()->photo_path == camera_new);
    assert(view_model.mode() == Mode::grid);
    assert(view_model.collection_count(Collection::all) == 2);
    assert(view_model.collection_count(Collection::camera) == 1);
    assert(view_model.collection_count(Collection::screenshots) == 0);
    assert(view_model.collection_count(Collection::samples) == 1);

    assert(std::remove(camera_new.c_str()) == 0);
    assert(std::remove(sample.c_str()) == 0);
    assert(::rmdir(photos.c_str()) == 0);
    assert(::rmdir(screenshots.c_str()) == 0);
    assert(::rmdir(samples.c_str()) == 0);
    assert(::rmdir(root.c_str()) == 0);
}

}  // namespace

int main()
{
    lv_init();
    test_all_collection();
    test_filtered_collections();
    lv_deinit();
    return 0;
}
