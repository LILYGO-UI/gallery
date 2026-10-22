#ifndef LILYGO_UI_GALLERY_SERVICES_VIDEO_PLAYER_HPP
#define LILYGO_UI_GALLERY_SERVICES_VIDEO_PLAYER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class GalleryVideoPlaybackState {
    idle,
    loading,
    playing,
    paused,
    ended,
    error,
};

struct GalleryVideoPlayback {
    GalleryVideoPlaybackState state = GalleryVideoPlaybackState::idle;
    double position                 = 0.0;
    double duration                 = 0.0;
    double volume                   = 100.0;
    bool buffering                  = false;
    bool has_audio                  = false;
    std::int64_t width               = 0;
    std::int64_t height              = 0;
    std::string container;
    std::string video_codec;
    std::string audio_codec;
    std::string error;
};

// Small software-rendered libmpv adapter for local gallery videos. The
// library is loaded at runtime so simulator builds do not need libmpv headers
// or a link-time media dependency. render() returns a BGR0 frame buffer.
class GalleryVideoPlayer {
public:
    static constexpr int thumbnail_width  = 360;
    static constexpr int thumbnail_height = 640;

    explicit GalleryVideoPlayer(bool silent = false);
    ~GalleryVideoPlayer();

    GalleryVideoPlayer(const GalleryVideoPlayer &)            = delete;
    GalleryVideoPlayer &operator=(const GalleryVideoPlayer &) = delete;

    void load(const std::string &path);
    void stop();
    void toggle_pause();
    void seek(double seconds);
    void set_volume(double percent);
    [[nodiscard]] double volume() const;
    const GalleryVideoPlayback &poll();

    // Render the most recent frame into width*height*4 BGR0 pixels. Returns
    // true when a new frame was produced.
    bool render(std::vector<std::uint8_t> &pixels, int width, int height);

    // Decode the first available video frame and return it as RGB565 pixels.
    // This is intended for gallery thumbnails and stops the player before
    // returning. It may block briefly while libmpv opens the media.
    bool capture_thumbnail(const std::string &path, std::vector<std::uint8_t> &rgb565, int width = thumbnail_width,
                           int height = thumbnail_height);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Generate a fixed-size RGB565 sidecar next to a video. The write is atomic;
// an existing complete sidecar is reused. The sidecar uses the same
// `<stem>.thumb.rgb565` convention as camera photo thumbnails.
[[nodiscard]] bool gallery_generate_video_thumbnail(const std::string &video_path, const std::string &thumbnail_path);

#endif
