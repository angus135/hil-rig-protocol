/**
 * @file application_status.h
 * @brief Local result codes returned by the HIL-RIG Application C API.
 *
 * @details These values describe local function outcomes. They are distinct
 * from on-message HIL_Application_Response_Outcome_T values, Application error
 * messages, and Transport delivery or corruption conditions. Implementations
 * must never serialize this enum as an Application response or error.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_STATUS_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_STATUS_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Result of one local Application API operation.
 *
 * @details Numeric assignments are explicit for stable bindings and diagnostic
 * output. They do not define the eventual wire representation. Function-level
 * documentation specifies output guarantees for each result.
 */
typedef enum
{
    /** Operation completed successfully. */
    HIL_APPLICATION_STATUS_OK = 0,

    /** A pointer, alignment, enum, count, or argument combination is invalid. */
    HIL_APPLICATION_STATUS_INVALID_ARGUMENT = 1,

    /** The supplied context has not completed successful initialization. */
    HIL_APPLICATION_STATUS_UNINITIALIZED = 2,

    /** Caller-provided output or decode storage is smaller than required. */
    HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL = 3,

    /** The tagged message type is invalid or reserved. */
    HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE = 4,

    /** The subtype is invalid for the selected message type. */
    HIL_APPLICATION_STATUS_INVALID_SUBTYPE = 5,

    /** Encoded bytes violate the eventual Application envelope or body syntax. */
    HIL_APPLICATION_STATUS_MALFORMED_MESSAGE = 6,

    /** A complete-message input ends before all declared fields are present. */
    HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE = 7,

    /** A byte length, declared data length, or size relationship is invalid. */
    HIL_APPLICATION_STATUS_INVALID_LENGTH = 8,

    /** An element count is invalid or exceeds configured policy. */
    HIL_APPLICATION_STATUS_INVALID_COUNT = 9,

    /** Message feature or Application protocol version is unsupported. */
    HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE = 10,

    /** Test-ID presence is inconsistent with the selected message structure. */
    HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID = 11,

    /** Tick metadata is inconsistent with the selected message or response scope. */
    HIL_APPLICATION_STATUS_INCONSISTENT_TICK = 12,

    /** A typed message omits structurally required fixed or variable data. */
    HIL_APPLICATION_STATUS_INCOMPLETE_DATA = 13,

    /** Structural validation failed without a more specific result. */
    HIL_APPLICATION_STATUS_VALIDATION_FAILED = 14,

    /** The declared API exists but its runtime behavior is intentionally absent. */
    HIL_APPLICATION_STATUS_NOT_IMPLEMENTED = 15,

    /** A library-private invariant failed without a more specific status. */
    HIL_APPLICATION_STATUS_INTERNAL_ERROR = 16
} HIL_Application_Status_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_STATUS_H */
