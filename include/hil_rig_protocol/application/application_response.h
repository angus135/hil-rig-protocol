/**
 * @file application_response.h
 * @brief Application acceptance, rejection, retention, and control outcomes.
 *
 * @details Responses are ordinary complete Application messages, normally
 * HIL-RIG to host. They are distinct from Transport delivery ACKs and local
 * HIL_Application_Status_T values.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RESPONSE_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RESPONSE_H

#include <stdint.h>

#include "hil_rig_protocol/application/application_control.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Operation correlated by an Application Response.
 *
 * @warning Numeric values may become wire identifiers.
 */
typedef enum
{
    /** Invalid sentinel. */
    HIL_APPLICATION_RESPONSE_SCOPE_INVALID = 0,
    /**
     * Acceptance/rejection of Test Configuration proposing an upload.
     *
     * ACCEPTED creates the active upload transaction for the envelope Test ID.
     * A negative outcome creates no transaction.
     */
    HIL_APPLICATION_RESPONSE_SCOPE_TEST_CONFIGURATION = 1,
    /**
     * Semantic acceptance and retention responsibility for one complete tick.
     *
     * A negative outcome invalidates the initial upload transaction.
     */
    HIL_APPLICATION_RESPONSE_SCOPE_TICK = 2,
    /**
     * Automatic whole-test validation after all expected upload data arrives.
     *
     * ACCEPTED makes the retained test available for START. A negative outcome
     * invalidates the initial transaction.
     */
    HIL_APPLICATION_RESPONSE_SCOPE_COMPLETE_TEST = 3,
    /** Validation/performance of an Execution Control request. */
    HIL_APPLICATION_RESPONSE_SCOPE_EXECUTION_CONTROL = 4,
    /** Validation/performance of a test-independent Global Control request. */
    HIL_APPLICATION_RESPONSE_SCOPE_GLOBAL_CONTROL = 5,
    /** Reserved sentinel. */
    HIL_APPLICATION_RESPONSE_SCOPE_RESERVED = 255
} HIL_Application_Response_Scope_T;

/**
 * @brief High-level Application result for the referenced scope.
 *
 * @details ACCEPTED means understood, semantically validated, and accepted by
 * receiving Application integration. For a Tick Response, it specifically
 * means all fixed and declared variable data was received and validated and the
 * receiver has taken responsibility for retaining it so the sender may
 * continue. Retention may be in NAND, RAM, or an accepted storage-manager queue
 * according to integration policy; the protocol does not require a NAND write.
 *
 * For a Complete Test Response, ACCEPTED means all expected retained ticks and
 * variable data passed whole-test validation and the identified test is
 * available for a subsequent START request. COMPLETED means a requested control
 * operation was performed. REJECTED means a structurally valid input or request
 * was semantically unacceptable and was not accepted/performed. FAILED means
 * processing began but could not complete. Scope-specific transaction effects
 * are defined by the protocol contract, not by the codec.
 */
typedef enum
{
    /** Invalid sentinel. */
    HIL_APPLICATION_RESPONSE_OUTCOME_INVALID = 0,
    /** Input was validated and accepted, including required retention ownership. */
    HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED = 1,
    /** Input or request was understood but rejected. */
    HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED = 2,
    /** Requested control operation was actually performed. */
    HIL_APPLICATION_RESPONSE_OUTCOME_COMPLETED = 3,
    /** Processing began but could not complete. */
    HIL_APPLICATION_RESPONSE_OUTCOME_FAILED = 4,
    /** Reserved sentinel. */
    HIL_APPLICATION_RESPONSE_OUTCOME_RESERVED = 255
} HIL_Application_Response_Outcome_T;

/**
 * @brief Initial expandable reason associated with a response.
 *
 * @details This intentionally modest taxonomy is an Application protocol value,
 * not a local function status.
 */
typedef enum
{
    /** No additional reason is needed. */
    HIL_APPLICATION_RESPONSE_REASON_NONE = 0,
    /** Message feature/channel/operation is unsupported. */
    HIL_APPLICATION_RESPONSE_REASON_UNSUPPORTED = 1,
    /** A structurally valid operation cannot currently be performed. */
    HIL_APPLICATION_RESPONSE_REASON_OPERATION_NOT_ALLOWED = 2,
    /** Test ID does not match the active upload/test. */
    HIL_APPLICATION_RESPONSE_REASON_INCONSISTENT_TEST_ID = 3,
    /** Tick identity or range is invalid. */
    HIL_APPLICATION_RESPONSE_REASON_INVALID_TICK = 4,
    /** Fixed declaration and variable-data length disagree. */
    HIL_APPLICATION_RESPONSE_REASON_LENGTH_MISMATCH = 5,
    /** Firmware/host integration cannot take retention responsibility. */
    HIL_APPLICATION_RESPONSE_REASON_STORAGE_UNAVAILABLE = 6,
    /** Semantic or whole-test validation failed. */
    HIL_APPLICATION_RESPONSE_REASON_VALIDATION_FAILED = 7,
    /** Hardware is not healthy or ready for the requested operation. */
    HIL_APPLICATION_RESPONSE_REASON_HARDWARE_NOT_READY = 8,
    /** Unclassified Application processing failure. */
    HIL_APPLICATION_RESPONSE_REASON_INTERNAL_FAILURE = 9,
    /** Reserved sentinel. */
    HIL_APPLICATION_RESPONSE_REASON_RESERVED = 255
} HIL_Application_Response_Reason_T;

/**
 * @brief Body of one Application Response.
 *
 * @details The enclosing envelope contains a Test ID for every test-scoped
 * response and omits it for GLOBAL_CONTROL. tick_number is meaningful only for
 * TICK scope and must otherwise be zero. control_command is meaningful only for
 * EXECUTION_CONTROL scope, and global_control_command only for GLOBAL_CONTROL;
 * both must otherwise be INVALID.
 *
 * Responses do not create mandatory stop-and-wait. Multiple instruction/data
 * messages may be in flight subject to Transport flow control. Correlation uses
 * test ID when applicable, scope, tick number, and relevant control command—not
 * an Application sequence.
 *
 * A Response is semantic Application acceptance. It is distinct from a
 * Transport ACK, which confirms reliable byte/frame delivery but says nothing
 * about message semantics or retention responsibility.
 */
typedef struct
{
    /** Operation family whose result is reported. */
    HIL_Application_Response_Scope_T scope;
    /** Positive or negative Application outcome. */
    HIL_Application_Response_Outcome_T outcome;
    /** Expandable reason; normally NONE for successful outcomes. */
    HIL_Application_Response_Reason_T reason;
    /** Zero-based tick identity for TICK scope, otherwise zero. */
    uint32_t tick_number;
    /** Referenced command for EXECUTION_CONTROL scope, otherwise INVALID. */
    HIL_Application_Control_Command_T control_command;
    /** Referenced command for GLOBAL_CONTROL scope, otherwise INVALID. */
    HIL_Application_Global_Control_Command_T global_control_command;
    /** Integration-defined diagnostic detail; zero when unused. */
    uint32_t detail;
} HIL_Application_Response_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RESPONSE_H */
