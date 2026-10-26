# Tiberium Project @feature The render-device interface (RHI) and its Vulkan 1.3 device.
# The device loads the Vulkan loader at run time (vulkan-1.dll, libvulkan.so.1, libvulkan.1.dylib),
# so only the Vulkan headers are needed to build: the system's (the Vulkan SDK, a distribution's
# package), else Khronos' Vulkan-Headers fetched from GitHub.

set(RHI_VULKAN_HEADERS_TAG "v1.3.275" CACHE STRING "Vulkan-Headers tag fetched when the system has none")

find_path(RHI_VULKAN_INCLUDE_DIR vulkan/vulkan.h HINTS "$ENV{VULKAN_SDK}/Include" "$ENV{VULKAN_SDK}/include")
if(NOT RHI_VULKAN_INCLUDE_DIR)
    include(FetchContent)
    FetchContent_Declare(
        vulkan_headers
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG ${RHI_VULKAN_HEADERS_TAG}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(vulkan_headers)
    set(RHI_VULKAN_INCLUDE_DIR "${vulkan_headers_SOURCE_DIR}/include" CACHE PATH "Vulkan headers" FORCE)
endif()
message(STATUS "RHI: Vulkan headers in ${RHI_VULKAN_INCLUDE_DIR}")

add_library(core_rhi STATIC
    include/rhi/rhi.h
    src/rhi_vulkan.cpp
    src/vk_loader.cpp
    src/vk_loader.h
)
target_include_directories(core_rhi PUBLIC include)
target_include_directories(core_rhi PRIVATE "${RHI_VULKAN_INCLUDE_DIR}")
target_compile_features(core_rhi PUBLIC cxx_std_20)
if(WIN32)
    # <windows.h> for LoadLibrary, without min/max and the rest
    target_compile_definitions(core_rhi PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
target_link_libraries(core_rhi PRIVATE ${CMAKE_DL_LIBS})

# The headless self-test: raster, compute, ray queries and frames in flight on any Vulkan 1.3
# device, lavapipe included. Exit code 77: no device (skipped). The shaders are precompiled SPIR-V
# (tests/spirv, rebuilt by scripts/tooling/rhi/build_spirv.py from tests/shaders).
add_executable(rhi_selftest tests/rhi_selftest.cpp)
target_include_directories(rhi_selftest PRIVATE tests)
target_link_libraries(rhi_selftest PRIVATE core_rhi)
if(BUILD_TESTING)
    add_test(NAME rhi_selftest COMMAND rhi_selftest)
    set_tests_properties(rhi_selftest PROPERTIES SKIP_RETURN_CODE 77)
endif()
