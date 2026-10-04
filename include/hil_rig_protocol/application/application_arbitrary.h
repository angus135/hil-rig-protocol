/**
 * @file application_arbitrary.h
 * @brief Direction-neutral endpoint-defined control and opaque data containers.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_ARBITRARY_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_ARBITRARY_H

#include "hil_rig_protocol/application/application_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Fixed eight-byte payload. Both fields are ordinary endpoint-defined u32 values. */
typedef struct
{
    /** Endpoint-defined identifier; every uint32_t value is structurally valid. */
    uint32_t control_id;
    /** Endpoint-defined argument or bitfield; no implicit acknowledgement. */
    uint32_t value;
} HIL_Application_Arbitrary_Control_T;

/** Six-byte prefix followed by exactly payload.size opaque bytes. */
typedef struct
{
    /** Endpoint-defined identifier; interpretation belongs to the receiving handler. */
    uint32_t data_id;
    /** Synchronously borrowed bytes; empty is valid, decoding copies into caller storage. */
    HIL_Application_Byte_Span_T payload;
} HIL_Application_Arbitrary_Data_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_ARBITRARY_H */
