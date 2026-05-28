#include "hil_rig_protocol/version.h"

uint32_t HIL_RIG_PROTOCOL_Version_Major( void )
{
    return HIL_RIG_PROTOCOL_VERSION_MAJOR;
}

uint32_t HIL_RIG_PROTOCOL_Version_Minor( void )
{
    return HIL_RIG_PROTOCOL_VERSION_MINOR;
}

uint32_t HIL_RIG_PROTOCOL_Version_Patch( void )
{
    return HIL_RIG_PROTOCOL_VERSION_PATCH;
}

const char* HIL_RIG_PROTOCOL_Version_String( void )
{
    return HIL_RIG_PROTOCOL_VERSION_STRING;
}
