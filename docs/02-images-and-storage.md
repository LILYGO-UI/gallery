# Images and Storage

[Back to README](../README.md)

## Source Directories

| Collection | Default directory | Environment override | Access in Gallery |
| --- | --- | --- | --- |
| Camera | `$HOME/Camera` or `CAMERA_OUTPUT_DIR` | `LILYGO_GALLERY_PHOTO_DIR` | Browse and delete photos and MP4 videos |
| Pictures | `$HOME/Pictures` | `LILYGO_GALLERY_PICTURES_DIR` | Browse and delete photos and MP4 videos |
| Screenshots | `$HOME/screenshots` | `LILYGO_GALLERY_SCREENSHOT_DIR` | Browse and delete |
| Samples | explicit `LILYGO_GALLERY_SAMPLE_DIR` or host fallback | `LILYGO_GALLERY_SAMPLE_DIR` | Read-only |

Directories are scanned when the application opens and when the refresh button
is pressed. Scanning is nonrecursive; unavailable directories are skipped.
Supported JPEG, PNG, PPM, and camera H.264 MP4 files from all sources are merged
and sorted by modification time, newest first, with at most 24 items retained
across the whole library. In-progress camera recordings ending in `.part.mp4`
are ignored. Albums filter this retained set, so their counts do not represent
every file on disk.
Dates use filesystem modification times, not EXIF capture dates.

For host development, launch with your own directories from the repository root:

```sh
LILYGO_GALLERY_PHOTO_DIR=/path/to/photos \
LILYGO_GALLERY_PICTURES_DIR=/path/to/pictures \
LILYGO_GALLERY_SCREENSHOT_DIR=/path/to/screenshots \
LILYGO_GALLERY_SAMPLE_DIR=/path/to/sample/gallery \
  lpm start --foreground
```

Nonempty environment values replace the corresponding defaults when the app
opens. The same overrides work on a device if explicitly supplied to its
process. Unset or empty values use the defaults. Refresh rescans the selected
directories; restart the app to apply changed environment settings.
When `LILYGO_GALLERY_PHOTO_DIR` is unset, Gallery follows the camera
application's `CAMERA_OUTPUT_DIR` before falling back to `$HOME/Camera`.

`LILYGO_GALLERY_SAMPLE_DIR` replaces the packaged sample directory and disables
the automatic host fallback described below. Files from this override remain
read-only in Gallery.

## Packaged Samples and Host Fallback

The repository includes PNG, JPEG, GIF, and PPM examples under
[`assets/gallery`](../assets/gallery). The Debian package keeps these files in
`/usr/share/lilygo-ui-gallery/gallery` as its import source and copies supported
images to the installing user's `$HOME/Pictures` without overwriting existing
files. The copied files are writable gallery items and can be deleted. The
application also performs the same one-time import at first launch when the
package was installed before the user's home directory was available.

An explicit `LILYGO_GALLERY_SAMPLE_DIR` still adds a read-only sample directory,
which is useful for development and rendering tests.

For a host simulator build, CMake also records the absolute path to the
repository's `assets/gallery/` directory. If the regular source scans find no
supported files and no nonempty `LILYGO_GALLERY_SAMPLE_DIR` is set, the app loads
that directory as a read-only fallback. This works even when the simulator is
launched from another working directory.

When real photos become available, refreshing replaces the fallback samples
with the regular scan results. Packaged samples, or samples selected by an
explicit override, are still part of the regular scan and may coexist with real
photos. The device build does not use the development checkout as a fallback.

To exercise an empty gallery on the host, explicitly point all three directory
overrides at empty directories. An empty string for the sample override does
not disable fallback.

## Image Formats and Thumbnails

| Format | Support |
| --- | --- |
| JPEG (`.jpg`, `.jpeg`) | Decoded with libjpeg-turbo, including progressive JPEGs |
| PNG (`.png`) | Decoded with LVGL's bundled LodePNG, including alpha compositing onto the page background |
| PPM (`.ppm`) | Binary P6 images with an 8-bit channel maximum of 255, decoded by Gallery |
| RGB565 (`.thumb.rgb565`) | `360x640` sidecar thumbnails for photos and videos, not standalone library entries |
| H.264 MP4 (`.mp4`) | First frame used for a generated thumbnail; played in the full-screen viewer through the runtime libmpv decoder |
| GIF (`.gif`) | Included in the asset package for other consumers; not scanned or decoded by Gallery |

File extensions are matched without case sensitivity. JPEG decoding is a required
build dependency and the Debian package declares `libjpeg62-turbo` as a runtime
dependency. The decoder accepts both baseline and progressive JPEG files and
keeps the existing input-size and source-pixel limits.

Camera photos and MP4 recordings can have a sibling thumbnail using the same
filename stem, such as `photo.jpg` and `photo.thumb.rgb565`. It must contain
exactly `360 x 640` little-endian RGB565 pixels, or 460,800 bytes, without a
header. When present, this file supplies thumbnails for the grid, album cover,
and filmstrip; otherwise photo thumbnails are generated from the original image
and writable video recordings are decoded from their first frame and cached in
this format. Screenshot thumbnails are always generated from their original
image. Incomplete `.part` files are never used as thumbnail sources.

The full-screen viewer decodes the original photo and uses contain-fit sizing
against the page background so the image is not cropped. MP4 videos open in the
same viewer, start automatically, and can be paused or resumed. The grid uses
square cover-fit thumbnails; if libmpv cannot decode a video, it falls back to a
play icon placeholder. Unreadable or unsupported image data produces a
placeholder; the viewer reports a playback error when libmpv cannot open a video.

## Browsing and Deletion

Library shows all retained photos and videos. Albums provides All Photos, Camera,
Screenshots, and Samples collections with cover images and counts. Opening a
collection filters both its grid and subsequent viewer navigation.

Tap a grid image to enter the viewer. Use the filmstrip, previous/next buttons,
or horizontal swipes to switch media within the collection. Swipe down to
return to the grid. Swipe up to reveal the filename, modification time, file
size, source, and location; the details button in the viewer controls opens the
same panel directly. The details body scrolls when needed; closing the panel
restores the viewer controls and filmstrip. On displays shorter than
400 pixels, the filmstrip is hidden to leave room for the photo. The viewer's
plus and minus controls zoom the photo in 25% steps up to the memory-safe
maximum for the current viewport. While zoomed, drag the photo to inspect
other regions; horizontal and vertical viewer swipes are reserved for panning.

Deleting a writable photo or video opens a bottom action sheet with Delete Item and
Cancel actions. Confirmation permanently removes the original file; it does
not move the file to a trash folder. The app then attempts to remove any separate
thumbnail associated with that photo and refreshes the library. A media deletion
failure is shown in the confirmation sheet. Samples have deletion disabled.

For layout and interaction acceptance requirements, see the
[UI development guidelines](01-user-interface.md). For rendering the packaged
PPM examples, see [tests and rendering](03-development.md#tests-and-rendering).
