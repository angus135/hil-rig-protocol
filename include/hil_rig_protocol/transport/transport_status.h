/**
 * @file transport_status.h
 * @brief Public status codes for the HIL-RIG transport layer.
 *
 * @details These status values are shared by transport frame encoding,
 * decoding, stream parsing, CRC helpers, and session/reliability helpers.
 *
 * @note The values are API-level results and are not a committed wire format.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_STATUS_H
#define HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result codes returned by transport-layer APIs.
 */
typedef enum
{
    HIL_TRANSPORT_STATUS_OK = 0,
    HIL_TRANSPORT_STATUS_INVALID_ARGUMENT,
    HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL,
    HIL_TRANSPORT_STATUS_MALFORMED_FRAME,
    HIL_TRANSPORT_STATUS_BAD_CRC,
    HIL_TRANSPORT_STATUS_UNSUPPORTED_VERSION,
    HIL_TRANSPORT_STATUS_INVALID_STATE,
    HIL_TRANSPORT_STATUS_UNEXPECTED_SEQUENCE,
    HIL_TRANSPORT_STATUS_DUPLICATE_FRAME,
    HIL_TRANSPORT_STATUS_TIMEOUT,
    HIL_TRANSPORT_STATUS_NOT_READY,
    HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED,
    HIL_TRANSPORT_STATUS_INTERNAL_ERROR
} HIL_Transport_Status_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_STATUS_H */
