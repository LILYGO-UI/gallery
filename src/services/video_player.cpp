#include "services/video_player.hpp"

#include <mpv/client.h>
#include <mpv/render.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <initializer_list>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kTimePositionProperty = 1;
constexpr int kDurationProperty     = 2;
constexpr int kPauseProperty        = 3;
constexpr int kBufferingProperty    = 4;
constexpr int kVolumeProperty       = 5;
constexpr int kEofProperty          = 6;
constexpr int kAudioTrackProperty   = 7;
constexpr int kVideoWidthProperty   = 8;
constexpr int kVideoHeightProperty  = 9;
constexpr int kContainerProperty    = 10;
constexpr int kVideoCodecProperty   = 11;

std::string number_argument(double value)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << value;
    return stream.str();
}

bool write_rgb565_file(const std::string &path, const std::vector<std::uint8_t> &pixels)
{
    if (path.empty() || pixels.size() != static_cast<std::size_t>(GalleryVideoPlayer::thumbnail_width) *
                                             static_cast<std::size_t>(GalleryVideoPlayer::thumbnail_height) * 2U)
        return false;

    const auto temporary = path + ".part";
    auto *file           = std::fopen(temporary.c_str(), "wb");
    if (!file) return false;
    const auto written = std::fwrite(pixels.data(), 1, pixels.size(), file);
    const bool flushed = written == pixels.size() && std::fflush(file) == 0;
    const bool closed  = std::fclose(file) == 0;
    if (!flushed || !closed) {
        std::remove(temporary.c_str());
        return false;
    }
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}

void convert_bgr0_to_rgb565(const std::vector<std::uint8_t> &bgr0, int width, int height,
                            std::vector<std::uint8_t> &rgb565)
{
    const auto pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (bgr0.size() < pixels * 4U) {
        rgb565.clear();
        return;
    }
    rgb565.resize(pixels * 2U);
    for (std::size_t index = 0; index < pixels; ++index) {
        const auto blue    = bgr0[index * 4U];
        const auto green   = bgr0[index * 4U + 1U];
        const auto red     = bgr0[index * 4U + 2U];
        const auto value   = static_cast<std::uint16_t>(((red & 0xf8U) << 8U) | ((green & 0xfcU) << 3U) | (blue >> 3U));
        rgb565[index * 2U] = static_cast<std::uint8_t>(value & 0xffU);
        rgb565[index * 2U + 1] = static_cast<std::uint8_t>(value >> 8U);
    }
}

}  // namespace

struct GalleryVideoPlayer::Impl {
    void *library               = nullptr;
    mpv_handle *handle          = nullptr;
    mpv_render_context *context = nullptr;
    std::atomic_bool dirty{true};
    GalleryVideoPlayback playback;
    bool silent            = true;
    bool loaded            = false;
    bool paused            = false;
    bool eof               = false;
    bool thumbnail_mode    = false;
    int last_render_width  = 0;
    int last_render_height = 0;

#define MPV_FUNCTION(name) decltype(&name) name##_fn = nullptr
    MPV_FUNCTION(mpv_create);
    MPV_FUNCTION(mpv_initialize);
    MPV_FUNCTION(mpv_set_option_string);
    MPV_FUNCTION(mpv_set_property_async);
    MPV_FUNCTION(mpv_command_async);
    MPV_FUNCTION(mpv_observe_property);
    MPV_FUNCTION(mpv_get_property_string);
    MPV_FUNCTION(mpv_free);
    MPV_FUNCTION(mpv_wait_event);
    MPV_FUNCTION(mpv_error_string);
    MPV_FUNCTION(mpv_request_log_messages);
    MPV_FUNCTION(mpv_terminate_destroy);
    MPV_FUNCTION(mpv_render_context_create);
    MPV_FUNCTION(mpv_render_context_set_update_callback);
    MPV_FUNCTION(mpv_render_context_update);
    MPV_FUNCTION(mpv_render_context_render);
    MPV_FUNCTION(mpv_render_context_free);
#undef MPV_FUNCTION

    explicit Impl(bool quiet) : silent(quiet)
    {
    }

    ~Impl()
    {
        release();
    }

