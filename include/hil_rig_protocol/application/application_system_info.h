/**
 * @file application_system_info.h
 * @brief Minimal System Information request and response bodies.
 *
 * @details System Information is diagnostic Application data, normally sent as
 * a host request and HIL-RIG response. It is not associated with a test ID.
 * Capability negotiation and extensible feature discovery remain deferred.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_SYSTEM_INFO_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_SYSTEM_INFO_H

#include <stdint.h>

#include "hil_rig_protocol/application/application_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief System Information request selector.
 *
 * @warning Numeric assignments may become wire identifiers and must not be
 * changed casually after a wire format is approved.
 */
typedef enum
{
    /** Invalid sentinel. */
    HIL_APPLICATION_SYSTEM_INFO_QUERY_INVALID = 0,
    /** Request the initial basic version and diagnostic record. */
    HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC = 1,
    /** Reserved sentinel, never valid in a message. */
    HIL_APPLICATION_SYSTEM_INFO_QUERY_RESERVED = 255
} HIL_Application_System_Info_Query_T;

/** Host-to-HIL-RIG System Information request body. */
typedef struct
{
    /** Requested diagnostic record; initially only BASIC is defined. */
    HIL_Application_System_Info_Query_T query;
    /**
     * Nonzero requests an optional firmware Git hash when available.
     *
     * The peer may return an empty hash when firmware policy omits it.
     */
    uint8_t request_firmware_git_hash;
} HIL_Application_System_Info_Request_T;

/**
 * @brief HIL-RIG-to-host System Information response body.
 *
 * @details Version fields are diagnostics rather than a complete compatibility
 * negotiation mechanism. git_hash and diagnostic_data are borrowed during
 * encoding and point into caller decode storage after successful decoding.
 * Integration may expose firmware-specific runtime diagnostics in
 * diagnostic_data, but those bytes do not define a shared protocol or firmware
 * state machine. Precise text/binary conventions and maximum wire lengths
 * remain TODO.
 */
typedef struct
{
    /** Application protocol major version reported by the peer. */
    uint16_t application_protocol_major;
    /** Application protocol minor version reported by the peer. */
    uint16_t application_protocol_minor;
    /** Firmware semantic-version major component. */
    uint16_t firmware_version_major;
    /** Firmware semantic-version minor component. */
    uint16_t firmware_version_minor;
    /** Firmware semantic-version patch component. */
    uint16_t firmware_version_patch;
    /** Optional firmware Git hash bytes; empty when unavailable or omitted. */
    HIL_Application_Byte_Span_T firmware_git_hash;
    /**
     * Optional basic diagnostic bytes.
     *
     * Detailed schema and future capabilities are intentionally deferred.
     */
    HIL_Application_Byte_Span_T diagnostic_data;
} HIL_Application_System_Info_Response_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_SYSTEM_INFO_H */
