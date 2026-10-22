#include "services/video_player.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace {

template <typename Predicate>
bool wait_for(GalleryVideoPlayer &player, Predicate predicate, int timeout_ms = 2500)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto &state = player.poll();
        if (predicate(state)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate(player.poll());
}

}  // namespace

int main(int argc, char **argv)
{
    assert(argc == 2);

    GalleryVideoPlayer player(false);
    player.load(argv[1]);
    assert(wait_for(player, [](const auto &state) {
        return state.state == GalleryVideoPlaybackState::playing || state.state == GalleryVideoPlaybackState::error;
    }));
    assert(wait_for(player, [](const auto &state) {
        return state.state == GalleryVideoPlaybackState::playing && state.duration >= 2.5 && state.has_audio;
    }));

    player.set_volume(34.0);
    assert(wait_for(player, [](const auto &state) { return std::abs(state.volume - 34.0) < 0.5; }));

    std::vector<std::uint8_t> frame;
    assert(wait_for(player, [&](const auto &) { return player.render(frame, 320, 180); }));
    assert(frame.size() == 320U * 180U * 4U);
    assert(wait_for(player, [&](const auto &) { return player.render(frame, 568, 1232); }));
    assert(frame.size() == 568U * 1232U * 4U);

    player.toggle_pause();
    assert(wait_for(player, [](const auto &state) { return state.state == GalleryVideoPlaybackState::paused; }));
    player.seek(1.5);
    assert(wait_for(player, [](const auto &state) {
        return state.state == GalleryVideoPlaybackState::paused && std::abs(state.position - 1.5) < 0.3;
    }));
    player.toggle_pause();
    assert(wait_for(player, [](const auto &state) {
        return state.state == GalleryVideoPlaybackState::playing || state.state == GalleryVideoPlaybackState::ended;
    }));
    assert(wait_for(player, [](const auto &state) { return state.state == GalleryVideoPlaybackState::ended; }));

    player.seek(0.5);
    assert(wait_for(player, [](const auto &state) {
        return state.state == GalleryVideoPlaybackState::playing && std::abs(state.position - 0.5) < 0.3;
    }));
    player.stop();
}
