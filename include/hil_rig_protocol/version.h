#ifndef HIL_RIG_PROTOCOL_VERSION_H
#define HIL_RIG_PROTOCOL_VERSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HIL_RIG_PROTOCOL_VERSION_MAJOR 0u
#define HIL_RIG_PROTOCOL_VERSION_MINOR 1u
#define HIL_RIG_PROTOCOL_VERSION_PATCH 0u
#define HIL_RIG_PROTOCOL_VERSION_STRING "0.1.0"

uint32_t    HIL_RIG_PROTOCOL_Version_Major( void );
uint32_t    HIL_RIG_PROTOCOL_Version_Minor( void );
uint32_t    HIL_RIG_PROTOCOL_Version_Patch( void );
const char* HIL_RIG_PROTOCOL_Version_String( void );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_VERSION_H */
