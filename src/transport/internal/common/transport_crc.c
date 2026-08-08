#include "transport_crc.h"

static const uint32_t HIL_TRANSPORT_CRC32_NIBBLE_TABLE[16] = {
    UINT32_C( 0x00000000 ), UINT32_C( 0x1DB71064 ), UINT32_C( 0x3B6E20C8 ),
    UINT32_C( 0x26D930AC ), UINT32_C( 0x76DC4190 ), UINT32_C( 0x6B6B51F4 ),
    UINT32_C( 0x4DB26158 ), UINT32_C( 0x5005713C ), UINT32_C( 0xEDB88320 ),
    UINT32_C( 0xF00F9344 ), UINT32_C( 0xD6D6A3E8 ), UINT32_C( 0xCB61B38C ),
    UINT32_C( 0x9B64C2B0 ), UINT32_C( 0x86D3D2D4 ), UINT32_C( 0xA00AE278 ),
    UINT32_C( 0xBDBDF21C ) };

uint32_t HIL_TRANSPORT_CRC32_Init( void )
{
    return UINT32_C( 0xFFFFFFFF );
}

uint32_t HIL_TRANSPORT_CRC32_Update( uint32_t crc, const uint8_t* data, size_t len )
{
    size_t index;

    if ( ( data == NULL ) && ( len != 0u ) )
    {
        return crc;
    }

    for ( index = 0u; index < len; index++ )
    {
        crc ^= data[index];
        crc = ( crc >> 4u ) ^ HIL_TRANSPORT_CRC32_NIBBLE_TABLE[crc & UINT32_C( 0x0F )];
        crc = ( crc >> 4u ) ^ HIL_TRANSPORT_CRC32_NIBBLE_TABLE[crc & UINT32_C( 0x0F )];
    }

    return crc;
}

uint32_t HIL_TRANSPORT_CRC32_Finish( uint32_t crc )
{
    return crc ^ UINT32_C( 0xFFFFFFFF );
}

uint32_t HIL_TRANSPORT_CRC32_Compute( const uint8_t* data, size_t len )
{
    return HIL_TRANSPORT_CRC32_Finish(
        HIL_TRANSPORT_CRC32_Update( HIL_TRANSPORT_CRC32_Init(), data, len ) );
}
