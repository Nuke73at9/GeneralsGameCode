set(DEBUG_SRC
    "debug_cmd.cpp"
    "debug_cmd.h"
    "debug_debug.cpp"
    "debug_debug.h"
    "debug_doc.h"
    "debug_except.cpp"
    "debug_getdefaultcommands.cpp"
    "debug_internal.cpp"
    "debug_io_con.cpp"
    "debug_io_flat.cpp"
    "debug_io_net.cpp"
    "debug_io_ods.cpp"
    "debug_io.h"
    "debug_macro.h"
    "debug_stack.cpp"
    "debug_stack.h"
    "debug.h"
    "internal_except.h"
    "internal_io.h"
    "internal.h"
)

# Debug library is Windows-only (depends on windows.h)
# Abram's Personal Test @build It reads 32-bit x86 registers (Eip, Esp, x87 state) and uses x86 inline
# assembly, and only the tools link it, so it is built for 32-bit Windows only.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_library(core_debug STATIC)

    target_sources(core_debug PRIVATE ${DEBUG_SRC})

    target_include_directories(core_debug INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_precompile_headers(core_debug PRIVATE
        [["Precompiled/BasePrecomp.h"]] # Must be first
        "debug.h"
        "internal.h"
        "internal_except.h"
        "internal_io.h"
        <windows.h>
    )

    target_link_libraries(core_debug PRIVATE
        core_wwcommon
        corei_always
    )
endif()
