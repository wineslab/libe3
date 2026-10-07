# libe3 Source Files
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

set(LIBE3_PUBLIC_HEADERS
    include/libe3/types.hpp
    include/libe3/logger.hpp
    include/libe3/e3_connector.hpp
    include/libe3/e3_encoder.hpp
    include/libe3/mpmc_queue.hpp
    include/libe3/subscription_manager.hpp
    include/libe3/lockfree_queue.hpp
    include/libe3/sm_interface.hpp
    include/libe3/e3_interface.hpp
    include/libe3/e3_agent.hpp
    include/libe3/libe3.hpp
    include/libe3/c_api.h
    include/libe3/error_codes.h
    include/libe3/latrec.h
)

# Tooling for the ring format, installed alongside latrec.h. See docs/latrec.md.
set(LIBE3_TOOLS
    tools/latrec_reader.py
    tools/latrec2csv.py
)

set(LIBE3_SOURCES
    # Core
    src/core/e3_agent.cpp
    src/core/e3_interface.cpp
    src/core/subscription_manager.cpp
    src/core/sm_registry.cpp

    # Encoder
    src/encoder/e3_encoder.cpp
    src/encoder/encoder_factory.cpp

    # Connector
    src/connector/connector_factory.cpp
    src/connector/posix_connector.cpp
    src/c_api.cpp
)

# Conditionally add ZMQ connector
if(LIBE3_ENABLE_ZMQ)
    list(APPEND LIBE3_SOURCES src/connector/zmq_connector.cpp)
endif()

# Backs latrec.h's TLS convenience API; excluded when the recorder is off, where
# latrec.h falls back to inline no-op stubs. See docs/latrec.md.
if(LIBE3_ENABLE_LATREC)
    list(APPEND LIBE3_SOURCES src/core/latrec.c)
endif()

# Conditionally include encoder implementations
if(LIBE3_ENABLE_ASN1)
    list(APPEND LIBE3_SOURCES src/encoder/asn1_encoder.cpp)
endif()

if(LIBE3_ENABLE_JSON)
    list(APPEND LIBE3_SOURCES src/encoder/json_encoder.cpp)
endif()

if(LIBE3_ENABLE_PROTOBUF)
    list(APPEND LIBE3_SOURCES src/encoder/protobuf_encoder.cpp)
endif()

# E2SM-DAPP codec (libe3::e2sm_dapp): a self-contained pair of libraries, built from
# its own sources plus the asn1_e2sm_dapp objects, and kept out of LIBE3_SOURCES so
# that the main libe3 target never depends on them.
if(LIBE3_ENABLE_E2SM_DAPP)
    set(LIBE3_E2SM_DAPP_PUBLIC_HEADERS
        include/libe3/e2sm_dapp.hpp
        include/libe3/e2sm_dapp_c.h
    )
    set(LIBE3_E2SM_DAPP_SOURCES
        src/e2sm_dapp/e2sm_dapp.cpp
        src/e2sm_dapp/e2sm_dapp_c.cpp
    )
    # Export lists for the shared library: only the public API leaves it.
    set(LIBE3_E2SM_DAPP_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/e2sm_dapp/e2sm_dapp.map")
    set(LIBE3_E2SM_DAPP_EXPORTS_LIST "${CMAKE_CURRENT_SOURCE_DIR}/src/e2sm_dapp/e2sm_dapp.exp")
    # The grammar is installed beside the headers (see libe3Install.cmake).
    set(LIBE3_E2SM_DAPP_GRAMMAR_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn")
endif()
