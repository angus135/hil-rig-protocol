/**
 * @file transport_receive_mvp.h
 * @brief Private transactional byte-stream receive coordinator for the MVP.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_RECEIVE_MVP_H
#define HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_RECEIVE_MVP_H

#include "transport_types_mvp.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Accept an exact stream prefix while retaining blocked parser-body transactions. */
HIL_Transport_Status_T HIL_TRANSPORT_MVP_Receive_Bytes( HIL_Transport_Mvp_Root_T* root,
                                                        const uint8_t* data, size_t data_size,
                                                        size_t* bytes_consumed );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_INTERNAL_MVP_TRANSPORT_RECEIVE_MVP_H */
