# Central build-time selection for the private Transport implementation.
# Public callers do not receive a runtime profile enum or profile-specific API.

set(HIL_RIG_PROTOCOL_TRANSPORT_PROFILE "MVP" CACHE STRING
    "Private Transport implementation profile (MVP or EXTENDED)"
)
set_property(CACHE HIL_RIG_PROTOCOL_TRANSPORT_PROFILE PROPERTY STRINGS MVP EXTENDED)

string(TOUPPER "${HIL_RIG_PROTOCOL_TRANSPORT_PROFILE}"
    HIL_RIG_PROTOCOL_TRANSPORT_PROFILE_NORMALIZED
)

set(HIL_RIG_PROTOCOL_TRANSPORT_COMMON_SOURCES
    src/transport/internal/third_party/cobs/cobs.c
    src/transport/internal/common/transport_cobs.c
    src/transport/internal/common/transport_crc.c
    src/transport/internal/common/transport_parser.c
)

if(HIL_RIG_PROTOCOL_TRANSPORT_PROFILE_NORMALIZED STREQUAL "MVP")
    set(HIL_RIG_PROTOCOL_TRANSPORT_PROFILE_SOURCES
        src/transport/internal/mvp/transport_profile_mvp.c
        src/transport/internal/mvp/transport_frame_codec_mvp.c
        src/transport/internal/mvp/transport_session_mvp.c
        src/transport/internal/mvp/transport_reliability_mvp.c
        src/transport/internal/mvp/transport_control_output_mvp.c
    )
elseif(HIL_RIG_PROTOCOL_TRANSPORT_PROFILE_NORMALIZED STREQUAL "EXTENDED")
    message(FATAL_ERROR
        "HIL_RIG_PROTOCOL_TRANSPORT_PROFILE=EXTENDED is an integration skeleton and is not implemented. Use MVP."
    )
else()
    message(FATAL_ERROR
        "Unsupported HIL_RIG_PROTOCOL_TRANSPORT_PROFILE='${HIL_RIG_PROTOCOL_TRANSPORT_PROFILE}'. Expected MVP or EXTENDED."
    )
endif()
