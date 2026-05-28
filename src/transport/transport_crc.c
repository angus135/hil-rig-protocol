#include "hil_rig_protocol/transport/transport_crc.h"

uint32_t HIL_TRANSPORT_CRC32_Init( void )
{
    /*
     * TODO: Replace this placeholder once the CRC32 polynomial, initial value,
     * reflection settings, and final XOR are fixed.
     */
    return 0u;
}

uint32_t HIL_TRANSPORT_CRC32_Update( uint32_t crc, const uint8_t* data, size_t len )
{
    /*
     * TODO: Implement the selected CRC32 algorithm. The current stub preserves
     * the accumulator so callers can compile without receiving real CRCs.
     */
    ( void )data;
    ( void )len;

    return crc;
}

uint32_t HIL_TRANSPORT_CRC32_Finish( uint32_t crc )
{
    /*
     * TODO: Apply the selected final XOR/reflection behaviour.
     */
    return crc;
}

uint32_t HIL_TRANSPORT_CRC32_Compute( const uint8_t* data, size_t len )
{
    uint32_t crc = HIL_TRANSPORT_CRC32_Init();

    crc = HIL_TRANSPORT_CRC32_Update( crc, data, len );

    return HIL_TRANSPORT_CRC32_Finish( crc );
}
