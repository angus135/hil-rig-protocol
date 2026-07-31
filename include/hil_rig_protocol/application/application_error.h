/**
 * @file application_error.h
 * @brief Broader Application fault message body.
 *
 * @details Application Error messages describe faults broader than rejection
 * of one request. They are distinct from local C statuses, Application
 * Responses, and Transport delivery/corruption/connection failures. Transport
 * handles Transport-invalid data before Application decoding and reports
 * session events to integration; it does not synthesize this message.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_ERROR_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_ERROR_H

#include <stdint.h>

#include "hil_rig_protocol/application/application_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initial category for an Application-wide fault.
 *
 * @warning Values may become wire identifiers. Detailed sub-classification is
 * intentionally deferred.
 */
typedef enum
{
    /** Invalid sentinel. */
    HIL_APPLICATION_ERROR_CATEGORY_INVALID = 0,
    /** Power, electrical, or other hardware-health fault. */
    HIL_APPLICATION_ERROR_CATEGORY_HARDWARE = 1,
    /** Failure while executing a test. */
    HIL_APPLICATION_ERROR_CATEGORY_EXECUTION = 2,
    /** Application transaction or upload timeout. */
    HIL_APPLICATION_ERROR_CATEGORY_TIMEOUT = 3,
    /** Retained test data is corrupt or internally inconsistent. */
    HIL_APPLICATION_ERROR_CATEGORY_RETAINED_DATA = 4,
    /** Application transaction/protocol condition requiring recovery. */
    HIL_APPLICATION_ERROR_CATEGORY_PROTOCOL = 5,
    /** Unclassified internal Application failure. */
    HIL_APPLICATION_ERROR_CATEGORY_INTERNAL = 6,
    /** Reserved sentinel. */
    HIL_APPLICATION_ERROR_CATEGORY_RESERVED = 255
} HIL_Application_Error_Category_T;

/**
 * @brief Body of one Application Error message.
 *
 * @details The envelope may omit a test ID for a global hardware/power fault,
 * or include it when a specific upload/execution is affected. has_tick_number
 * explicitly controls tick_number presence rather than reserving a magic tick.
 * diagnostic_data is borrowed for encoding and points into caller decode
 * storage after decoding. A host can respond to a global or unidentified
 * recoverable fault with test-independent RESET_APPLICATION; the codec does not
 * perform that recovery and the operation does not reset Transport. An Error
 * detected during a successfully started test does not replace the required
 * fixed Test Result for any tick; communication/session loss or reset may make
 * completing that result set impossible.
 */
typedef struct
{
    /** Broad fault category. */
    HIL_Application_Error_Category_T category;
    /** Nonzero when recovery might be possible without power-cycle/service. */
    uint8_t recoverable;
    /** Nonzero when tick_number identifies an affected test tick. */
    uint8_t has_tick_number;
    /** Zero-based affected tick when present; integration validates its range. */
    uint32_t tick_number;
    /** Integration-defined detail value; final classification remains TODO. */
    uint32_t detail;
    /** Optional bounded diagnostic bytes with deferred schema. */
    HIL_Application_Byte_Span_T diagnostic_data;
} HIL_Application_Error_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_ERROR_H */
