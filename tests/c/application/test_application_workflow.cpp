#include <array>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"

namespace {
HIL_Application_Test_Id_T ExampleTestId( std::uint8_t discriminator )
{
    HIL_Application_Test_Id_T test_id{};
    test_id.bytes[0]  = discriminator;
    test_id.bytes[15] = static_cast<std::uint8_t>( discriminator ^ 0xa5u );
    return test_id;
}

HIL_Application_Message_T
TestResponse( const HIL_Application_Test_Id_T& test_id, HIL_Application_Response_Scope_T scope,
              HIL_Application_Response_Outcome_T outcome, HIL_Application_Response_Reason_T reason,
              std::uint32_t                     tick_number = 0u,
              HIL_Application_Control_Command_T command     = HIL_APPLICATION_CONTROL_INVALID )
{
    HIL_Application_Message_T response{};
    response.type                                 = HIL_APPLICATION_MESSAGE_TYPE_RESPONSE;
    response.subtype                              = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    response.has_test_id                          = 1u;
    response.test_id                              = test_id;
    response.body.response.scope                  = scope;
    response.body.response.outcome                = outcome;
    response.body.response.reason                 = reason;
    response.body.response.tick_number            = tick_number;
    response.body.response.control_command        = command;
    response.body.response.global_control_command = HIL_APPLICATION_GLOBAL_CONTROL_INVALID;
    return response;
}

void CompileCodecFacadeUsage()
{
    HIL_Application_Context_T context{};
    HIL_Application_Config_T  config{};

    std::array<std::uint8_t, 2048u> encoded_message{};
    std::array<std::uint8_t, 2048u> decode_storage{};
    std::size_t                     encoded_size            = 0u;
    std::size_t                     required_decode_storage = 0u;
    HIL_Application_Message_T       decoded{};

    ( void )HIL_APPLICATION_Default_Config( &config );
    config.max_encoded_message_size        = encoded_message.size();
    config.max_variable_data_size          = 512u;
    config.max_peripheral_config_count     = 16u;
    config.max_variable_transfers_per_tick = 8u;
    config.max_expected_tick_count         = 1000u;
    ( void )HIL_APPLICATION_Init( &context, &config );

    HIL_Application_Message_T configuration{};
    configuration.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    configuration.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    configuration.has_test_id = 1u;
    configuration.test_id     = ExampleTestId( 0x11u );
    configuration.body.test_configuration.tick_duration.nanoseconds = 1000000u;
    configuration.body.test_configuration.expected_tick_count       = 2u;

    ( void )HIL_APPLICATION_Validate_Message( &context, &configuration );
    ( void )HIL_APPLICATION_Encoded_Size( &context, &configuration, &encoded_size );
    ( void )HIL_APPLICATION_Encode_Message( &context, &configuration, nullptr, 0u, &encoded_size );
    ( void )HIL_APPLICATION_Encode_Message( &context, &configuration, encoded_message.data(),
                                            encoded_message.size(), &encoded_size );
    ( void )HIL_APPLICATION_Validate_Encoded_Message( &context, encoded_message.data(),
                                                      encoded_size, &required_decode_storage );
    ( void )HIL_APPLICATION_Decode_Storage_Size( &context, encoded_message.data(), encoded_size,
                                                 &required_decode_storage );
    ( void )HIL_APPLICATION_Decode_Message( &context, encoded_message.data(), encoded_size,
                                            decode_storage.data(), decode_storage.size(), &decoded,
                                            &required_decode_storage );

    /* Context remains codec-only; endpoint transaction data is never supplied. */
}

void CompileUploadConformanceScenarios()
{
    const HIL_Application_Test_Id_T    test_a = ExampleTestId( 0x21u );
    const HIL_Application_Test_Id_T    test_b = ExampleTestId( 0x22u );
    const std::array<std::uint8_t, 6u> uart_bytes{ 1u, 2u, 3u, 4u, 5u, 6u };

    HIL_Application_Message_T configuration{};
    configuration.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    configuration.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    configuration.has_test_id = 1u;
    configuration.test_id     = test_a;
    configuration.body.test_configuration.tick_duration.nanoseconds = 1000000u;
    configuration.body.test_configuration.expected_tick_count       = 2u;

    /* ACCEPTED creates upload A; REJECTED configuration B creates no transaction. */
    const HIL_Application_Message_T configuration_accepted = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TEST_CONFIGURATION,
        HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED, HIL_APPLICATION_RESPONSE_REASON_NONE );
    const HIL_Application_Message_T configuration_rejected =
        TestResponse( test_b, HIL_APPLICATION_RESPONSE_SCOPE_TEST_CONFIGURATION,
                      HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED,
                      HIL_APPLICATION_RESPONSE_REASON_HARDWARE_NOT_READY );

    HIL_Application_Data_Declaration_T declaration{};
    declaration.channel.peripheral = HIL_APPLICATION_PERIPHERAL_UART;
    declaration.channel.channel    = 0u;
    declaration.byte_length        = uart_bytes.size();