    void release()
    {
        if (context) {
            mpv_render_context_set_update_callback_fn(context, nullptr, nullptr);
            mpv_render_context_free_fn(context);
            context = nullptr;
        }
        if (handle) {
            mpv_terminate_destroy_fn(handle);
            handle = nullptr;
        }
        if (library) {
            dlclose(library);
            library = nullptr;
        }
        loaded = false;
    }

    template <typename T>
    void symbol(T &pointer, const char *name)
    {
        pointer = reinterpret_cast<T>(dlsym(library, name));
        if (!pointer) throw std::runtime_error(std::string("Incompatible libmpv: ") + name);
    }

    void option(const char *key, const char *value)
    {
        const int error = mpv_set_option_string_fn(handle, key, value);
        if (error < 0) throw std::runtime_error(std::string(key) + ": " + mpv_error_string_fn(error));
    }

    bool initialize()
    {
        if (handle) return true;
        try {
            for (const char *name : {"libmpv.so.2", "libmpv.so", "libmpv.2.dylib", "/opt/homebrew/lib/libmpv.2.dylib",
                                     "/usr/local/lib/libmpv.2.dylib"}) {
                library = dlopen(name, RTLD_NOW | RTLD_LOCAL);
                if (library) break;
            }
            if (!library) throw std::runtime_error("libmpv is unavailable. Install libmpv2 on the device.");

#define LOAD(name) symbol(name##_fn, #name)
            LOAD(mpv_create);
            LOAD(mpv_initialize);
            LOAD(mpv_set_option_string);
            LOAD(mpv_set_property_async);
            LOAD(mpv_command_async);
            LOAD(mpv_observe_property);
            LOAD(mpv_get_property_string);
            LOAD(mpv_free);
            LOAD(mpv_wait_event);
            LOAD(mpv_error_string);
            LOAD(mpv_request_log_messages);
            LOAD(mpv_terminate_destroy);
            LOAD(mpv_render_context_create);
            LOAD(mpv_render_context_set_update_callback);
            LOAD(mpv_render_context_update);
            LOAD(mpv_render_context_render);
            LOAD(mpv_render_context_free);
#undef LOAD

            handle = mpv_create_fn();
            if (!handle) throw std::runtime_error("Cannot create libmpv player");
            option("config", "no");
            option("load-scripts", "no");
            option("vo", "libmpv");
            option("terminal", "no");
            option("input-default-bindings", "no");
            option("input-vo-keyboard", "no");
            option("osc", "no");
            option("osd-level", "0");
            option("video-timing-offset", "0");
            option("hwdec", "auto-copy");
            option("vd-lavc-threads", "2");
            option("demuxer-max-bytes", "16MiB");
            option("demuxer-max-back-bytes", "4MiB");
            option("keep-open", "yes");
            const auto volume = number_argument(playback.volume);
            option("volume", volume.c_str());
            if (silent) option("ao", "null");

            int error = mpv_initialize_fn(handle);
            if (error < 0) throw std::runtime_error(mpv_error_string_fn(error));
            mpv_request_log_messages_fn(handle, "error");
            mpv_observe_property_fn(handle, kTimePositionProperty, "time-pos", MPV_FORMAT_DOUBLE);
            mpv_observe_property_fn(handle, kDurationProperty, "duration", MPV_FORMAT_DOUBLE);
            mpv_observe_property_fn(handle, kPauseProperty, "pause", MPV_FORMAT_FLAG);
            mpv_observe_property_fn(handle, kBufferingProperty, "paused-for-cache", MPV_FORMAT_FLAG);
            mpv_observe_property_fn(handle, kVolumeProperty, "volume", MPV_FORMAT_DOUBLE);
            mpv_observe_property_fn(handle, kEofProperty, "eof-reached", MPV_FORMAT_FLAG);
            mpv_observe_property_fn(handle, kAudioTrackProperty, "aid", MPV_FORMAT_INT64);
            mpv_observe_property_fn(handle, kVideoWidthProperty, "width", MPV_FORMAT_INT64);
            mpv_observe_property_fn(handle, kVideoHeightProperty, "height", MPV_FORMAT_INT64);
            mpv_observe_property_fn(handle, kContainerProperty, "file-format", MPV_FORMAT_STRING);
            mpv_observe_property_fn(handle, kVideoCodecProperty, "video-codec", MPV_FORMAT_STRING);

            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_SW)},
                {MPV_RENDER_PARAM_INVALID, nullptr},
            };
            error = mpv_render_context_create_fn(&context, handle, params);
            if (error < 0) throw std::runtime_error(std::string("Video renderer: ") + mpv_error_string_fn(error));
            mpv_render_context_set_update_callback_fn(
                context, [](void *data) { static_cast<Impl *>(data)->dirty.store(true); }, this);
            return true;
        } catch (const std::exception &error) {
            playback.state = GalleryVideoPlaybackState::error;
            playback.error = error.what();
            release();
            return false;
        }
    }

    void command(std::initializer_list<const char *> args)
    {
        if (!handle) return;
        std::vector<const char *> argv(args.begin(), args.end());
        argv.push_back(nullptr);
        const int error = mpv_command_async_fn(handle, 0, argv.data());
        if (error < 0) {
            playback.state = GalleryVideoPlaybackState::error;
            playback.error = mpv_error_string_fn(error);
        }
    }

    void update_state()
    {
        if (!loaded) return;
        playback.state = eof ? GalleryVideoPlaybackState::ended
                             : (paused ? GalleryVideoPlaybackState::paused : GalleryVideoPlaybackState::playing);
    }

    std::string string_property(const std::string &name)
    {
        auto *value = mpv_get_property_string_fn(handle, name.c_str());
        if (!value) return {};
        std::string result(value);
        mpv_free_fn(value);
        return result;
    }

    void update_audio_codec()
    {
        playback.audio_codec.clear();
        std::istringstream count_text(string_property("track-list/count"));
        int count = 0;
        if (!(count_text >> count) || count < 0 || count > 1024) return;
        for (int index = 0; index < count; ++index) {
            const auto track = "track-list/" + std::to_string(index) + '/';
            if (string_property(track + "type") != "audio") continue;
            const auto codec = string_property(track + "codec");
            if (string_property(track + "selected") == "yes") {
                playback.audio_codec = codec;
                return;
            }
            if (playback.audio_codec.empty()) playback.audio_codec = codec;
        }
    }
};

