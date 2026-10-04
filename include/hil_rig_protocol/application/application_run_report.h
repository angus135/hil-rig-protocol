/**
 * @file application_run_report.h
 * @brief Terminal outcome and diagnostic measurements for one admitted run.
 * @details Schema 1 has 177 + E payload bytes and 200 + E complete bytes,
 * E=0..255. Native struct sizes/alignments are not wire sizes. Only extensions
 * require caller decode storage. One report follows the final result and closes
 * the stream; Errors do not replace it. Endpoint ordering remains caller-owned.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RUN_REPORT_H
#define HIL_RIG_PROTOCOL_APPLICATION_APPLICATION_RUN_REPORT_H

#include "hil_rig_protocol/application/application_failure.h"
#include "hil_rig_protocol/application/application_types.h"

/** Stable schema-1 RunOutcome assignments. */
typedef enum
{
    HIL_APPLICATION_RUN_OUTCOME_INVALID = 0x0,
    HIL_APPLICATION_RUN_OUTCOME_SUCCESS = 0x1,
    HIL_APPLICATION_RUN_OUTCOME_FAILED  = 0x2,
    HIL_APPLICATION_RUN_OUTCOME_ABORTED = 0x3,
} HIL_Application_Run_Outcome_T;

/** Stable schema-1 ExecutionOutcome assignments. */
typedef enum
{
    HIL_APPLICATION_EXECUTION_OUTCOME_INVALID     = 0x0,
    HIL_APPLICATION_EXECUTION_OUTCOME_NOT_STARTED = 0x1,
    HIL_APPLICATION_EXECUTION_OUTCOME_COMPLETE    = 0x2,
    HIL_APPLICATION_EXECUTION_OUTCOME_FAILED      = 0x3,
    HIL_APPLICATION_EXECUTION_OUTCOME_ABORTED     = 0x4,
} HIL_Application_Execution_Outcome_T;

/** Stable schema-1 RunResultStatus assignments. */
typedef enum
{
    HIL_APPLICATION_RUN_RESULT_STATUS_INVALID     = 0x0,
    HIL_APPLICATION_RUN_RESULT_STATUS_COMPLETE    = 0x1,
    HIL_APPLICATION_RUN_RESULT_STATUS_PARTIAL     = 0x2,
    HIL_APPLICATION_RUN_RESULT_STATUS_UNAVAILABLE = 0x3,
} HIL_Application_Run_Result_Status_T;

/** Stable schema-1 RunReportSection assignments. */
typedef enum
{
    HIL_APPLICATION_RUN_REPORT_VALID_TERMINAL                = 0x1,
    HIL_APPLICATION_RUN_REPORT_VALID_LAST_COMPLETED_BOUNDARY = 0x2,
    HIL_APPLICATION_RUN_REPORT_VALID_ISR_TIMING              = 0x4,
    HIL_APPLICATION_RUN_REPORT_VALID_INSTRUCTION_BUFFER      = 0x8,
    HIL_APPLICATION_RUN_REPORT_VALID_RESULT_BUFFER           = 0x10,
    HIL_APPLICATION_RUN_REPORT_VALID_FLASH_THROUGHPUT        = 0x20,
} HIL_Application_Run_Report_Section_T;

/** Executed-boundary IRQ work before the context-switch request; not interrupt latency.
 * All boundary fields use 0..N; invalid sections must contain zero fields.
 */
typedef struct
{
    uint32_t sample_count;
    uint64_t total_cycles;
    uint32_t minimum_cycles;
    uint32_t maximum_cycles;
    uint32_t maximum_boundary;
} HIL_Application_Run_Isr_Timing_T;

/** Unread instruction minima sampled while unread instructions remain.
 * All boundary fields use 0..N; invalid sections must contain zero fields.
 */
typedef struct
{
    uint32_t sample_count;
    uint32_t minimum_unread_bytes;
    uint32_t minimum_boundary;
} HIL_Application_Run_Instruction_Buffer_T;

/** Committed record bytes and pending NAND-drain pressure; records are not ticks.
 * All boundary fields use 0..N; invalid sections must contain zero fields.
 */
typedef struct
{
    uint32_t committed_record_count;
    uint32_t committed_bytes;
    uint32_t peak_pending_bytes;
    uint32_t peak_pending_boundary;
    uint32_t reserve_failure_count;
    uint32_t commit_failure_count;
} HIL_Application_Run_Result_Buffer_T;

/** Execution-phase flash snapshot, before finalisation drains; service components overlap.
 * All boundary fields use 0..N; invalid sections must contain zero fields.
 */
typedef struct
{
    uint32_t result_pages_drained;
    uint64_t result_bytes_drained;
    uint64_t result_drain_total_cycles;
    uint32_t result_drain_maximum_cycles;
    uint32_t instruction_pages_refilled;
    uint64_t instruction_bytes_refilled;
    uint64_t instruction_refill_total_cycles;
    uint32_t instruction_refill_maximum_cycles;
    uint32_t instruction_publish_sample_count;
    uint64_t instruction_publish_total_cycles;
    uint32_t instruction_publish_maximum_cycles;
    uint32_t service_gap_sample_count;
    uint64_t service_gap_total_cycles;
    uint32_t service_gap_maximum_cycles;
    uint32_t refill_drain_contention_count;
} HIL_Application_Run_Flash_Statistics_T;

/** Terminal payload for the envelope Test ID, taken from the admitted generation. */
typedef struct
{
    /** Exactly 1. */
    uint16_t                            schema_version;
    HIL_Application_Run_Outcome_T       run_outcome;
    HIL_Application_Execution_Outcome_T execution_outcome;
    HIL_Application_Run_Result_Status_T result_status;
    HIL_Application_Failure_Source_T    failure_source;
    HIL_Application_Failure_Stage_T     failure_stage;
    HIL_Application_Failure_Reason_T    failure_reason;
    /** TERMINAL is mandatory; absent measurement bits require canonical zero fields. */
    uint32_t valid_sections;
    /** Requested intervals N, 1..1000000; execution boundaries are 0..N. */
    uint32_t expected_tick_count;
    /** Nominal configured period, from the supported period set. */
    uint32_t tick_period_us;
    /** Last successful boundary, meaningful only when its validity bit is set. */
    uint32_t last_completed_boundary;
    /** Complete logical ticks accepted by output, 0..N; not host receipt. */
    uint32_t                                 result_ticks_emitted;
    HIL_Application_Run_Isr_Timing_T         isr_timing;
    HIL_Application_Run_Instruction_Buffer_T instruction_buffer;
    HIL_Application_Run_Result_Buffer_T      result_buffer;
    HIL_Application_Run_Flash_Statistics_T   flash;
    /** Opaque extension, 0..255 bytes; borrowed for synchronous encode only. */
    HIL_Application_Byte_Span_T extension_data;
} HIL_Application_Run_Report_T;

#endif
