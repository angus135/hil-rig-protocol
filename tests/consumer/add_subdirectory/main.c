#include "hil_rig_protocol/transport/transport.h"
#include "hil_rig_protocol/version.h"

#include <string.h>

int main( void )
{
    HIL_Transport_Config_T config;
    HIL_TRANSPORT_Default_Config( &config );

    if ( config.max_application_message_size != HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE )
    {
        return 1;
    }
    if ( strcmp( HIL_RIG_PROTOCOL_Version_String(), HIL_RIG_PROTOCOL_VERSION_STRING ) != 0 )
    {
        return 2;
    }
    return 0;
}