GalleryVideoPlayer::GalleryVideoPlayer(bool silent) : impl_(std::make_unique<Impl>(silent))
{
}

GalleryVideoPlayer::~GalleryVideoPlayer() = default;

void GalleryVideoPlayer::load(const std::string &path)
{
    auto &player              = *impl_;
    const auto volume         = player.playback.volume;
    player.playback           = {};
    player.playback.volume    = volume;
    player.playback.state     = GalleryVideoPlaybackState::loading;
    player.loaded             = false;
    player.paused             = false;
    player.eof                = false;
    player.last_render_width  = 0;
    player.last_render_height = 0;
    player.dirty.store(true);
    if (!player.initialize()) return;

    if (player.thumbnail_mode) player.command({"set", "keepaspect", "no"});
    player.command({"set", "pause", "no"});
    player.command({"loadfile", path.c_str(), "replace"});
}

void GalleryVideoPlayer::stop()
{
    auto &player      = *impl_;
    const auto volume = player.playback.volume;
    player.command({"stop"});
    player.loaded             = false;
    player.paused             = false;
    player.eof                = false;
    player.last_render_width  = 0;
    player.last_render_height = 0;
    player.thumbnail_mode     = false;
    player.playback           = {};
    player.playback.volume    = volume;
    player.dirty.store(true);
}

void GalleryVideoPlayer::toggle_pause()
{
    if (impl_->eof) {
        seek(0.0);
        impl_->command({"set", "pause", "no"});
        return;
    }
    impl_->command({"cycle", "pause"});
}

void GalleryVideoPlayer::seek(double seconds)
{
    auto &player = *impl_;
    if (!player.handle || !player.loaded || !std::isfinite(seconds)) return;

    const bool resume_after_eof = player.eof;
    seconds                     = std::max(0.0, seconds);
    if (player.playback.duration > 0.0) seconds = std::min(seconds, player.playback.duration);
    const auto target = number_argument(seconds);
    player.command({"seek", target.c_str(), "absolute+exact"});
    if (resume_after_eof) {
        player.command({"set", "pause", "no"});
        player.paused = false;
    }
    player.playback.position = seconds;
    player.eof               = false;
    player.update_state();
}