    HIL_Application_Message_T fixed_tick{};
    fixed_tick.type                                = HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION;
    fixed_tick.subtype                             = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    fixed_tick.has_test_id                         = 1u;
    fixed_tick.test_id                             = test_a;
    fixed_tick.body.test_instruction.tick_number   = 0u;
    fixed_tick.body.test_instruction.variable_data = &declaration;
    fixed_tick.body.test_instruction.variable_data_count = 1u;

    HIL_Application_Message_T variable_tick{};
    variable_tick.type        = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA;
    variable_tick.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    variable_tick.has_test_id = 1u;
    variable_tick.test_id     = test_a;
    variable_tick.body.variable_instruction_data.tick_number = 0u;
    variable_tick.body.variable_instruction_data.channel     = declaration.channel;
    variable_tick.body.variable_instruction_data.data =
        HIL_Application_Byte_Span_T{ uart_bytes.data(), uart_bytes.size() };

    /* Tick ACCEPTED represents the complete fixed-plus-variable acceptance. */
    const HIL_Application_Message_T tick_accepted = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TICK, HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED,
        HIL_APPLICATION_RESPONSE_REASON_NONE, 0u );

    HIL_Application_Message_T fixed_tick_1                 = fixed_tick;
    fixed_tick_1.body.test_instruction.tick_number         = 1u;
    fixed_tick_1.body.test_instruction.variable_data       = nullptr;
    fixed_tick_1.body.test_instruction.variable_data_count = 0u;
    const HIL_Application_Message_T tick_1_accepted        = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TICK, HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED,
        HIL_APPLICATION_RESPONSE_REASON_NONE, 1u );

    HIL_Application_Message_T out_of_order_tick         = fixed_tick;
    out_of_order_tick.body.test_instruction.tick_number = 2u;
    const HIL_Application_Message_T tick_rejected       = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TICK, HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED,
        HIL_APPLICATION_RESPONSE_REASON_INVALID_TICK, 2u );

    HIL_Application_Message_T wrong_test_id_tick           = fixed_tick;
    wrong_test_id_tick.test_id                             = test_b;
    const HIL_Application_Message_T wrong_test_id_rejected = TestResponse(
        test_b, HIL_APPLICATION_RESPONSE_SCOPE_TICK, HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED,
        HIL_APPLICATION_RESPONSE_REASON_INCONSISTENT_TEST_ID, 0u );

    const HIL_Application_Message_T complete_test_accepted = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_COMPLETE_TEST,
        HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED, HIL_APPLICATION_RESPONSE_REASON_NONE );
    const HIL_Application_Message_T complete_test_rejected =
        TestResponse( test_a, HIL_APPLICATION_RESPONSE_SCOPE_COMPLETE_TEST,
                      HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED,
                      HIL_APPLICATION_RESPONSE_REASON_VALIDATION_FAILED );

    /* Negative Tick/Complete Test outcomes invalidate the upload in integration. */
    ( void )configuration;
    ( void )configuration_accepted;
    ( void )configuration_rejected;
    ( void )fixed_tick;
    ( void )variable_tick;
    ( void )tick_accepted;
    ( void )fixed_tick_1;
    ( void )tick_1_accepted;
    ( void )out_of_order_tick;
    ( void )tick_rejected;
    ( void )wrong_test_id_tick;
    ( void )wrong_test_id_rejected;
    ( void )complete_test_accepted;
    ( void )complete_test_rejected;
}

void CompileControlConformanceScenarios()
{
    const HIL_Application_Test_Id_T test_a = ExampleTestId( 0x31u );
    const HIL_Application_Test_Id_T test_b = ExampleTestId( 0x32u );

    HIL_Application_Message_T start{};
    start.type                           = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    start.subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    start.has_test_id                    = 1u;
    start.test_id                        = test_a;
    start.body.execution_control.command = HIL_APPLICATION_CONTROL_START;

    const HIL_Application_Message_T start_completed =
        TestResponse( test_a, HIL_APPLICATION_RESPONSE_SCOPE_EXECUTION_CONTROL,
                      HIL_APPLICATION_RESPONSE_OUTCOME_COMPLETED,
                      HIL_APPLICATION_RESPONSE_REASON_NONE, 0u, HIL_APPLICATION_CONTROL_START );

    HIL_Application_Message_T premature_start      = start;
    premature_start.test_id                        = test_b;
    const HIL_Application_Message_T start_rejected = TestResponse(
        test_b, HIL_APPLICATION_RESPONSE_SCOPE_EXECUTION_CONTROL,
        HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED,
        HIL_APPLICATION_RESPONSE_REASON_OPERATION_NOT_ALLOWED, 0u, HIL_APPLICATION_CONTROL_START );

    HIL_Application_Message_T abort      = start;
    abort.body.execution_control.command = HIL_APPLICATION_CONTROL_ABORT;
    const HIL_Application_Message_T abort_completed =
        TestResponse( test_a, HIL_APPLICATION_RESPONSE_SCOPE_EXECUTION_CONTROL,
                      HIL_APPLICATION_RESPONSE_OUTCOME_COMPLETED,
                      HIL_APPLICATION_RESPONSE_REASON_NONE, 0u, HIL_APPLICATION_CONTROL_ABORT );

    /* Firmware integration and its execution manager decide these outcomes. */
    ( void )start;
    ( void )start_completed;
    ( void )premature_start;
    ( void )start_rejected;
    ( void )abort;
    ( void )abort_completed;
}

