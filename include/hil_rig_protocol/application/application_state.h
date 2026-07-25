/**
 * @file application_state.h
 * @brief Shared HIL-RIG Application Layer operating-state definition.
 *
 * @details The Application Layer owns these states, their legal transitions,
 * and every decision to accept, reject, or complete an Application operation.
 * Transport code may observe a caller-supplied state only where an explicitly
 * documented transport policy depends on it, such as selecting a maximum
 * advertised receive window. Observation does not transfer state ownership to
 * the Transport Layer and does not create Transport state-control traffic.
 *
 * This API-design revision intentionally defines only the shared state type.
 * Application commands, responses, transition rules, validation, and execution
 * behavior remain outside the scope of this Transport-focused change.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_STATE_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_STATE_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Application-owned HIL-RIG operating state.
 *
 * @details These values describe Application behavior, not transport-session,
 * communication-link, driver, DMA, interrupt, or RTOS state. The Application
 * Layer is authoritative for the current value and for all transitions.
 *
 * The numeric representation is an API definition only. This PR does not
 * define a Transport wire encoding for Application state, and Transport frames
 * must not serialize this enum as state-control metadata.
 */
typedef enum
{
    /** Connected and available, but not configuring or executing a test. */
    HIL_APPLICATION_STATE_IDLE = 0,

    /** Receiving, constructing, or validating an Application configuration. */
    HIL_APPLICATION_STATE_CONFIGURING,

    /** Configuration is accepted and the rig is waiting to start execution. */
    HIL_APPLICATION_STATE_ARMED,

    /** The rig Application is executing a test. */
    HIL_APPLICATION_STATE_RUNNING,

    /** The Application is preparing or making test results available. */
    HIL_APPLICATION_STATE_REPORTING,

    /** The Application is in a fault condition visible to its peer. */
    HIL_APPLICATION_STATE_FAULT,

    /**
     * Sentinel used when no valid Application state observation is available.
     *
     * This is not an operating state and must not be passed to
     * HIL_TRANSPORT_Process(). It exists so snapshots and internal metadata can
     * be initialized without implying that Transport owns an Application state.
     */
    HIL_APPLICATION_STATE_INVALID = 255
} HIL_Application_State_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_STATE_H */
