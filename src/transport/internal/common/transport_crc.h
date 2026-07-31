/**
 * @file transport_crc.h
 * @brief Internal integrity-check boundary for transport frame codecs.
 *
 * @details The frame encoder/decoder will use these helpers to calculate the
 * integrity field selected for a HIL-RIG wire protocol. This private seam may
 * be included by internal tests without creating a public compatibility promise.
 *
 * @note The polynomial, accumulator width/initial value, reflection behavior,
 * final XOR, wire byte order, and exact coverage remain TODO. The CRC name is a
 * design placeholder until those choices are approved. Public callers never
 * provide a calculated integrity value.
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
 * @details A future implementation will return the selected algorithm's exact
 * initial value. Use this rather than embedding that value so incremental and
 * complete calculations remain consistent.
 *
 * @return Initial accumulator. The current design-only stub returns zero.
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
 * @return Updated accumulator. The current design-only stub preserves crc.
 */
uint32_t HIL_TRANSPORT_CRC32_Update( uint32_t crc, const uint8_t* data, size_t len );

/**
 * @brief Convert an incremental accumulator to its final integrity value.
 *
 * @details A future implementation will apply the selected final XOR and any
 * other finalization exactly once.
 *
 * @param[in] crc Accumulator after the final Update.
 *
 * @return Final host-order value. The current design-only stub preserves crc.
 */
uint32_t HIL_TRANSPORT_CRC32_Finish( uint32_t crc );

/**
 * @brief Calculate the finalized integrity value for one contiguous byte span.
 *
 * @details The future implementation should be equivalent to Init, one Update,
 * and Finish. It is a convenience for complete raw-frame regions and tests.
 *
 * @param[in] data Bytes to incorporate; may be NULL only when len is zero.
 * @param[in] len Number of bytes at data.
 *
 * @return Final integrity value. The current design-only stub returns zero.
 */
uint32_t HIL_TRANSPORT_CRC32_Compute( const uint8_t* data, size_t len );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_COMMON_TRANSPORT_CRC_H */
