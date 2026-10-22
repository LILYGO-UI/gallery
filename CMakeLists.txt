cmake_minimum_required(VERSION 3.21)

include(cmake/LpmMetadata.cmake)
project(${LILYGO_UI_CMAKE_PROJECT_NAME} LANGUAGES C CXX)

include(GNUInstallDirs)
include(CTest)
find_package(LilyGoUI CONFIG REQUIRED)

# Gallery JPEGs use LVGL's libjpeg-turbo decoder. The target runtime is
# libjpeg-turbo's ABI-compatible libjpeg.so.62. Some CM0 BSPs contain only the
# runtime library, so LIBJPEG_TURBO_DEV_ROOT can point at an extracted matching
# libjpeg-turbo development package with its headers and linker symlink.
if(CMAKE_CROSSCOMPILING)
    set(LIBJPEG_TURBO_DEV_ROOT "" CACHE PATH
        "Root of an extracted target libjpeg-turbo development package")
    # Keep the old cache names readable for checkouts configured before this
    # variable was introduced.
    set(GALLERY_JPEG_INCLUDE_DIR "${GALLERY_JPEG_INCLUDE_DIR}" CACHE PATH
        "Directory containing target libjpeg-turbo headers")
    set(GALLERY_JPEG_LIBRARY "${GALLERY_JPEG_LIBRARY}" CACHE FILEPATH
        "Target libjpeg-turbo library (usually libjpeg.so.62)")

    function(_gallery_fetch_libjpeg_turbo_dev output)
        set(_version "2.1.5-4")
        set(_package "libjpeg62-turbo-dev_${_version}_arm64.deb")
        set(_cache "${CM0_BSP_CACHE_DIR}/libjpeg-turbo/${_version}")
        set(_archive "${_cache}/${_package}")
        set(_root "${_cache}/arm64")
        file(MAKE_DIRECTORY "${_cache}")
        file(LOCK "${_cache}" DIRECTORY GUARD FUNCTION TIMEOUT 180
            RESULT_VARIABLE _lock_result)
        if(NOT _lock_result STREQUAL "0")
            message(FATAL_ERROR "Could not lock the libjpeg-turbo cache: ${_lock_result}")
        endif()

        if(NOT EXISTS "${_root}/usr/include/jpeglib.h")
            message(STATUS "Downloading libjpeg-turbo development headers ${_version} (arm64)")
            file(DOWNLOAD
                "https://deb.debian.org/debian/pool/main/libj/libjpeg-turbo/${_package}"
                "${_archive}"
                TLS_VERIFY ON
                TIMEOUT 120
                INACTIVITY_TIMEOUT 30
                STATUS _download_status)
            list(GET _download_status 0 _download_code)
            list(GET _download_status 1 _download_message)
            if(NOT _download_code EQUAL 0)
                file(REMOVE "${_archive}")
                message(FATAL_ERROR
                    "Failed to download ${_package}: ${_download_message}\n"
                    "For offline builds, provide -DLIBJPEG_TURBO_DEV_ROOT=/path/to/libjpeg62-turbo-dev.")
            endif()

            set(_staging "${_cache}/extracting")
            file(REMOVE_RECURSE "${_staging}")
            file(MAKE_DIRECTORY "${_staging}/package" "${_staging}/root")
            file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_staging}/package")
            file(GLOB _data_archives "${_staging}/package/data.tar.*")
            list(LENGTH _data_archives _data_count)
            if(NOT _data_count EQUAL 1)
                message(FATAL_ERROR "Missing data archive in ${_package}")
            endif()
            list(GET _data_archives 0 _data_archive)
            file(ARCHIVE_EXTRACT INPUT "${_data_archive}" DESTINATION "${_staging}/root")
            if(NOT EXISTS "${_staging}/root/usr/include/jpeglib.h")
                message(FATAL_ERROR "Missing libjpeg-turbo headers in ${_package}")
            endif()
            file(REMOVE_RECURSE "${_root}")
            file(RENAME "${_staging}/root" "${_root}")
            file(REMOVE_RECURSE "${_staging}")
        endif()
        set(${output} "${_root}" PARENT_SCOPE)
    endfunction()

    if(NOT LIBJPEG_TURBO_DEV_ROOT AND NOT EXISTS "${CMAKE_SYSROOT}/usr/include/jpeglib.h" AND
       CM0_BSP_VERSION STREQUAL "0.1.0" AND CM0_BSP_CACHE_DIR)
        _gallery_fetch_libjpeg_turbo_dev(_gallery_fetched_jpeg_root)
        set(LIBJPEG_TURBO_DEV_ROOT "${_gallery_fetched_jpeg_root}" CACHE PATH
            "Root of an extracted target libjpeg-turbo development package" FORCE)
    endif()

    set(_gallery_jpeg_header_paths
        "${CMAKE_SYSROOT}/usr/include"
        "${CMAKE_SYSROOT}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}")
    set(_gallery_jpeg_library_paths
        "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
        "${CMAKE_SYSROOT}/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
        "${CMAKE_SYSROOT}/usr/lib"
        "${CMAKE_SYSROOT}/lib")
    if(LIBJPEG_TURBO_DEV_ROOT)
        list(PREPEND _gallery_jpeg_header_paths
            "${LIBJPEG_TURBO_DEV_ROOT}/include"
            "${LIBJPEG_TURBO_DEV_ROOT}/usr/include"
            "${LIBJPEG_TURBO_DEV_ROOT}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}")
        list(PREPEND _gallery_jpeg_library_paths
            "${LIBJPEG_TURBO_DEV_ROOT}/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
            "${LIBJPEG_TURBO_DEV_ROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
            "${LIBJPEG_TURBO_DEV_ROOT}/lib"
            "${LIBJPEG_TURBO_DEV_ROOT}/usr/lib")
    endif()

    if(GALLERY_JPEG_INCLUDE_DIR)
        set(_gallery_jpeg_include_dir "${GALLERY_JPEG_INCLUDE_DIR}")
    elseif(LIBJPEG_TURBO_DEV_ROOT AND EXISTS "${LIBJPEG_TURBO_DEV_ROOT}/usr/include/jpeglib.h")
        set(_gallery_jpeg_include_dir "${LIBJPEG_TURBO_DEV_ROOT}/usr/include")
    else()
        find_path(_gallery_jpeg_include_dir NAMES jpeglib.h
            PATHS ${_gallery_jpeg_header_paths} NO_DEFAULT_PATH)
    endif()
    if(GALLERY_JPEG_LIBRARY)
        set(_gallery_jpeg_library "${GALLERY_JPEG_LIBRARY}")
    elseif(EXISTS "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/libjpeg.so.62")
        set(_gallery_jpeg_library
            "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/libjpeg.so.62")
    elseif(LIBJPEG_TURBO_DEV_ROOT AND
           EXISTS "${LIBJPEG_TURBO_DEV_ROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/libjpeg.so")
        set(_gallery_jpeg_library
            "${LIBJPEG_TURBO_DEV_ROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/libjpeg.so")
    else()
        find_library(_gallery_jpeg_library
            NAMES jpeg libjpeg.so.62
            PATHS ${_gallery_jpeg_library_paths} NO_DEFAULT_PATH)
    endif()

    if(_gallery_jpeg_include_dir AND _gallery_jpeg_library)
        set(_gallery_jpeg_interface_includes "${_gallery_jpeg_include_dir}")
        if(LIBJPEG_TURBO_DEV_ROOT AND
           IS_DIRECTORY "${LIBJPEG_TURBO_DEV_ROOT}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}")
            list(APPEND _gallery_jpeg_interface_includes
                "${LIBJPEG_TURBO_DEV_ROOT}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}")
        endif()
        add_library(JPEG::JPEG UNKNOWN IMPORTED)
        set_target_properties(JPEG::JPEG PROPERTIES
            IMPORTED_LOCATION "${_gallery_jpeg_library}"
            INTERFACE_INCLUDE_DIRECTORIES "${_gallery_jpeg_interface_includes}")
        set(JPEG_FOUND TRUE)
        set(JPEG_INCLUDE_DIRS "${_gallery_jpeg_include_dir}")
        set(JPEG_LIBRARIES JPEG::JPEG)
    endif()