void GalleryVideoPlayer::set_volume(double percent)
{
    auto &player = *impl_;
    if (!std::isfinite(percent)) return;
    percent                = std::clamp(percent, 0.0, 100.0);
    player.playback.volume = percent;
    if (!player.handle) return;

    const int error = player.mpv_set_property_async_fn(player.handle, 0, "volume", MPV_FORMAT_DOUBLE, &percent);
    if (error < 0) {
        player.playback.state = GalleryVideoPlaybackState::error;
        player.playback.error = player.mpv_error_string_fn(error);
    }
}

double GalleryVideoPlayer::volume() const
{
    return impl_->playback.volume;
}

const GalleryVideoPlayback &GalleryVideoPlayer::poll()
{
    auto &player = *impl_;
    if (!player.handle) return player.playback;

    for (int count = 0; count < 100; ++count) {
        const auto *event = player.mpv_wait_event_fn(player.handle, 0);
        if (!event || event->event_id == MPV_EVENT_NONE) break;
        if (event->event_id == MPV_EVENT_START_FILE) {
            player.loaded            = false;
            player.playback.state    = GalleryVideoPlaybackState::loading;
            player.playback.position = 0.0;
        } else if (event->event_id == MPV_EVENT_FILE_LOADED) {
            player.loaded = true;
            player.playback.error.clear();
            player.update_audio_codec();
            player.update_state();
        } else if (event->event_id == MPV_EVENT_END_FILE) {
            const auto *end = static_cast<const mpv_event_end_file *>(event->data);
            if (!end) continue;
            if (end->reason == MPV_END_FILE_REASON_ERROR) {
                player.loaded         = false;
                player.playback.state = GalleryVideoPlaybackState::error;
                if (player.playback.error.empty()) player.playback.error = player.mpv_error_string_fn(end->error);
            } else if (end->reason == MPV_END_FILE_REASON_EOF) {
                player.eof            = true;
                player.playback.state = GalleryVideoPlaybackState::ended;
            }
        } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto *property = static_cast<const mpv_event_property *>(event->data);
            if (!property || !property->data) continue;
            if (event->reply_userdata == kTimePositionProperty && property->format == MPV_FORMAT_DOUBLE)
                player.playback.position = *static_cast<const double *>(property->data);
            else if (event->reply_userdata == kDurationProperty && property->format == MPV_FORMAT_DOUBLE)
                player.playback.duration = *static_cast<const double *>(property->data);
            else if (event->reply_userdata == kPauseProperty && property->format == MPV_FORMAT_FLAG) {
                player.paused = *static_cast<const int *>(property->data) != 0;
                player.update_state();
            } else if (event->reply_userdata == kBufferingProperty && property->format == MPV_FORMAT_FLAG) {
                player.playback.buffering = *static_cast<const int *>(property->data) != 0;
            } else if (event->reply_userdata == kVolumeProperty && property->format == MPV_FORMAT_DOUBLE)
                player.playback.volume = *static_cast<const double *>(property->data);
            else if (event->reply_userdata == kEofProperty && property->format == MPV_FORMAT_FLAG) {
                player.eof = *static_cast<const int *>(property->data) != 0;
                player.update_state();
            } else if (event->reply_userdata == kAudioTrackProperty && property->format == MPV_FORMAT_INT64)
                player.playback.has_audio = *static_cast<const std::int64_t *>(property->data) >= 0;
            else if (event->reply_userdata == kVideoWidthProperty && property->format == MPV_FORMAT_INT64)
                player.playback.width = std::max<std::int64_t>(0, *static_cast<const std::int64_t *>(property->data));
            else if (event->reply_userdata == kVideoHeightProperty && property->format == MPV_FORMAT_INT64)
                player.playback.height = std::max<std::int64_t>(0, *static_cast<const std::int64_t *>(property->data));
            else if ((event->reply_userdata == kContainerProperty || event->reply_userdata == kVideoCodecProperty) &&
                     property->format == MPV_FORMAT_STRING) {
                const auto *value = *static_cast<char *const *>(property->data);
                auto &target = event->reply_userdata == kContainerProperty ? player.playback.container
                                                                         : player.playback.video_codec;
                target = value ? value : "";
            }
        } else if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
            const auto *log = static_cast<const mpv_event_log_message *>(event->data);
            if (log && log->text && *log->text && player.playback.state == GalleryVideoPlaybackState::loading)
                player.playback.error = std::string(log->text).substr(0, 500);
        } else if ((event->event_id == MPV_EVENT_COMMAND_REPLY || event->event_id == MPV_EVENT_SET_PROPERTY_REPLY) &&
                   event->error < 0) {
            player.playback.state = GalleryVideoPlaybackState::error;
            player.playback.error = player.mpv_error_string_fn(event->error);
        }
    }
    return player.playback;
}

