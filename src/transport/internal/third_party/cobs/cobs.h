/*****************************************************************************
 * Vendored from cmcqueen/cobs-c. See README.md and LICENSE.txt in this folder.
 ****************************************************************************/
#ifndef HIL_TRANSPORT_THIRD_PARTY_COBS_H
#define HIL_TRANSPORT_THIRD_PARTY_COBS_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    COBS_ENCODE_OK                  = 0x00,
    COBS_ENCODE_NULL_POINTER        = 0x01,
    COBS_ENCODE_OUT_BUFFER_OVERFLOW = 0x02
} cobs_encode_status;

typedef struct
{
    size_t             out_len;
    cobs_encode_status status;
} cobs_encode_result;

typedef enum
{
    COBS_DECODE_OK                  = 0x00,
    COBS_DECODE_NULL_POINTER        = 0x01,
    COBS_DECODE_OUT_BUFFER_OVERFLOW = 0x02,
    COBS_DECODE_ZERO_BYTE_IN_INPUT  = 0x04,
    COBS_DECODE_INPUT_TOO_SHORT     = 0x08
} cobs_decode_status;

typedef struct
{
    size_t             out_len;
    cobs_decode_status status;
} cobs_decode_result;

#ifdef __cplusplus
extern "C"
{
#endif

cobs_encode_result HIL_TRANSPORT_THIRD_PARTY_COBS_Encode( void* dst_buf_ptr, size_t dst_buf_len,
                                                          const void* src_ptr, size_t src_len );
cobs_decode_result HIL_TRANSPORT_THIRD_PARTY_COBS_Decode( void* dst_buf_ptr, size_t dst_buf_len,
                                                          const void* src_ptr, size_t src_len );

#ifdef __cplusplus
}
#endif

#endif /* HIL_TRANSPORT_THIRD_PARTY_COBS_H */
