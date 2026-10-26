# Base headers shared by all libraries
add_subdirectory(Include)

# WW common libraries decended from earliest C&C games
add_subdirectory(Source/WWVegas)

# profiling library
add_subdirectory(Source/profile)

# debugging library
add_subdirectory(Source/debug)

add_subdirectory(Source/EABrowserDispatch)
add_subdirectory(Source/EABrowserEngine)
add_subdirectory(Source/Compression)

# Tiberium Project @feature The render-device interface and its Vulkan device
if(SAGE_USE_VULKAN_RHI)
    add_subdirectory(Source/RHI)
endif()