else()
    find_package(JPEG QUIET)
endif()

if(NOT JPEG_FOUND OR NOT TARGET JPEG::JPEG)
    message(FATAL_ERROR
        "Gallery requires libjpeg-turbo development headers and a target "
        "libjpeg-turbo library. Install libjpeg-turbo-dev, or set "
        "LIBJPEG_TURBO_DEV_ROOT to an extracted matching target dev package "
        "for a CM0 build.")
endif()

# Do not silently accept a classic IJG libjpeg installation. libjpeg-turbo's
# jconfig.h identifies the implementation and is also used to check that the
# target headers match the selected runtime.
get_target_property(_gallery_jpeg_include_dirs JPEG::JPEG INTERFACE_INCLUDE_DIRECTORIES)
set(_gallery_jpeg_config_header "")
foreach(_gallery_jpeg_include_dir IN LISTS _gallery_jpeg_include_dirs)
    if(EXISTS "${_gallery_jpeg_include_dir}/jconfig.h")
        set(_gallery_jpeg_config_header "${_gallery_jpeg_include_dir}/jconfig.h")
        break()
    endif()
endforeach()
if(_gallery_jpeg_config_header)
    file(STRINGS "${_gallery_jpeg_config_header}" _gallery_turbo_version
        REGEX "^#[ \\t]*define[ \\t]+LIBJPEG_TURBO_VERSION[ \\t]+")