bool GalleryVideoPlayer::render(std::vector<std::uint8_t> &pixels, int width, int height)
{
    auto &player = *impl_;
    if (!player.context || width < 1 || height < 1 || width > 1920 || height > 1920 ||
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) > 2U * 1024U * 1024U ||
        (!player.dirty.exchange(false) && width == player.last_render_width && height == player.last_render_height))
        return false;
    const auto update = player.mpv_render_context_update_fn(player.context);
    const bool resize =
        player.last_render_width > 0 && (width != player.last_render_width || height != player.last_render_height);
    if (!(update & MPV_RENDER_UPDATE_FRAME) && !resize) return false;

    pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    int size[]                = {width, height};
    std::size_t stride        = static_cast<std::size_t>(width) * 4U;
    int block                 = 0;
    mpv_render_param params[] = {{MPV_RENDER_PARAM_SW_SIZE, size},
                                 {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("bgr0")},
                                 {MPV_RENDER_PARAM_SW_STRIDE, &stride},
                                 {MPV_RENDER_PARAM_SW_POINTER, pixels.data()},
                                 {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
                                 {MPV_RENDER_PARAM_INVALID, nullptr}};
    const int error           = player.mpv_render_context_render_fn(player.context, params);
    if (error < 0) {
        player.playback.state = GalleryVideoPlaybackState::error;
        player.playback.error = player.mpv_error_string_fn(error);
        return false;
    }
    player.last_render_width  = width;
    player.last_render_height = height;
    return true;
}

bool GalleryVideoPlayer::capture_thumbnail(const std::string &path, std::vector<std::uint8_t> &rgb565, int width,
                                           int height)
{
    rgb565.clear();
    if (path.empty() || width < 1 || height < 1) return false;
    impl_->thumbnail_mode = true;
    load(path);

    std::vector<std::uint8_t> bgr0;
    // Opening a local recording and producing the first software-rendered
    // frame normally takes only a few iterations. Keep the upper bound small
    // so a corrupt recording cannot stall the gallery indefinitely.
    constexpr int attempts     = 150;
    constexpr auto retry_delay = std::chrono::milliseconds(10);
    for (int attempt = 0; attempt < attempts; ++attempt) {
        const auto &state = poll();
        if (state.state == GalleryVideoPlaybackState::error || state.state == GalleryVideoPlaybackState::ended) break;
        if (render(bgr0, width, height)) {
            convert_bgr0_to_rgb565(bgr0, width, height, rgb565);
            stop();
            return !rgb565.empty();
        }
        std::this_thread::sleep_for(retry_delay);
    }
    stop();
    return false;
}

bool gallery_generate_video_thumbnail(const std::string &video_path, const std::string &thumbnail_path)
{
    if (video_path.empty() || thumbnail_path.empty()) return false;

    const auto expected_size = static_cast<std::size_t>(GalleryVideoPlayer::thumbnail_width) *
                               static_cast<std::size_t>(GalleryVideoPlayer::thumbnail_height) * 2U;
    if (auto *existing = std::fopen(thumbnail_path.c_str(), "rb")) {
        std::fseek(existing, 0, SEEK_END);
        const auto size = std::ftell(existing);
        std::fclose(existing);
        if (size >= 0 && static_cast<std::size_t>(size) == expected_size) return true;
    }

    GalleryVideoPlayer player(true);
    std::vector<std::uint8_t> pixels;
    if (!player.capture_thumbnail(video_path, pixels, GalleryVideoPlayer::thumbnail_width,
                                  GalleryVideoPlayer::thumbnail_height))
        return false;
    return write_rgb565_file(thumbnail_path, pixels);
}
