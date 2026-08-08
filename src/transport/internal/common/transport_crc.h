/**
 * @file transport_crc.h
 * @brief Internal integrity-check boundary for transport frame codecs.
 *
 * @details The MVP frame encoder and decoder use these helpers to calculate the
 * implemented wire integrity field. This private seam may be included by
 * internal tests without creating a public compatibility promise.
 *
 * @details The MVP uses CRC-32/ISO-HDLC: reflected polynomial 0xEDB88320,
 * initial value 0xFFFFFFFF, reflected input/output, and final XOR 0xFFFFFFFF.
 * The frame codec covers the decoded header and payload, excluding the CRC,
 * COBS overhead, and trailing delimiter. Public callers never provide a
 * calculated integrity value. This detects accidental corruption and is not
 * authentication.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_COMMON_TRANSPORT_CRC_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_COMMON_TRANSPORT_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create the initial accumulator for a new integrity calculation.
 *
 * @details Use this rather than embedding the initial value so incremental and
 * complete calculations remain consistent.
 *
 * @return Initial CRC-32/ISO-HDLC accumulator.
 */
uint32_t HIL_TRANSPORT_CRC32_Init( void );

/**
 * @brief Extend an integrity accumulator with another byte span.
 *
 * @details This supports frames assembled from separate header and payload
 * spans without temporary concatenation or heap allocation. Processing chunks
 * in order must produce the same result as one complete call.
 *
 * @param[in] crc Accumulator returned by Init or a previous Update.
 * @param[in] data Bytes to incorporate; may be NULL only when len is zero.
 * @param[in] len Number of bytes at data.
 *
 * @return Accumulator after incorporating the supplied bytes.
 */
uint32_t HIL_TRANSPORT_CRC32_Update( uint32_t crc, const uint8_t* data, size_t len );

/**
 * @brief Convert an incremental accumulator to its final integrity value.
 *
 * @details Applies the CRC-32/ISO-HDLC final XOR exactly once.
 *
 * @param[in] crc Accumulator after the final Update.
 *
 * @return Final host-order value after applying the final XOR.
 */
uint32_t HIL_TRANSPORT_CRC32_Finish( uint32_t crc );

/**
 * @brief Calculate the finalized integrity value for one contiguous byte span.
 *
 * @details Equivalent to Init, one Update, and Finish. It is a convenience for
 * complete raw-frame regions and tests.
 *
 * @param[in] data Bytes to incorporate; may be NULL only when len is zero.
 * @param[in] len Number of bytes at data.
 *
 * @return Final integrity value from the complete CRC-32/ISO-HDLC calculation.
 */
uint32_t HIL_TRANSPORT_CRC32_Compute( const uint8_t* data, size_t len );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_COMMON_TRANSPORT_CRC_H */
