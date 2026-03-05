# libe3 Compiler Settings
#
# SPDX-License-Identifier: Apache-2.0

# ============================================================================
# Compiler Warnings
# ============================================================================
add_library(libe3_warnings INTERFACE)
target_compile_options(libe3_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wold-style-cast
        -Wcast-qual
        -Wformat=2
        -Werror=return-type
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /WX
        /permissive-
    >
)

# ============================================================================
# Sanitizers
# ============================================================================
add_library(libe3_sanitizers INTERFACE)

if(LIBE3_ENABLE_ASAN)
    target_compile_options(libe3_sanitizers INTERFACE
        -fsanitize=address
        -fno-omit-frame-pointer
    )
    target_link_options(libe3_sanitizers INTERFACE
        -fsanitize=address
    )
endif()

if(LIBE3_ENABLE_TSAN)
    target_compile_options(libe3_sanitizers INTERFACE
        -fsanitize=thread
        -fno-omit-frame-pointer
    )
    target_link_options(libe3_sanitizers INTERFACE
        -fsanitize=thread
    )
endif()
