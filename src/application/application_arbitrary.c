/**
 * @file application_arbitrary.c
 * @brief Explicit, bounded codecs for endpoint-defined messages.
 */
#include "application_arbitrary.h"
#include "application_internal.h"

#include <string.h>

HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Control_validate( const HIL_Application_Context_T*           context,
                                            const HIL_Application_Arbitrary_Control_T* data )
{
    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    return context->initialized == 0u ? HIL_APPLICATION_STATUS_UNINITIALIZED
                                      : HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Data_validate( const HIL_Application_Context_T*        context,
                                         const HIL_Application_Arbitrary_Data_T* data )
{
    if ( context == NULL || data == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( context->initialized == 0u )
    {
        return HIL_APPLICATION_STATUS_UNINITIALIZED;
    }
    if ( data->payload.size > context->config.max_variable_data_size
         || ( data->payload.size != 0u && data->payload.data == NULL ) )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Data_size( const HIL_Application_Context_T*        context,
                                     const HIL_Application_Arbitrary_Data_T* data,
                                     size_t*                                 payload_size )
{
    if ( payload_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    const HIL_Application_Status_T status =
        HIL_APPLICATION_Arbitrary_Data_validate( context, data );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_ARBITRARY_DATA_PREFIX_SIZE,
                                            data->payload.size, payload_size ) )
    {
        return HIL_APPLICATION_STATUS_INVALID_LENGTH;
    }
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Control_encode( const HIL_Application_Arbitrary_Control_T* data,
                                          uint8_t* payload, size_t capacity, size_t* used_size )
{
    if ( data == NULL || payload == NULL || used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( capacity < HIL_APPLICATION_ARBITRARY_CONTROL_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Write_U32_Le( payload, data->control_id );
    HIL_APPLICATION_Write_U32_Le( &payload[4], data->value );
    *used_size = HIL_APPLICATION_ARBITRARY_CONTROL_PAYLOAD_SIZE;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Data_encode( const HIL_Application_Context_T*        context,
                                       const HIL_Application_Arbitrary_Data_T* data,
                                       uint8_t* payload, size_t capacity, size_t* used_size )
{
    size_t                   payload_size = 0u;
    HIL_Application_Status_T status =
        HIL_APPLICATION_Arbitrary_Data_size( context, data, &payload_size );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( payload == NULL || used_size == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( capacity < payload_size )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    HIL_APPLICATION_Write_U32_Le( payload, data->data_id );
    HIL_APPLICATION_Write_U16_Le( &payload[HIL_APPLICATION_ARBITRARY_DATA_LENGTH_OFFSET],
                                  data->payload.size );
    if ( data->payload.size != 0u )
    {
        memcpy( &payload[HIL_APPLICATION_ARBITRARY_DATA_PREFIX_SIZE], data->payload.data,
                data->payload.size );
    }
    *used_size = payload_size;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Control_decode( HIL_Application_Arbitrary_Control_T* data,
                                          const uint8_t* payload, size_t payload_size,
                                          size_t* consumed_size, size_t* used_storage )
{
    if ( data == NULL || payload == NULL || consumed_size == NULL || used_storage == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    if ( payload_size != HIL_APPLICATION_ARBITRARY_CONTROL_PAYLOAD_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    data->control_id = HIL_APPLICATION_Read_U32_Le( payload );
    data->value      = HIL_APPLICATION_Read_U32_Le( &payload[4] );
    *consumed_size   = payload_size;
    *used_storage    = 0u;
    return HIL_APPLICATION_STATUS_OK;
}

/** Check the internal length before applying local policy, without borrowing output storage. */
HIL_Application_Status_T
HIL_APPLICATION_Arbitrary_Data_scan( const HIL_Application_Context_T* context,
                                     const uint8_t* payload, size_t payload_size,
                                     size_t* required_storage )
{
    if ( context == NULL || payload == NULL || required_storage == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    *required_storage = 0u;
    if ( payload_size < HIL_APPLICATION_ARBITRARY_DATA_PREFIX_SIZE )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    const uint16_t length =
        HIL_APPLICATION_Read_U16_Le( &payload[HIL_APPLICATION_ARBITRARY_DATA_LENGTH_OFFSET] );
    size_t expected_size = 0u;
    if ( !HIL_APPLICATION_Checked_Add_Size( HIL_APPLICATION_ARBITRARY_DATA_PREFIX_SIZE, length,
                                            &expected_size )
         || payload_size != expected_size )
    {
        return HIL_APPLICATION_STATUS_MALFORMED_MESSAGE;
    }
    if ( length > context->config.max_variable_data_size )
    {
        return HIL_APPLICATION_STATUS_VALIDATION_FAILED;
    }
    *required_storage = length;
    return HIL_APPLICATION_STATUS_OK;
}

HIL_Application_Status_T HIL_APPLICATION_Arbitrary_Data_decode(
    const HIL_Application_Context_T* context, HIL_Application_Arbitrary_Data_T* data,
    const uint8_t* payload, size_t payload_size, size_t* consumed_size, uint8_t* storage,
    size_t storage_capacity, size_t* used_storage )
{
    if ( data == NULL || consumed_size == NULL || used_storage == NULL )
    {
        return HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    }
    size_t                         required_storage = 0u;
    const HIL_Application_Status_T status =
        HIL_APPLICATION_Arbitrary_Data_scan( context, payload, payload_size, &required_storage );
    if ( status != HIL_APPLICATION_STATUS_OK )
    {
        return status;
    }
    if ( required_storage > storage_capacity || ( required_storage != 0u && storage == NULL ) )
    {
        return HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL;
    }
    data->data_id      = HIL_APPLICATION_Read_U32_Le( payload );
    data->payload.size = ( uint16_t )required_storage;
    data->payload.data = required_storage == 0u ? NULL : storage;
    if ( required_storage != 0u )
    {
        memcpy( storage, &payload[HIL_APPLICATION_ARBITRARY_DATA_PREFIX_SIZE], required_storage );
    }
    *consumed_size = payload_size;
    *used_storage  = required_storage;
    return HIL_APPLICATION_STATUS_OK;
}