endif()
if(NOT _gallery_turbo_version)
    message(FATAL_ERROR
        "Gallery requires libjpeg-turbo headers (jconfig.h must define "
        "LIBJPEG_TURBO_VERSION)")
endif()

set(GALLERY_DEVELOPMENT_SAMPLE_DIR "")
if(CM0_SIMULATOR AND NOT CMAKE_CROSSCOMPILING)
    set(GALLERY_DEVELOPMENT_SAMPLE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/gallery")
endif()

configure_file(
    cmake/app_identity.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/app_identity.h
    @ONLY)

add_library(lilygo_ui_gallery_core STATIC
    src/pages/gallery/gallery_model.cpp
    src/services/video_player.cpp)
target_include_directories(lilygo_ui_gallery_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/include)
target_compile_options(lilygo_ui_gallery_core PRIVATE
    -Wall -Wextra -Wpedantic)
target_compile_features(lilygo_ui_gallery_core PUBLIC cxx_std_17)
target_link_libraries(lilygo_ui_gallery_core PUBLIC ${CMAKE_DL_LIBS})

add_library(lilygo_ui_gallery_ui STATIC
    src/app.cpp
    src/components/components.cpp
    src/pages/gallery/gallery_image.cpp
    src/pages/gallery/gallery_view.cpp
    src/pages/gallery/gallery_view_model.cpp)
target_include_directories(lilygo_ui_gallery_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_options(lilygo_ui_gallery_ui PRIVATE
    -Wall -Wextra -Wpedantic)
target_link_libraries(lilygo_ui_gallery_ui PUBLIC
    lilygo_ui_gallery_core
    LilyGoUI::Runtime)
target_compile_definitions(lvgl PUBLIC
    LV_USE_LIBJPEG_TURBO=1
    LV_USE_TJPGD=0)
target_include_directories(lvgl SYSTEM BEFORE PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/jpegint_compat")
target_link_libraries(lvgl PUBLIC JPEG::JPEG)
target_compile_definitions(lilygo_ui_gallery_ui PRIVATE GALLERY_HAS_LVGL_LIBJPEG_TURBO=1)
message(STATUS "Gallery: using libjpeg-turbo for JPEG photo decoding")

add_executable(lilygo_ui_gallery src/main.cpp)
target_compile_options(lilygo_ui_gallery PRIVATE
    -Wall -Wextra -Wpedantic)
target_link_libraries(lilygo_ui_gallery PRIVATE lilygo_ui_gallery_ui)
set_target_properties(lilygo_ui_gallery PROPERTIES
    OUTPUT_NAME "${LILYGO_UI_EXECUTABLE_NAME}")

configure_file(data/launcher.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.json @ONLY)
configure_file(data/application.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.desktop @ONLY)
configure_file(data/application.metainfo.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.metainfo.xml @ONLY)
configure_file(cmake/gallery-postinst.in
    ${CMAKE_CURRENT_BINARY_DIR}/postinst @ONLY)

install(TARGETS lilygo_ui_gallery
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT app)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.json
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/launcher/apps
    COMPONENT app)
lilygo_ui_install_launcher_icon(
    "${LILYGO_UI_APP_ID}"
    "${CMAKE_CURRENT_SOURCE_DIR}/${LILYGO_UI_APP_ICON}")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    COMPONENT app)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LILYGO_UI_APP_ID}.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
    COMPONENT app)
