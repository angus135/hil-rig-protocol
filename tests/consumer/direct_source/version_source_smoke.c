#include "hil_rig_protocol/version.h"

#include <string.h>

int main( void )
{
    if ( HIL_RIG_PROTOCOL_Version_Major() != HIL_RIG_PROTOCOL_VERSION_MAJOR )
    {
        return 1;
    }
    if ( HIL_RIG_PROTOCOL_Version_Minor() != HIL_RIG_PROTOCOL_VERSION_MINOR )
    {
        return 2;
    }
    if ( HIL_RIG_PROTOCOL_Version_Patch() != HIL_RIG_PROTOCOL_VERSION_PATCH )
    {
        return 3;
    }
    if ( strcmp( HIL_RIG_PROTOCOL_Version_String(), HIL_RIG_PROTOCOL_VERSION_STRING ) != 0 )
    {
        return 4;
    }
    return 0;
}
