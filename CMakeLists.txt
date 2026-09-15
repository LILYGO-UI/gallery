cmake_minimum_required(VERSION 3.21)

include(cmake/LpmMetadata.cmake)
project(${LILYGO_UI_CMAKE_PROJECT_NAME} LANGUAGES C CXX)

include(GNUInstallDirs)
include(CTest)
find_package(LilyGoUI CONFIG REQUIRED)
find_package(JPEG QUIET)

set(GALLERY_USE_LVGL_TJPGD OFF)
if(NOT JPEG_FOUND AND TARGET lvgl)
    get_target_property(_gallery_lvgl_is_imported lvgl IMPORTED)
    if(NOT _gallery_lvgl_is_imported)
        target_compile_definitions(lvgl PUBLIC LV_USE_TJPGD=1)
        set(GALLERY_USE_LVGL_TJPGD ON)
    endif()
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
    src/pages/gallery/gallery_model.cpp)
target_include_directories(lilygo_ui_gallery_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_options(lilygo_ui_gallery_core PRIVATE
    -Wall -Wextra -Wpedantic)
target_compile_features(lilygo_ui_gallery_core PUBLIC cxx_std_17)

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
if(JPEG_FOUND)
    target_compile_definitions(lilygo_ui_gallery_ui PRIVATE GALLERY_HAS_JPEG=1)
    target_link_libraries(lilygo_ui_gallery_ui PRIVATE JPEG::JPEG)
    message(STATUS "Gallery: JPEG photo decoding enabled")
elseif(GALLERY_USE_LVGL_TJPGD)
    target_compile_definitions(lilygo_ui_gallery_ui PRIVATE GALLERY_HAS_LVGL_TJPGD=1)
    message(STATUS "Gallery: using LVGL's bounded baseline JPEG decoder")
else()
    message(WARNING "Gallery: libjpeg was not found; JPEG photos will use placeholders")
endif()

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
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/gallery/sunrise.ppm)

    add_executable(gallery-render-ui tests/render_ui.cpp)
    target_compile_options(gallery-render-ui PRIVATE
        -Wall -Wextra -Wpedantic -UNDEBUG)
    target_link_libraries(gallery-render-ui PRIVATE
        lilygo_ui_gallery_ui)
    foreach(viewport IN ITEMS portrait landscape)
        if(viewport STREQUAL "portrait")
            set(_width 568)
            set(_height 1232)
        else()
            set(_width 1232)
            set(_height 568)
        endif()
        add_test(NAME gallery-render-${viewport} COMMAND gallery-render-ui
            ${_width} ${_height}
            ${CMAKE_CURRENT_BINARY_DIR}/gallery-${viewport}.ppm
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/gallery/forest.jpg)
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
    "libc6, libstdc++6, libdrm2, libfreetype6, lilygo-ui-appkit-dev (>= ${LILYGO_UI_MIN_APPKIT_VERSION})")
if(JPEG_FOUND)
    string(APPEND CPACK_DEBIAN_APP_PACKAGE_DEPENDS ", libjpeg62-turbo")
endif()
set(CPACK_DEBIAN_APP_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE
        "${LILYGO_UI_PACKAGE_ARCHITECTURE}")
endif()
include(CPack)
