/**
 * @file application_rig_status.h
 * @brief Captured endpoint readiness and fault status, schema 1.
 * @details Fixed 12-byte payload, 35-byte complete message, no decode storage.
 * Native layout is not the wire format. Hardware readiness is endpoint-owned.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RIG_STATUS_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RIG_STATUS_H

#include "hil_rig_protocol/application/application_failure.h"
#include <stdint.h>

/** Distinguishes the single solicited GET_STATUS reply from notifications. */
typedef enum
{
    HIL_APPLICATION_STATUS_ORIGIN_INVALID        = 0,
    HIL_APPLICATION_STATUS_ORIGIN_QUERY_RESPONSE = 1,
    HIL_APPLICATION_STATUS_ORIGIN_NOTIFICATION   = 2
} HIL_Application_Status_Origin_T;

/** Stable public lifecycle states; map endpoint states explicitly. */
typedef enum
{
    HIL_APPLICATION_RIG_STATE_INVALID       = 0,
    HIL_APPLICATION_RIG_STATE_INITIALISING  = 1,
    HIL_APPLICATION_RIG_STATE_IDLE          = 2,
    HIL_APPLICATION_RIG_STATE_UPLOADING     = 3,
    HIL_APPLICATION_RIG_STATE_CONFIGURING   = 4,
    HIL_APPLICATION_RIG_STATE_ARMED         = 5,
    HIL_APPLICATION_RIG_STATE_RUNNING       = 6,
    HIL_APPLICATION_RIG_STATE_FINALISING    = 7,
    HIL_APPLICATION_RIG_STATE_RESULTS_READY = 8,
    HIL_APPLICATION_RIG_STATE_TRANSFERRING  = 9,
    HIL_APPLICATION_RIG_STATE_RECOVERING    = 10,
    HIL_APPLICATION_RIG_STATE_FAULT         = 11
} HIL_Application_Rig_State_T;

/** Bits describing the captured instant, without reserving future readiness. */
typedef enum
{
    HIL_APPLICATION_RIG_STATUS_READY_FOR_NEW_TEST = 0x01,
    HIL_APPLICATION_RIG_STATUS_TRANSITION_PENDING = 0x02,
    HIL_APPLICATION_RIG_STATUS_RESET_PERMITTED    = 0x04,
    HIL_APPLICATION_RIG_STATUS_EXECUTION_ACTIVE   = 0x08
} HIL_Application_Rig_Status_Flag_T;

/** Snapshot; an envelope Test ID denotes the currently active transaction. */
typedef struct
{
    /** Exactly 1; other schemas are unsupported. */
    uint16_t schema_version;
    /** QUERY_RESPONSE completes GET_STATUS; NOTIFICATION does not. */
    HIL_Application_Status_Origin_T origin;
    /** Captured public lifecycle state. */
    HIL_Application_Rig_State_T state;
    /** Defined status bits only. READY requires IDLE, RESET_PERMITTED, no ID/failure/activity. */
    uint32_t flags;
    /** First failure provenance, or NONE. */
    HIL_Application_Failure_Source_T failure_source;
    /** Stage captured when the first failure occurred, or NONE. */
    HIL_Application_Failure_Stage_T failure_stage;
    /** Stable reason; NONE requires source/stage NONE. */
    HIL_Application_Failure_Reason_T failure_reason;
} HIL_Application_Rig_Status_T;

#endif
