/**
 * @file application_system_info.h
 * @brief Minimal System Information request and response bodies.
 *
 * @details System Information Request is Python to firmware and System
 * Information Response is firmware to Python. Neither is associated with a Test
 * ID. The codec is direction-neutral; endpoint handlers enforce these
 * directions after decoding. Capability negotiation and extensible feature
 * discovery remain deferred.
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

/**
 * @brief Python-to-firmware System Information request body.
 *
 * @details Structural validation accepts representable foreign discovery
 * versions. Encoded sizing and encoding require the protocol triplet to equal
 * this library's compiled version.
 */
typedef struct
{
    /**
     * Nonzero requests an optional firmware Git hash when available.
     *
     * The peer may return an empty hash when firmware policy omits it.
     */
    uint8_t request_firmware_git_hash;
    /** Requested diagnostic record; initially only BASIC is defined. */
    HIL_Application_System_Info_Query_T query;
    /** Application protocol major version used for exact compatibility confirmation. */
    uint16_t application_protocol_major;
    /** Application protocol minor version used for exact compatibility confirmation. */
    uint16_t application_protocol_minor;
    /** Application protocol patch version used for exact compatibility confirmation. */
    uint16_t application_protocol_patch;
} HIL_Application_System_Info_Request_T;

/**
 * @brief Firmware-to-Python System Information response body.
 *
 * @details Version fields are diagnostics rather than a compatibility
 * negotiation mechanism. For a typed message to be structurally valid, the
 * application_protocol_major and application_protocol_minor fields must fit the
 * discovery envelope bytes; patch is the full uint16_t value. They do not select
 * or change that version. Encoded sizing and encoding require the protocol
 * triplet to equal this library's compiled version. Endpoint integration compares
 * decoded values exactly with its compiled version before accepting test traffic.
 * firmware_git_hash and diagnostic_data are borrowed during encoding
 * and point into caller decode storage after successful
 * decoding.
 * Integration may expose firmware-specific runtime diagnostics in
 * diagnostic_data, but those bytes do not define a shared protocol or firmware
 * state machine. Precise content/schema conventions and family-specific
 * semantic limits remain deferred. Each span uses the common uint8_t wire
 * length and therefore carries at most 255 data bytes.
 *
 * The current wire order is repository protocol major/minor/patch, firmware
 * major/minor/patch, diagnostic_data span, then firmware_git_hash span. The
 * public C member order does not define this wire order.
 */
typedef struct
{
    /** Diagnostic repository-wide HIL-RIG protocol major version reported by the peer. */
    uint16_t application_protocol_major;
    /** Diagnostic repository-wide HIL-RIG protocol minor version reported by the peer. */
    uint16_t application_protocol_minor;
    /** Repository-wide protocol patch diagnostic; legacy member name retained. */
    uint16_t application_protocol_patch;
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
