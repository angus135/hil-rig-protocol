/**
 * @file transport_cobs.h
 * @brief Validating private adapter around the vendored ordinary COBS codec.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_COMMON_TRANSPORT_COBS_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_COMMON_TRANSPORT_COBS_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/transport/transport_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Calculate the maximum ordinary COBS body size for input_size decoded bytes. */
HIL_Transport_Status_T HIL_TRANSPORT_COBS_Max_Encoded_Size( size_t  input_size,
                                                            size_t* encoded_size );

/**
 * @brief Encode one nonempty byte span using ordinary COBS.
 *
 * @details Input and output are caller-owned, bounded, non-overlapping buffers.
 * The adapter allocates nothing and publishes encoded_size only on success.
 */
HIL_Transport_Status_T HIL_TRANSPORT_COBS_Encode( const uint8_t* input, size_t input_size,
                                                  uint8_t* output, size_t output_size,
                                                  size_t* encoded_size );

/**
 * @brief Decode one nonempty COBS body without a trailing delimiter.
 *
 * @details Rejects embedded zero bytes, impossible code lengths, insufficient
 * output capacity, null/length mismatches, and overlapping buffers.
 */
HIL_Transport_Status_T HIL_TRANSPORT_COBS_Decode( const uint8_t* input, size_t input_size,
                                                  uint8_t* output, size_t output_size,
                                                  size_t* decoded_size );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_COMMON_TRANSPORT_COBS_H */
