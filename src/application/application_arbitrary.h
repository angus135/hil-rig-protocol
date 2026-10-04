/** Private codec helpers for the two endpoint-defined message containers. */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_ARBITRARY_INTERNAL_H
#define HIL_RIG_PROTOCOL_APPLICATION_ARBITRARY_INTERNAL_H

#include "hil_rig_protocol/application/application_arbitrary.h"
#include "hil_rig_protocol/application/application_status.h"

/** Validate the control representation, accepting every u32 ID and value. */
HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Control_validate( const HIL_Application_Context_T*           context,
                                            const HIL_Application_Arbitrary_Control_T* data );
/** Validate the data span pointer and configured byte bound. */
HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Data_validate( const HIL_Application_Context_T*        context,
                                         const HIL_Application_Arbitrary_Data_T* data );
/** Size the six-byte prefix and opaque bytes; the facade adds the envelope. */
HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Data_size( const HIL_Application_Context_T*        context,
                                     const HIL_Application_Arbitrary_Data_T* data,
                                     size_t*                                 payload_size );
/** Encode two u32 fields after checking the exact eight-byte capacity. */
HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Control_encode( const HIL_Application_Arbitrary_Control_T* data,
                                          uint8_t* payload, size_t capacity, size_t* used_size );
/** Encode the prefix and bytes, borrowing the source only for this call. */
HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Data_encode( const HIL_Application_Context_T*        context,
                                       const HIL_Application_Arbitrary_Data_T* data,
                                       uint8_t* payload, size_t capacity, size_t* used_size );
/** Decode the exact eight-byte control body without using storage. */
HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Control_decode( HIL_Application_Arbitrary_Control_T* data,
                                          const uint8_t* payload, size_t payload_size,
                                          size_t* consumed_size, size_t* used_storage );
/** Validate exact internal length and policy, reporting N storage bytes without allocating. */
HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Data_scan( const HIL_Application_Context_T* context,
                                     const uint8_t* payload, size_t payload_size,
                                     size_t* required_storage );
/** Decode the bounded body and detach the opaque bytes into caller-owned storage. */
HIL_Application_Status_T HIL_APPLICATION_Arbitrary_Data_decode(
    const HIL_Application_Context_T* context, HIL_Application_Arbitrary_Data_T* data,
    const uint8_t* payload, size_t payload_size, size_t* consumed_size, uint8_t* storage,
    size_t storage_capacity, size_t* used_storage );

#endif /* HIL_RIG_PROTOCOL_APPLICATION_ARBITRARY_INTERNAL_H */