void CompileResultConformanceScenario()
{
    const HIL_Application_Test_Id_T    test_a = ExampleTestId( 0x41u );
    const std::array<std::uint8_t, 4u> can_bytes{ 0x10u, 0x20u, 0x30u, 0x40u };

    HIL_Application_Data_Declaration_T result_declaration{};
    result_declaration.channel.peripheral = HIL_APPLICATION_PERIPHERAL_CAN;
    result_declaration.channel.channel    = 0u;
    result_declaration.byte_length        = can_bytes.size();

    HIL_Application_Message_T fixed_result_0{};
    fixed_result_0.type                                 = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    fixed_result_0.subtype                              = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    fixed_result_0.has_test_id                          = 1u;
    fixed_result_0.test_id                              = test_a;
    fixed_result_0.body.test_result.tick_number         = 0u;
    fixed_result_0.body.test_result.variable_data       = &result_declaration;
    fixed_result_0.body.test_result.variable_data_count = 1u;
    fixed_result_0.body.test_result.condition           = HIL_APPLICATION_RESULT_CONDITION_OK;

    HIL_Application_Message_T variable_result_0{};
    variable_result_0.type        = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA;
    variable_result_0.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    variable_result_0.has_test_id = 1u;
    variable_result_0.test_id     = test_a;
    variable_result_0.body.variable_result_data.tick_number = 0u;
    variable_result_0.body.variable_result_data.channel     = result_declaration.channel;
    variable_result_0.body.variable_result_data.data =
        HIL_Application_Byte_Span_T{ can_bytes.data(), can_bytes.size() };

    HIL_Application_Message_T fixed_result_1            = fixed_result_0;
    fixed_result_1.body.test_result.tick_number         = 1u;
    fixed_result_1.body.test_result.variable_data       = nullptr;
    fixed_result_1.body.test_result.variable_data_count = 0u;

    /* For N=2, host completion requires both fixed results and declared CAN data. */
    const std::array<HIL_Application_Message_T, 3u> complete_result_set{
        fixed_result_0,
        variable_result_0,
        fixed_result_1,
    };
    ( void )complete_result_set;
}

void CompileRecoveryConformanceScenarios()
{
    const HIL_Application_Test_Id_T invalidated_test = ExampleTestId( 0x51u );
    const HIL_Application_Test_Id_T restarted_test   = ExampleTestId( 0x52u );

    HIL_Application_Message_T reset{};
    reset.type                        = HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL;
    reset.subtype                     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    reset.has_test_id                 = 0u;
    reset.body.global_control.command = HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION;

    HIL_Application_Message_T reset_completed{};
    reset_completed.type                          = HIL_APPLICATION_MESSAGE_TYPE_RESPONSE;
    reset_completed.subtype                       = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    reset_completed.has_test_id                   = 0u;
    reset_completed.body.response.scope           = HIL_APPLICATION_RESPONSE_SCOPE_GLOBAL_CONTROL;
    reset_completed.body.response.outcome         = HIL_APPLICATION_RESPONSE_OUTCOME_COMPLETED;
    reset_completed.body.response.reason          = HIL_APPLICATION_RESPONSE_REASON_NONE;
    reset_completed.body.response.control_command = HIL_APPLICATION_CONTROL_INVALID;
    reset_completed.body.response.global_control_command =
        HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION;

    /* Session loss has no Application message; integration invalidates the upload. */
    const HIL_Application_Message_T in_flight_rejected =
        TestResponse( invalidated_test, HIL_APPLICATION_RESPONSE_SCOPE_TICK,
                      HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED,
                      HIL_APPLICATION_RESPONSE_REASON_INCONSISTENT_TEST_ID, 1u );

    HIL_Application_Message_T restarted_configuration{};
    restarted_configuration.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    restarted_configuration.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    restarted_configuration.has_test_id = 1u;
    restarted_configuration.test_id     = restarted_test;
    restarted_configuration.body.test_configuration.tick_duration.nanoseconds = 1000000u;
    restarted_configuration.body.test_configuration.expected_tick_count       = 2u;

    ( void )reset;
    ( void )reset_completed;
    ( void )in_flight_rejected;
    ( void )restarted_configuration;
}
}  // namespace

TEST( ApplicationFacadeApiDesign, IntentionalStubsRemainExplicit )
{
    HIL_Application_Context_T context{};
    HIL_Application_Config_T  config{};
    EXPECT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
}

TEST( ApplicationFacadeApiDesign, DocumentedTransactionScenariosCompile )
{
    const std::array<void ( * )(), 5u> scenarios{
        &CompileCodecFacadeUsage,
        &CompileUploadConformanceScenarios,
        &CompileControlConformanceScenarios,
        &CompileResultConformanceScenario,
        &CompileRecoveryConformanceScenarios,
    };

    for ( const auto scenario : scenarios )
    {
        EXPECT_NE( scenario, nullptr );
    }
}