install(FILES
    third_party/mpv/include/mpv/client.h
    third_party/mpv/include/mpv/render.h
    third_party/mpv/README.md
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/${LILYGO_UI_PACKAGE_NAME}/licenses/mpv
    COMPONENT app)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/gallery/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${LILYGO_UI_PACKAGE_NAME}/gallery
    COMPONENT app
    FILES_MATCHING
    PATTERN "*.gif"
    PATTERN "*.jpg"
    PATTERN "*.jpeg"
    PATTERN "*.png"
    PATTERN "*.ppm")
lilygo_ui_install_font_licenses(
    "${CMAKE_INSTALL_DATAROOTDIR}/doc/${LILYGO_UI_PACKAGE_NAME}"
    app)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    find_program(GALLERY_FFMPEG_EXECUTABLE ffmpeg)
    find_library(GALLERY_MPV_RUNTIME NAMES mpv mpv.2
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)
    if(GALLERY_FFMPEG_EXECUTABLE AND GALLERY_MPV_RUNTIME)
        execute_process(
            COMMAND "${GALLERY_FFMPEG_EXECUTABLE}" -hide_banner -encoders
            OUTPUT_VARIABLE _gallery_ffmpeg_encoders
            ERROR_VARIABLE _gallery_ffmpeg_encoders_error
            RESULT_VARIABLE _gallery_ffmpeg_encoders_result)
        string(APPEND _gallery_ffmpeg_encoders "${_gallery_ffmpeg_encoders_error}")
    endif()
    if(GALLERY_FFMPEG_EXECUTABLE AND GALLERY_MPV_RUNTIME AND
       _gallery_ffmpeg_encoders_result EQUAL 0 AND
       _gallery_ffmpeg_encoders MATCHES "libx264" AND
       _gallery_ffmpeg_encoders MATCHES "aac")
        set(_gallery_video_test_fixture "${CMAKE_CURRENT_BINARY_DIR}/gallery-video-test.mp4")
        add_custom_command(
            OUTPUT "${_gallery_video_test_fixture}"
            COMMAND "${GALLERY_FFMPEG_EXECUTABLE}" -hide_banner -loglevel error -y
                -f lavfi -i testsrc=size=160x90:rate=15
                -f lavfi -i sine=frequency=440:sample_rate=44100
                -t 3 -c:v libx264 -pix_fmt yuv420p -c:a aac -movflags +faststart
                "${_gallery_video_test_fixture}"
            VERBATIM)
        add_custom_target(gallery-video-test-fixture
            DEPENDS "${_gallery_video_test_fixture}")
        add_executable(gallery-video-player-tests tests/test_video_player.cpp)
        target_compile_options(gallery-video-player-tests PRIVATE
            -Wall -Wextra -Wpedantic -UNDEBUG)
        target_link_libraries(gallery-video-player-tests PRIVATE
            lilygo_ui_gallery_core)
        add_dependencies(gallery-video-player-tests gallery-video-test-fixture)
        add_test(NAME gallery-video-player COMMAND gallery-video-player-tests
            "${_gallery_video_test_fixture}")
    else()
        message(STATUS
            "Gallery: skipping libmpv integration test (requires libmpv and FFmpeg libx264/AAC encoders)")
    endif()

    add_executable(gallery-core-tests tests/test_core.cpp)
    target_compile_options(gallery-core-tests PRIVATE
        -Wall -Wextra -Wpedantic -UNDEBUG)
    target_link_libraries(gallery-core-tests PRIVATE
        lilygo_ui_gallery_core)
    add_test(NAME gallery-core COMMAND gallery-core-tests)

    add_executable(gallery-view-model-tests tests/test_view_model.cpp)
    target_compile_options(gallery-view-model-tests PRIVATE
        -Wall -Wextra -Wpedantic -UNDEBUG)
    target_link_libraries(gallery-view-model-tests PRIVATE
        lilygo_ui_gallery_ui)
    add_test(NAME gallery-view-model COMMAND gallery-view-model-tests)

    add_executable(gallery-image-tests tests/test_image.cpp)
    target_compile_options(gallery-image-tests PRIVATE
        -Wall -Wextra -Wpedantic -UNDEBUG)
    target_link_libraries(gallery-image-tests PRIVATE
        lilygo_ui_gallery_ui)
    add_test(NAME gallery-image COMMAND gallery-image-tests
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/app-icon.png
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/gallery/forest.jpg
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/gallery/sunrise.png
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/gallery/sunrise.ppm
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/sunrise-progressive.jpg)

    add_executable(gallery-render-ui tests/render_ui.cpp)
    target_compile_options(gallery-render-ui PRIVATE
        -Wall -Wextra -Wpedantic -UNDEBUG)
    target_link_libraries(gallery-render-ui PRIVATE
        lilygo_ui_gallery_ui)
    if(TARGET gallery-video-test-fixture)
        add_dependencies(gallery-render-ui gallery-video-test-fixture)
    endif()
    foreach(viewport IN ITEMS portrait landscape)
        if(viewport STREQUAL "portrait")
            set(_width 568)
            set(_height 1232)
        else()
            set(_width 1232)
            set(_height 568)
        endif()
        set(_gallery_render_args
            ${_width} ${_height}
            ${CMAKE_CURRENT_BINARY_DIR}/gallery-${viewport}.ppm
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/gallery/forest.jpg)
        if(TARGET gallery-video-test-fixture)
            list(APPEND _gallery_render_args "${_gallery_video_test_fixture}")
        endif()
        add_test(NAME gallery-render-${viewport} COMMAND gallery-render-ui
            ${_gallery_render_args})
    endforeach()
