#include "transport_frame_codec_mvp.h"

size_t HIL_TRANSPORT_MVP_Max_Encoded_Size( size_t maximum_application_message_size )
{
    /*
     * TODO: Validate the nonzero complete-message limit and use checked
     * arithmetic for the approved minimal header, integrity, framing expansion,
     * and delimiter. Return zero until those wire decisions are approved.
     */
    ( void )maximum_application_message_size;
    return 0u;
}

HIL_Transport_Status_T HIL_TRANSPORT_MVP_Encode_Frame( const HIL_Transport_Mvp_Frame_T* frame,
                                                       uint8_t* out_buffer, size_t out_buffer_size,
                                                       size_t* output_size )
{
    /*
     * TODO: Clear and require output_size; validate the minimal frame category,
     * role, session identity, handshake/sequence/ACK semantics (including the
     * reliable INITIATE, RESPONSE, CONFIRM, and CONFIRM ACK relationship), one
     * complete Application payload, and output capacity; explicitly serialize
     * approved fixed-width fields; calculate integrity; and publish only
     * complete bytes. Retried reliable work must reuse the retained output rather
     * than calling the encoder to create replacement bytes.
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
HIL_TRANSPORT_MVP_Decode_Frame( const uint8_t* encoded_body, size_t encoded_body_size,
                                HIL_Transport_Mvp_Frame_T* frame, uint8_t* message_buffer,
                                size_t message_buffer_size, size_t* message_size,
                                HIL_Transport_Mvp_Decode_Result_T* decode_result )
{
    /*
     * TODO: Clear outputs; validate exact body/framing/integrity and minimal MVP
     * fields; copy a complete Application message only after success; retain no
     * input pointer; and publish a private classification only after complete
     * validation so the profile maps malformed/integrity/session conditions to
     * PROTOCOL_ERROR rather than leaking an internal numeric status.
     */
    ( void )encoded_body;
    ( void )encoded_body_size;
    ( void )frame;
    ( void )message_buffer;
    ( void )message_buffer_size;
    ( void )decode_result;
    if ( message_size != NULL )
    {
        *message_size = 0u;
    }
    return HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED;
}
