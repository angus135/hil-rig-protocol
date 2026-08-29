/**
 * @file version.h
 * @brief Library package version for hil-rig-protocol.
 *
 * @details VERSION in the repository root is the release metadata authority.
 * This checked-in public header mirrors that value so direct source consumers
 * do not require a CMake-generated include tree. Repository validation fails if
 * these macros drift from VERSION.
 *
 * These values identify the C library release. They are deliberately separate
 * from the transport wire-protocol version declared in transport_types.h:
 * implementation releases may change without changing the wire format, and a
 * future library may support more than one wire version.
 */
#ifndef HIL_RIG_PROTOCOL_VERSION_H
#define HIL_RIG_PROTOCOL_VERSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Major component of the library's semantic version. */
#define HIL_RIG_PROTOCOL_VERSION_MAJOR 0u

/** Minor component of the library's semantic version. */
#define HIL_RIG_PROTOCOL_VERSION_MINOR 1u

/** Patch component of the library's semantic version. */
#define HIL_RIG_PROTOCOL_VERSION_PATCH 0u

/** Complete dotted library version string. */
#define HIL_RIG_PROTOCOL_VERSION_STRING "0.1.0"

/** Get the library major version. */
uint32_t HIL_RIG_PROTOCOL_Version_Major( void );

/** Get the library minor version. */
uint32_t HIL_RIG_PROTOCOL_Version_Minor( void );

/** Get the library patch version. */
uint32_t HIL_RIG_PROTOCOL_Version_Patch( void );

/**
 * @brief Get the complete library version string.
 *
 * @return Pointer to immutable static storage containing
 * HIL_RIG_PROTOCOL_VERSION_STRING. The caller must not modify or free it.
 */
const char* HIL_RIG_PROTOCOL_Version_String( void );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_VERSION_H */
