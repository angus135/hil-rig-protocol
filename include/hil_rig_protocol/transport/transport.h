/**
 * @file transport.h
 * @brief Umbrella include for the HIL-RIG transport-layer public API.
 *
 * @details Include this header when a caller needs the complete transport API
 * surface. Individual transport headers may be included directly for narrower
 * dependencies.
 *
 * @note The transport layer exposes COBS-delimited frame, parser, CRC, and
 * session API boundaries without implementing protocol logic in this skeleton.
 */
#ifndef HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_H
#define HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_H

#include "hil_rig_protocol/transport/transport_crc.h"
#include "hil_rig_protocol/transport/transport_frame.h"
#include "hil_rig_protocol/transport/transport_parser.h"
#include "hil_rig_protocol/transport/transport_session.h"
#include "hil_rig_protocol/transport/transport_status.h"
#include "hil_rig_protocol/transport/transport_types.h"

#endif /* HIL_RIG_PROTOCOL_TRANSPORT_TRANSPORT_H */
