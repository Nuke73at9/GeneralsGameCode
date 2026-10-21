# Contains base types, functions, macros and pragmas for all game build targets
add_subdirectory(Libraries)

# i stands for Interface
add_library(corei_gameengine_include INTERFACE)
add_library(corei_libraries_source_wwvegas INTERFACE)
add_library(corei_main INTERFACE)
add_library(corei_platform INTERFACE)
add_library(corei_always INTERFACE)
add_library(corei_always_no_pch INTERFACE) # Use this for Shared Libs with MFC AFX

# Create d3d8lib INTERFACE target early so Core/GameEngine can link to it
# This will be configured later by GeneralsMD/Code/CompatLib/CMakeLists.txt
# Only create if not already created by min-dx8-sdk CMakeLists
if(NOT TARGET d3d8lib)
    add_library(d3d8lib INTERFACE)
endif()

# Tiberium Project @feature Direct3D 9 renderer: <d3d8.h> resolves to the Direct3D 8 adapter over Direct3D 9
if(SAGE_USE_D3D9)
    if(SAGE_USE_DX8 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "SAGE_USE_D3D9 needs a 64-bit build (DXVK's Direct3D 9)")
    endif()
    target_include_directories(d3d8lib BEFORE INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Source/WWVegas/WW3D2/d3d9compat")
    target_compile_definitions(d3d8lib INTERFACE RTS_RENDERER_D3D9=1)
    message(STATUS "Renderer: Direct3D 9 (through the Direct3D 8 adapter)")
endif()

target_include_directories(corei_gameengine_include INTERFACE "GameEngine/Include")
target_include_directories(corei_libraries_source_wwvegas INTERFACE "Libraries/Source/WWVegas")
target_include_directories(corei_platform INTERFACE "Libraries/Source/Platform")
if(SAGE_USE_SDL3)
    target_link_libraries(corei_platform INTERFACE SDL3::SDL3)
endif()
target_include_directories(corei_main INTERFACE "Main")

target_link_libraries(corei_always INTERFACE
    core_libraries # Gives force included headers. Link early.
    core_config
    core_profile_tracy
    deps_utility
    resources
)
target_link_libraries(corei_always_no_pch INTERFACE
    core_libraries # Gives force included headers. Link early.
    core_config
    deps_utility
    resources
)

# Set where the build results will end up
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Base GameEngine library
add_subdirectory(GameEngine)

# Platform specific GameEngine code
add_subdirectory(GameEngineDevice)

add_subdirectory(Tools)
