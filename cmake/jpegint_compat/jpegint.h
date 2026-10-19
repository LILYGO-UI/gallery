/*
 * Compatibility declaration used by LVGL's libjpeg-turbo decoder.
 *
 * Debian's libjpeg-turbo development package does not always install the
 * private jpegint.h header, while LVGL uses the marker-reader callback to
 * inspect EXIF orientation. The marker-reader prefix is stable across the
 * supported libjpeg-turbo ABI versions, so keep the small declaration LVGL
 * needs here and use the public jpeglib.h types for the rest.
 */
#ifndef LILYGO_UI_GALLERY_JPEGINT_COMPAT_H
#define LILYGO_UI_GALLERY_JPEGINT_COMPAT_H

#include <jpeglib.h>

struct jpeg_marker_reader {
    boolean (*reset_marker_reader)(j_decompress_ptr cinfo);
    boolean (*read_markers)(j_decompress_ptr cinfo);
    jpeg_marker_parser_method process_COM;
    jpeg_marker_parser_method process_APPn[16];
    unsigned int length_limit_COM;
    unsigned int length_limit_APPn[16];
};

#endif