endif()

# New applications have no legacy CM0 package to replace. Add Debian migration
# fields only when this project supersedes an actually published package.
set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME "${LILYGO_UI_PACKAGE_NAME}")
set(CPACK_PACKAGE_VERSION "${LILYGO_UI_VERSION}")
if(LILYGO_UI_VERSION MATCHES
   "^([0-9]+:)?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(CPACK_PACKAGE_VERSION_MAJOR "${CMAKE_MATCH_2}")
    set(CPACK_PACKAGE_VERSION_MINOR "${CMAKE_MATCH_3}")
    set(CPACK_PACKAGE_VERSION_PATCH "${CMAKE_MATCH_4}")
else()
    set(CPACK_PACKAGE_VERSION_MAJOR 0)
    set(CPACK_PACKAGE_VERSION_MINOR 0)
    set(CPACK_PACKAGE_VERSION_PATCH 0)
endif()
set(CPACK_PACKAGE_CONTACT "${LILYGO_UI_PACKAGE_CONTACT}")
set(CPACK_PACKAGE_VENDOR "${LILYGO_UI_PACKAGE_CONTACT}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${LILYGO_UI_APP_SUMMARY}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${LILYGO_UI_APP_HOMEPAGE}")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_SET_DESTDIR ON)
set(CPACK_COMPONENTS_ALL app)
set(CPACK_DEBIAN_APP_PACKAGE_NAME "${LILYGO_UI_PACKAGE_NAME}")
set(CPACK_DEBIAN_APP_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_APP_PACKAGE_DEPENDS
    "libc6, libstdc++6, libdrm2, libfreetype6, libmpv2 (>= 0.38), lilygo-ui-appkit-dev (>= ${LILYGO_UI_MIN_APPKIT_VERSION})")
string(APPEND CPACK_DEBIAN_APP_PACKAGE_DEPENDS ", libjpeg62-turbo")
set(CPACK_DEBIAN_APP_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_APP_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_BINARY_DIR}/postinst")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE
        "${LILYGO_UI_PACKAGE_ARCHITECTURE}")
endif()
include(CPack)
