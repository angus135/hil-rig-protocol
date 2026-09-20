/**
 * @file application_validation.h
 * @brief Private declarations for Application typed-body structural validation.
 *
 * @details These helpers validate the currently implemented structural rules for
 * one typed body. Common envelope rules, including type/subtype and Test-ID
 * presence, are handled separately by HIL_APPLICATION_Validate_Common_Message_Fields().
 * Test Configuration structural rules are implemented here. Stateful transaction,
 * hardware-capability, and deferred-family semantics remain outside these helpers.
 */
#ifndef HIL_RIG_PROTOCOL_APPLICATION_VALIDATION_INTERNAL_H
#define HIL_RIG_PROTOCOL_APPLICATION_VALIDATION_INTERNAL_H

#include "hil_rig_protocol/application/application_instruction.h"
#include "hil_rig_protocol/application/application_message.h"
#include "hil_rig_protocol/application/application_response.h"
#include "hil_rig_protocol/application/application_result.h"
#include "hil_rig_protocol/application/application_status.h"
#include "hil_rig_protocol/application/application_system_info.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/application/application_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Validate the currently implemented System Information Request rules.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed request body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_System_Info_Request_validate( const HIL_Application_Context_T*             context,
                                              const HIL_Application_System_Info_Request_T* data );

/**
 * @brief Validate the currently implemented System Information Response rules.
 * @details The typed repository protocol version must equal the compiled
 * repository version, and both byte spans must satisfy the public span contract.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed response body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_System_Info_Response_validate( const HIL_Application_Context_T* context,
                                               const HIL_Application_System_Info_Response_T* data );

/**
 * @brief Validate Test Configuration structural protocol rules.
 * @details Checks global fields, the extension span, fixed-array Booleans and
 * enums, canonical disabled records, PWM constraints, communication rates and
 * UART directions, and canonical disabled I2C configuration. Hardware
 * capability and workflow state remain integration-owned.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed Test Configuration body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Test_Configuration_validate( const HIL_Application_Context_T*            context,
                                             const HIL_Application_Test_Configuration_T* data );

/**
 * @brief Validate fixed Test Instruction structural value rules.
 * @details Enforces the configured tick ceiling, Boolean Digital Outputs, and
 * PWM duty/zero-period rules without active-Test-Configuration or hardware state.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed fixed instruction body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Test_Instructions_validate( const HIL_Application_Context_T*          context,
                                            const HIL_Application_Test_Instruction_T* data );

/**
 * @brief Validate an Update Instruction body.
 * @details Enforces tick ceiling, flags, operation count, unique (peripheral, channel)
 * pairs, and per-peripheral payload bounds.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed update instruction body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Update_Instruction_validate( const HIL_Application_Context_T*            context,
                                             const HIL_Application_Update_Instruction_T* data );

/** Validate one encoded or typed logical operation payload. */
HIL_Application_Status_T HIL_APPLICATION_Logical_Operation_Fields_validate(
    const HIL_Application_Context_T* context, HIL_Application_Peripheral_Type_T peripheral_type,
    uint8_t channel, const HIL_Application_Byte_Span_T* payload );

/** Validate one encoded or typed captured-record payload. */
HIL_Application_Status_T HIL_APPLICATION_Captured_Record_Fields_validate(
    const HIL_Application_Context_T* context, HIL_Application_Peripheral_Type_T peripheral_type,
    uint8_t channel, const HIL_Application_Byte_Span_T* data );

/** Mark a fixed-state peripheral/channel pair and reject duplicates. */
HIL_Application_Status_T
HIL_APPLICATION_Record_Pair_Mark( uint16_t* seen_peripheral_channels, size_t seen_count,
                                  HIL_Application_Peripheral_Type_T peripheral_type,
                                  uint8_t                           channel );

/**
 * @brief Validate an Execution Control body.
 * @details Only START/ABORT commands are structurally accepted and reserved
 * flags must be zero.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed Execution Control body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Execution_Control_validate( const HIL_Application_Context_T*           context,
                                            const HIL_Application_Execution_Control_T* data );

/**
 * @brief Validate a Global Control body.
 * @details RESET_APPLICATION is the only current command and reserved flags
 * must be zero.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed Global Control body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Global_Control_validate( const HIL_Application_Context_T*        context,
                                         const HIL_Application_Global_Control_T* data );

/** Validate a Finalize Test Upload body; v0.3.0 flags must be zero. */
HIL_Application_Status_T
HIL_APPLICATION_Finalize_Test_Upload_validate( const HIL_Application_Context_T* context,
                                               const HIL_Application_Finalize_Test_Upload_T* data );

/**
 * @brief Validate fixed Test Result structural value rules.
 * @details Enforces the configured tick ceiling, Boolean Digital Inputs, PWM
 * duty/zero-period rules, and the three defined result conditions. Analogue
 * values and problem_detail are intentionally not range-limited here.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed fixed result body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Test_Result_validate( const HIL_Application_Context_T*     context,
                                      const HIL_Application_Test_Result_T* data );

/**
 * @brief Validate a Variable Test Result body.
 * @details Enforces tick ceiling, condition, problem_detail, flags, unique
 * (peripheral, channel) pairs, and per-peripheral payload bounds.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed variable test result body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Variable_Test_Result_validate( const HIL_Application_Context_T* context,
                                               const HIL_Application_Variable_Test_Result_T* data );

/**
 * @brief Validate the structural fields of an Application Response body.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed response body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Response_validate( const HIL_Application_Context_T*  context,
                                   const HIL_Application_Response_T* data );

/** Validate the structural fields of an Application Error body. */
HIL_Application_Status_T HIL_APPLICATION_Error_validate( const HIL_Application_Context_T* context,
                                                         const HIL_Application_Error_T*   data );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_VALIDATION_INTERNAL_H */
