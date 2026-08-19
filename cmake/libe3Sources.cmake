# libe3 Source Files
#
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

# The reader for the ring format and the converter built on it, installed so
# downstream users of latrec.h get the tooling that parses what it writes.
# latrec2csv.py needs numpy; latrec_reader.py is standard library only.
# latrec.sh drives a capture; its `on` and `off` must be sourced.
set(LIBE3_TOOLS
    tools/latrec_reader.py
    tools/latrec2csv.py
    tools/latrec.sh
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

# The latrec TLS ring registry backs include/libe3/latrec.h's convenience API
# (latrec_tstamp/latrec_tls_open_as/latrec_seq_next). Excluded entirely when
# the recorder is off, so a normal build carries none of it; latrec.h falls
# back to inline no-op stubs for that same API in that case.
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
