#include "transport_frame_codec_extended.h"

size_t HIL_TRANSPORT_EXTENDED_Encoded_Size( const HIL_Transport_Extended_Frame_T* frame )
{
    /*
     * TODO: After extended wire fields are approved, validate the private frame
     * and use checked arithmetic for exact header, fragment, payload, integrity,
     * framing, and delimiter size. Do not serialize native structs or size_t.
     */
    ( void )frame;
    return 0u;
}

HIL_Transport_Status_T
HIL_TRANSPORT_EXTENDED_Encode_Frame( const HIL_Transport_Extended_Frame_T* frame,
                                     uint8_t* out_buffer, size_t out_buffer_size,
                                     size_t* output_size )
{
    /*
     * TODO: Validate private extended metadata, output capacity, and checked
     * lengths; explicitly serialize an approved wire format; calculate integrity
     * internally; and publish no partial output. Retain no caller pointer.
     */
    ( void )frame;
    ( void )out_buffer;
    ( void )out_buffer_size;
    if ( output_size != NULL )
    {
        *output_size = 0u;
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}

HIL_Transport_Status_T
HIL_TRANSPORT_EXTENDED_Decode_Frame( const uint8_t* encoded_body, size_t encoded_body_size,
                                     HIL_Transport_Extended_Frame_T* frame, uint8_t* payload_buffer,
                                     size_t payload_buffer_size, size_t* payload_size )
{
    /*
     * TODO: Defensively clear outputs; reverse the approved framing; validate
     * exact structure and integrity; decode fixed-width extended metadata; copy
     * payload only after full success; and report detailed classifications to
     * the extended profile for explicit mapping to public status/events.
     */
    ( void )encoded_body;
    ( void )encoded_body_size;
    ( void )frame;
    ( void )payload_buffer;
    ( void )payload_buffer_size;
    if ( payload_size != NULL )
    {
        *payload_size = 0u;
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}
