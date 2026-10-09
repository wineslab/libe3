# libe3py, the Python binding of libe3, built by pip (scikit-build-core, see
# ../pyproject.toml) against the libe3 already installed on the machine:
#
#   sudo ./build_libe3 --all-encodings --install    # once
#   pip install libe3py                             # or: pip install . from the checkout
#
# The module is compiled from the same SWIG sources as the in-tree build and links
# the installed static libe3, with the feature macros (encodings, latrec) that
# libe3 was built with. It refuses an installed libe3 of another version, so the
# binding and the library always match.
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.18)

set(LIBE3_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
file(READ "${LIBE3_SOURCE_DIR}/VERSION" LIBE3PY_VERSION)
string(STRIP "${LIBE3PY_VERSION}" LIBE3PY_VERSION)

project(libe3py VERSION ${LIBE3PY_VERSION} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libe3 CONFIG QUIET)
if(NOT libe3_FOUND)
    message(FATAL_ERROR
        "libe3py ${LIBE3PY_VERSION} is built against an installed libe3, and none was found.\n"
        "Install libe3 ${LIBE3PY_VERSION} first (https://github.com/wineslab/libe3):\n"
        "  git clone --branch ${LIBE3PY_VERSION} https://github.com/wineslab/libe3 && cd libe3\n"
        "  ./build_libe3 -I && ./build_libe3 --all-encodings --install\n"
        "For a libe3 outside /usr and /usr/local, pass its prefix: "
        "pip install libe3py --config-settings=cmake.define.CMAKE_PREFIX_PATH=<prefix>")
endif()
if(NOT libe3_VERSION VERSION_EQUAL LIBE3PY_VERSION)
    message(FATAL_ERROR
        "The installed libe3 is ${libe3_VERSION} (${libe3_DIR}), but this is libe3py ${LIBE3PY_VERSION}.\n"
        "Install the libe3py that matches it (pip install \"libe3py==${libe3_VERSION}\"), or install libe3 ${LIBE3PY_VERSION}.")
endif()
message(STATUS "Building libe3py ${LIBE3PY_VERSION} against libe3 ${libe3_VERSION} from ${libe3_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LIBE3_SOURCE_DIR}/cmake")
include(libe3SwigModule)

get_target_property(_libe3_inc libe3::libe3 INTERFACE_INCLUDE_DIRECTORIES)
set(_out "${CMAKE_CURRENT_BINARY_DIR}/swig")
libe3_add_python_module(libe3py
    SOURCE_DIR "${LIBE3_SOURCE_DIR}"
    OUTPUT_DIR "${_out}"
    LINK libe3::libe3
    INCLUDE ${_libe3_inc})

if(DEFINED SKBUILD_PLATLIB_DIR)
    set(_dest "${SKBUILD_PLATLIB_DIR}")
else()
    set(_dest ".")
endif()
install(TARGETS libe3py LIBRARY DESTINATION "${_dest}")
install(FILES "${_out}/libe3py.py" DESTINATION "${_dest}")
