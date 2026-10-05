/**
 * @file application_control.h
 * @brief Python-to-firmware execution and global-control message bodies.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_CONTROL_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Requested operation for one identified test.
 *
 * @details Decoding a value never performs the operation. Firmware Application
 * integration validates the transaction prerequisite, asks the execution
 * manager whether the operation is allowed, performs any permitted action, and
 * creates an Application Response reporting the actual outcome. Firmware owns
 * every internal state and transition. The enclosing Execution Control message
 * always requires a Test ID.
 *
 * START requires a successfully accepted complete test. ABORT requests safe
 * termination or abandonment of the identified active transaction or
 * operation. START remains invalid until the host receives an accepted
 * Complete Test Response for FINALIZE_TEST_UPLOAD.
 *
 * @warning Assigned values may become wire identifiers. Changing them can
 * break compatibility after wire-format approval.
 */
typedef enum
{
    /** Invalid sentinel. */
    HIL_APPLICATION_CONTROL_INVALID = 0,
    /**
     * Request execution of a previously accepted complete test.
     *
     * For the matching armed test, admission establishes one attempt owing a
     * terminal RUN_REPORT, even if preparation fails before execution. A
     * terminal rejection of that test also owes a REJECTED report after its
     * Response. Invalid, wrong-ID, and premature requests owe no report; a
     * duplicate request owes no additional report.
     */
    HIL_APPLICATION_CONTROL_START = 1,
    /**
     * Request safe termination/abandonment of the identified transaction.
     *
     * A COMPLETED Response means the previous transaction cannot continue
     * normally. A later upload must restart from Test Configuration unless a
     * future protocol version defines resumption.
     * A wrong Test ID is rejected before acting. For an admitted attempt, any
     * remaining results and its terminal report precede ABORT COMPLETED.
     */
    HIL_APPLICATION_CONTROL_ABORT = 2,
    /** Reserved sentinel. */
    HIL_APPLICATION_CONTROL_RESERVED = 255
} HIL_Application_Control_Command_T;

/** Execution Control message body. */
typedef struct
{
    /** Requested operation; never automatically executed by the codec. */
    HIL_Application_Control_Command_T command;
    /**
     * Reserved command option bits.
     *
     * Must be zero in the initial protocol. Nonzero values are structurally
     * unsupported until exact options are defined by a future version.
     */
    uint32_t flags;
} HIL_Application_Execution_Control_T;

/**
 * @brief Test-independent operation affecting the receiving Application.
 *
 * @details A Global Control message never carries a Test ID. Decoding and
 * structurally validating a command does not execute it. Receiving integration
 * owns recovery, transaction data, retained tests, firmware state, storage, and
 * the resulting Response.
 */
typedef enum
{
    /** Invalid sentinel. */
    HIL_APPLICATION_GLOBAL_CONTROL_INVALID = 0,
    /**
     * Clear active Application transaction data and recoverable conditions.
     *
     * COMPLETED means cleanup finished and the rig is ready for a new test.
     * During execution, reject with OPERATION_NOT_ALLOWED; during a pending
     * transition, reject with HARDWARE_NOT_READY. Preserve any owed terminal
     * report before destructive cleanup; enqueue it before RESET COMPLETED,
     * then notify readiness. Transport remains connected and initialized.
     */
    HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION = 1,
    /**
     * Read readiness without changing endpoint state, after compatible discovery.
     * Success produces one RIG_STATUS with QUERY_RESPONSE origin, with no extra
     * success Response. Failure uses a Global Control Response echoing GET_STATUS.
     */
    HIL_APPLICATION_GLOBAL_CONTROL_GET_STATUS = 2,
    /** Reserved sentinel. */
    HIL_APPLICATION_GLOBAL_CONTROL_RESERVED = 255
} HIL_Application_Global_Control_Command_T;

/** Application-scoped Global Control body; the envelope forbids a Test ID. */
typedef struct
{
    /** Requested global operation; never automatically executed by the codec. */
    HIL_Application_Global_Control_Command_T command;
    /**
     * Reserved option bits; must be zero in the initial protocol.
     * Nonzero values are structurally unsupported.
     */
    uint32_t flags;
} HIL_Application_Global_Control_T;

/**
 * @brief Host request declaring that a test upload is complete.
 *
 * @details Firmware validates the retained upload and reports the outcome
 * using the existing COMPLETE_TEST Response scope. flags is reserved and
 * must be zero in v0.3.0.
 */
typedef struct
{
    /** Reserved option bits; must be zero in v0.3.0. */
    uint32_t flags;
} HIL_Application_Finalize_Test_Upload_T;

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_CONTROL_H */
