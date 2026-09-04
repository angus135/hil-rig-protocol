/**
 * @file application_validation.h
 * @brief Private declarations for Application typed-body structural validation.
 *
 * @details These helpers validate the currently implemented structural rules for
 * one typed body. Common envelope rules, including type/subtype and Test-ID
 * presence, are handled separately by HIL_APPLICATION_Validate_Common_Message_Fields().
 * Detailed configuration, fixed-I/O, transaction, and hardware semantics remain
 * outside this foundation and must not be inferred from these helpers.
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
 * @brief Validate the implemented Test Configuration foundation rules.
 * @details Checks the supported tick duration, configured expected-tick limit,
 * and extension byte-span structure. Detailed fixed-I/O and reserved-field
 * semantics remain deliberately deferred.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed Test Configuration body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Test_Configuration_validate( const HIL_Application_Context_T*            context,
                                             const HIL_Application_Test_Configuration_T* data );

/**
 * @brief Validate the currently implemented fixed Test Instruction rules.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed fixed instruction body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Test_Instructions_validate( const HIL_Application_Context_T*          context,
                                            const HIL_Application_Test_Instruction_T* data );

/**
 * @brief Preserve the reserved Variable Instruction Data validation entry point.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed variable instruction body.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Variable instruction validation is deferred.
 */
HIL_Application_Status_T HIL_APPLICATION_Variable_Instruction_Data_validate(
    const HIL_Application_Context_T*                   context,
    const HIL_Application_Variable_Instruction_Data_T* data );

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

/**
 * @brief Validate the currently implemented fixed Test Result rules.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed fixed result body.
 * @return Application status.
 */
HIL_Application_Status_T
HIL_APPLICATION_Test_Result_validate( const HIL_Application_Context_T*     context,
                                      const HIL_Application_Test_Result_T* data );

/**
 * @brief Preserve the reserved Variable Result Data validation entry point.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed variable result body.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Variable result validation is deferred.
 */
HIL_Application_Status_T
HIL_APPLICATION_Variable_Result_Data_validate( const HIL_Application_Context_T* context,
                                               const HIL_Application_Variable_Result_Data_T* data );

/**
 * @brief Preserve the reserved Application Response validation entry point.
 * @param[in] context Initialized Application context.
 * @param[in] data    Typed response body.
 * @retval HIL_APPLICATION_STATUS_NOT_IMPLEMENTED Response semantics are deferred.
 */
HIL_Application_Status_T
HIL_APPLICATION_Response_validate( const HIL_Application_Context_T*  context,
                                   const HIL_Application_Response_T* data );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_APPLICATION_VALIDATION_INTERNAL_H */
