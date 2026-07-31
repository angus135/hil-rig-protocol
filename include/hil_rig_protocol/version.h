/**
 * @file version.h
 * @brief Library package version for hil-rig-protocol.
 *
 * @details These values identify the C library release. They are deliberately
 * separate from the transport wire-protocol version declared in
 * transport_types.h: implementation releases may change without changing the
 * wire format, and a future library may support more than one wire version.
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

/**
 * @brief Get the library major version.
 *
 * @return HIL_RIG_PROTOCOL_VERSION_MAJOR.
 */
uint32_t HIL_RIG_PROTOCOL_Version_Major( void );

/**
 * @brief Get the library minor version.
 *
 * @return HIL_RIG_PROTOCOL_VERSION_MINOR.
 */
uint32_t HIL_RIG_PROTOCOL_Version_Minor( void );

/**
 * @brief Get the library patch version.
 *
 * @return HIL_RIG_PROTOCOL_VERSION_PATCH.
 */
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
