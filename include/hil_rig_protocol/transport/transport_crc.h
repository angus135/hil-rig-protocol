/**
 * @file transport_crc.h
 * @brief CRC helper API for transport frame validation.
 *
 * @details These helpers will eventually provide the CRC32 calculation used by
 * the transport frame encoder and decoder.
 *
 * @note TODO: Fix the CRC polynomial, initial value, final XOR, reflection
 * behaviour, and byte order before publishing golden vectors.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_CRC_H
#define HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Return the initial CRC32 accumulator value.
 *
 * @return Initial CRC32 value.
 */
uint32_t HIL_TRANSPORT_CRC32_Init( void );

/**
 * @brief Update a CRC32 accumulator with bytes.
 *
 * @param crc Current CRC32 accumulator.
 * @param data Bytes to include in the CRC.
 * @param len Number of bytes in `data`.
 * @return Updated CRC32 accumulator.
 */
uint32_t HIL_TRANSPORT_CRC32_Update( uint32_t crc, const uint8_t* data, size_t len );

/**
 * @brief Finalize a CRC32 accumulator.
 *
 * @param crc Current CRC32 accumulator.
 * @return Final CRC32 value.
 */
uint32_t HIL_TRANSPORT_CRC32_Finish( uint32_t crc );

/**
 * @brief Compute CRC32 for a complete byte span.
 *
 * @param data Bytes to include in the CRC.
 * @param len Number of bytes in `data`.
 * @return Final CRC32 value.
 */
uint32_t HIL_TRANSPORT_CRC32_Compute( const uint8_t* data, size_t len );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_CRC_H */
