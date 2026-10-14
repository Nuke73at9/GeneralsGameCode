# Abram's Personal Test @build Support for the 64-bit Windows build (preset abram-win64).
#
# - FFmpeg: the engine links PkgConfig::FFMPEG. On Windows there is usually no pkg-config, so build the
#   same target from vcpkg's FFmpeg package instead.
# - DXVK: 64-bit Windows has no d3d8.dll of its own. Download DXVK's Windows release, which translates
#   DirectX 8 to Vulkan, and install its 64-bit d3d8.dll and d3d9.dll into a "dxvk" folder next to the
#   game. They are kept in a subfolder so the original 32-bit game never loads them by mistake.

if(NOT (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8))
    return()
endif()

if(RTS_BUILD_OPTION_FFMPEG AND NOT TARGET PkgConfig::FFMPEG)
    # Look the files up directly in vcpkg's folders (the vcpkg toolchain adds them to the search path).
    # find_package(FFMPEG) is not used because cmake/FindFFmpeg.cmake would be picked up instead of
    # vcpkg's own module on Windows, where file names are not case-sensitive.
    find_path(ABRAM_FFMPEG_INCLUDE_DIR libavcodec/avcodec.h REQUIRED)
    set(ABRAM_FFMPEG_LIBRARIES "")
    foreach(_abram_ffmpeg_lib avformat avcodec swscale swresample avutil)
        find_library(ABRAM_FFMPEG_${_abram_ffmpeg_lib}_LIBRARY NAMES ${_abram_ffmpeg_lib} REQUIRED)
        list(APPEND ABRAM_FFMPEG_LIBRARIES "${ABRAM_FFMPEG_${_abram_ffmpeg_lib}_LIBRARY}")
    endforeach()
    add_library(PkgConfig::FFMPEG INTERFACE IMPORTED GLOBAL)
    target_include_directories(PkgConfig::FFMPEG INTERFACE "${ABRAM_FFMPEG_INCLUDE_DIR}")
    target_link_libraries(PkgConfig::FFMPEG INTERFACE ${ABRAM_FFMPEG_LIBRARIES})
    set(FFMPEG_INCLUDE_DIRS "${ABRAM_FFMPEG_INCLUDE_DIR}" CACHE INTERNAL "")
    message(STATUS "FFmpeg from vcpkg: ${ABRAM_FFMPEG_INCLUDE_DIR}")
endif()

set(ABRAM_DXVK_WINDOWS_VERSION "2.6")
FetchContent_Declare(
    dxvk_windows
    URL "https://github.com/doitsujin/dxvk/releases/download/v${ABRAM_DXVK_WINDOWS_VERSION}/dxvk-${ABRAM_DXVK_WINDOWS_VERSION}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(dxvk_windows)
set(ABRAM_DXVK_DLLS
    "${dxvk_windows_SOURCE_DIR}/x64/d3d8.dll"
    "${dxvk_windows_SOURCE_DIR}/x64/d3d9.dll"
)
message(STATUS "DXVK ${ABRAM_DXVK_WINDOWS_VERSION} for 64-bit Windows: ${dxvk_windows_SOURCE_DIR}/x64")
