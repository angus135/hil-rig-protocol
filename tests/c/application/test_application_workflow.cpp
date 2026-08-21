#include <array>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cstring>
#include <type_traits>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"

namespace {
struct alignas( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT ) AlignedDecodeStorageBuffer
{
    std::array<std::uint8_t, 2048u> bytes{};
};

static_assert( alignof( AlignedDecodeStorageBuffer ) >= HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT );

HIL_Application_Test_Id_T ExampleTestId( std::uint8_t discriminator )
{
    HIL_Application_Test_Id_T test_id{};
    test_id.bytes[0]  = discriminator;
    test_id.bytes[15] = static_cast<std::uint8_t>( discriminator ^ 0xa5u );
    return test_id;
}

HIL_Application_Context_T MakeContext()
{
    HIL_Application_Context_T context{};
    HIL_Application_Config_T  config{};

    EXPECT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );

    return context;
}

void ExpectByteSpanEqual( const HIL_Application_Byte_Span_T& expected,
                          const HIL_Application_Byte_Span_T& actual )
{
    ASSERT_EQ( expected.size, actual.size );

    if ( expected.size == 0u )
    {
        return;
    }

    ASSERT_NE( expected.data, nullptr );
    ASSERT_NE( actual.data, nullptr );

    EXPECT_EQ( std::memcmp( expected.data, actual.data, expected.size ), 0 );
}

void ExpectChannelEqual( const HIL_Application_Channel_Id_T& expected,
                         const HIL_Application_Channel_Id_T& actual )
{
    EXPECT_EQ( expected.peripheral, actual.peripheral );
    EXPECT_EQ( expected.channel, actual.channel );
}

void ExpectDataDeclarationsEqual( const HIL_Application_Data_Declaration_T* expected,
                                  std::size_t                               expected_count,
                                  const HIL_Application_Data_Declaration_T* actual,
                                  std::size_t                               actual_count )
{
    ASSERT_EQ( expected_count, actual_count );

    for ( std::size_t i = 0u; i < expected_count; ++i )
    {
        ExpectChannelEqual( expected[i].channel, actual[i].channel );
        ExpectByteSpanEqual( expected[i].data, actual[i].data );
    }
}

void ExpectTestIdEqual( const HIL_Application_Test_Id_T& expected,
                        const HIL_Application_Test_Id_T& actual )
{
    EXPECT_EQ( std::memcmp( expected.bytes, actual.bytes, HIL_APPLICATION_TEST_ID_SIZE ), 0 );
}

/*
 * Compare the semantic contents of two Application messages.
 *
 * We deliberately do not use memcmp(&expected, &actual, sizeof(...))
 * because the message contains pointers to variable data.
 */
void ExpectMessagesEqual( const HIL_Application_Message_T& expected,
                          const HIL_Application_Message_T& actual )
{
    EXPECT_EQ( expected.type, actual.type );
    EXPECT_EQ( expected.subtype, actual.subtype );
    EXPECT_EQ( expected.has_test_id, actual.has_test_id );

    if ( expected.has_test_id != 0u )
    {
        ExpectTestIdEqual( expected.test_id, actual.test_id );
    }

    switch ( expected.type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST:
            EXPECT_EQ( expected.body.system_info_request.query,
                       actual.body.system_info_request.query );
            EXPECT_EQ( expected.body.system_info_request.request_firmware_git_hash,
                       actual.body.system_info_request.request_firmware_git_hash );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE:
            EXPECT_EQ( expected.body.system_info_response.application_protocol_major,
                       actual.body.system_info_response.application_protocol_major );
            EXPECT_EQ( expected.body.system_info_response.application_protocol_minor,
                       actual.body.system_info_response.application_protocol_minor );
            EXPECT_EQ( expected.body.system_info_response.firmware_version_major,
                       actual.body.system_info_response.firmware_version_major );
            EXPECT_EQ( expected.body.system_info_response.firmware_version_minor,
                       actual.body.system_info_response.firmware_version_minor );
            ExpectByteSpanEqual( expected.body.system_info_response.firmware_git_hash,
                                 actual.body.system_info_response.firmware_git_hash );
            ExpectByteSpanEqual( expected.body.system_info_response.diagnostic_data,
                                 actual.body.system_info_response.diagnostic_data );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION:
            EXPECT_EQ( expected.body.test_configuration.tick_duration.nanoseconds,
                       actual.body.test_configuration.tick_duration.nanoseconds );
            EXPECT_EQ( expected.body.test_configuration.expected_tick_count,
                       actual.body.test_configuration.expected_tick_count );
            EXPECT_EQ( expected.body.test_configuration.flags,
                       actual.body.test_configuration.flags );
            EXPECT_EQ( expected.body.test_configuration.peripheral_count,
                       actual.body.test_configuration.peripheral_count );

            for ( std::size_t i = 0u; i < expected.body.test_configuration.peripheral_count; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.peripherals[i].type,
                           actual.body.test_configuration.peripherals[i].type );

                EXPECT_EQ(
                    std::memcmp( &expected.body.test_configuration.peripherals[i].value,
                                 &actual.body.test_configuration.peripherals[i].value,
                                 sizeof( expected.body.test_configuration.peripherals[i].value ) ),
                    0 );
            }

            ExpectByteSpanEqual( expected.body.test_configuration.extension_data,
                                 actual.body.test_configuration.extension_data );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
            EXPECT_EQ( expected.body.test_instruction.tick_number,
                       actual.body.test_instruction.tick_number );

            EXPECT_EQ( std::memcmp( expected.body.test_instruction.digital_outputs,
                                    actual.body.test_instruction.digital_outputs,
                                    sizeof( expected.body.test_instruction.digital_outputs ) ),
                       0 );

            EXPECT_EQ( std::memcmp( expected.body.test_instruction.analog_outputs,
                                    actual.body.test_instruction.analog_outputs,
                                    sizeof( expected.body.test_instruction.analog_outputs ) ),
                       0 );

            EXPECT_EQ( std::memcmp( expected.body.test_instruction.pwm_outputs,
                                    actual.body.test_instruction.pwm_outputs,
                                    sizeof( expected.body.test_instruction.pwm_outputs ) ),
                       0 );

            ExpectDataDeclarationsEqual( expected.body.test_instruction.variable_data,
                                         expected.body.test_instruction.variable_data_count,
                                         actual.body.test_instruction.variable_data,
                                         actual.body.test_instruction.variable_data_count );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA:
            EXPECT_EQ( expected.body.variable_instruction_data.tick_number,
                       actual.body.variable_instruction_data.tick_number );

            ExpectChannelEqual( expected.body.variable_instruction_data.channel,
                                actual.body.variable_instruction_data.channel );

            ExpectByteSpanEqual( expected.body.variable_instruction_data.data,
                                 actual.body.variable_instruction_data.data );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL:
            EXPECT_EQ( expected.body.execution_control.command,
                       actual.body.execution_control.command );
            EXPECT_EQ( expected.body.execution_control.flags, actual.body.execution_control.flags );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL:
            EXPECT_EQ( expected.body.global_control.command, actual.body.global_control.command );
            EXPECT_EQ( expected.body.global_control.flags, actual.body.global_control.flags );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT:
            EXPECT_EQ( expected.body.test_result.tick_number, actual.body.test_result.tick_number );

            EXPECT_EQ( std::memcmp( expected.body.test_result.digital_inputs,
                                    actual.body.test_result.digital_inputs,
                                    sizeof( expected.body.test_result.digital_inputs ) ),
                       0 );

            EXPECT_EQ( std::memcmp( expected.body.test_result.analog_inputs,
                                    actual.body.test_result.analog_inputs,
                                    sizeof( expected.body.test_result.analog_inputs ) ),
                       0 );

            EXPECT_EQ( std::memcmp( expected.body.test_result.pwm_inputs,
                                    actual.body.test_result.pwm_inputs,
                                    sizeof( expected.body.test_result.pwm_inputs ) ),
                       0 );

            EXPECT_EQ( expected.body.test_result.condition, actual.body.test_result.condition );

            ExpectDataDeclarationsEqual( expected.body.test_result.variable_data,
                                         expected.body.test_result.variable_data_count,
                                         actual.body.test_result.variable_data,
                                         actual.body.test_result.variable_data_count );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA:
            EXPECT_EQ( expected.body.variable_result_data.tick_number,
                       actual.body.variable_result_data.tick_number );

            ExpectChannelEqual( expected.body.variable_result_data.channel,
                                actual.body.variable_result_data.channel );

            ExpectByteSpanEqual( expected.body.variable_result_data.data,
                                 actual.body.variable_result_data.data );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE:
            EXPECT_EQ( expected.body.response.scope, actual.body.response.scope );
            EXPECT_EQ( expected.body.response.outcome, actual.body.response.outcome );
            EXPECT_EQ( expected.body.response.reason, actual.body.response.reason );
            EXPECT_EQ( expected.body.response.tick_number, actual.body.response.tick_number );
            EXPECT_EQ( expected.body.response.control_command,
                       actual.body.response.control_command );
            EXPECT_EQ( expected.body.response.global_control_command,
                       actual.body.response.global_control_command );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_ERROR:
            EXPECT_EQ( expected.body.error.category, actual.body.error.category );
            EXPECT_EQ( expected.body.error.recoverable, actual.body.error.recoverable );
            EXPECT_EQ( expected.body.error.has_tick_number, actual.body.error.has_tick_number );
            EXPECT_EQ( expected.body.error.tick_number, actual.body.error.tick_number );

            ExpectByteSpanEqual( expected.body.error.diagnostic_data,
                                 actual.body.error.diagnostic_data );
            break;

        default:
            break;
    }
}

/*
 * This is the existing representative message set from
 * application_test_api.cpp, reproduced here so the behavioural
 * tests can actually exercise every codec family.
 */
std::array<HIL_Application_Message_T, 10u> ConstructCodecMessages()
{
    const HIL_Application_Test_Id_T test_id = ExampleTestId( 0x11u );  // CHANGED

    static const std::array<std::uint8_t, 8u> git_hash{ 'd', 'e', 'a', 'd', 'b', 'e', 'e', 'f' };

    static const std::array<std::uint8_t, 3u> diagnostic{ 1u, 2u, 3u };

    static const std::array<std::uint8_t, 5u> variable_bytes{ 9u, 8u, 7u, 6u, 5u };

    static const std::array<HIL_Application_Peripheral_Config_T, 3u> peripherals{
        [] {
            HIL_Application_Peripheral_Config_T peripheral{};
            peripheral.type                             = HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL;
            peripheral.value.digital.channel.peripheral = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
            peripheral.value.digital.channel.channel    = 2u;
            peripheral.value.digital.output_millivolts  = 3300u;
            peripheral.value.digital.initial_output_high = 0u;
            return peripheral;
        }(),

        [] {
            HIL_Application_Peripheral_Config_T peripheral{};
            peripheral.type                            = HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG;
            peripheral.value.analog.channel.peripheral = HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT;
            peripheral.value.analog.channel.channel    = 0u;
            peripheral.value.analog.minimum_microvolts = -1000000;
            peripheral.value.analog.maximum_microvolts = 1000000;
            return peripheral;
        }(),

        [] {
            HIL_Application_Peripheral_Config_T peripheral{};
            peripheral.type = HIL_APPLICATION_PERIPHERAL_CONFIG_COMMUNICATION;
            peripheral.value.communication.channel.peripheral  = HIL_APPLICATION_PERIPHERAL_UART;
            peripheral.value.communication.channel.channel     = 0u;
            peripheral.value.communication.bit_rate            = 115200u;
            peripheral.value.communication.flags               = 0u;
            peripheral.value.communication.capture_limit_bytes = variable_bytes.size();
            return peripheral;
        }() };
    static const std::array<HIL_Application_Data_Declaration_T, 1u> instruction_data{
        HIL_Application_Data_Declaration_T{
            HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_UART, 0u },
            HIL_Application_Byte_Span_T{ variable_bytes.data(), variable_bytes.size() } } };

    static const std::array<HIL_Application_Data_Declaration_T, 1u> result_data{
        HIL_Application_Data_Declaration_T{
            HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_UART, 0u },
            HIL_Application_Byte_Span_T{ variable_bytes.data(), variable_bytes.size() } } };

    std::array<HIL_Application_Message_T, 10u> messages{};

    messages[0].type                           = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST;
    messages[0].subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    messages[0].has_test_id                    = 0u;
    messages[0].body.system_info_request.query = HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC;
    messages[0].body.system_info_request.request_firmware_git_hash = 1u;

    messages[1].type        = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE;
    messages[1].subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    messages[1].has_test_id = 0u;
    messages[1].body.system_info_response.application_protocol_major =
        HIL_APPLICATION_PROTOCOL_VERSION_MAJOR;
    messages[1].body.system_info_response.application_protocol_minor =
        HIL_APPLICATION_PROTOCOL_VERSION_MINOR;
    messages[1].body.system_info_response.firmware_version_major = 1u;
    messages[1].body.system_info_response.firmware_git_hash =
        HIL_Application_Byte_Span_T{ git_hash.data(), git_hash.size() };
    messages[1].body.system_info_response.diagnostic_data =
        HIL_Application_Byte_Span_T{ diagnostic.data(), diagnostic.size() };

    messages[2].type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    messages[2].subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[2].has_test_id = 1u;
    messages[2].test_id     = test_id;
    messages[2].body.test_configuration.tick_duration.nanoseconds = 1000000u;
    messages[2].body.test_configuration.expected_tick_count       = 100u;
    messages[2].body.test_configuration.flags                     = 0u;
    messages[2].body.test_configuration.peripherals               = peripherals.data();
    messages[2].body.test_configuration.peripheral_count          = peripherals.size();
    messages[2].body.test_configuration.extension_data = HIL_Application_Byte_Span_T{ nullptr, 0u };

    messages[3].type                              = HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION;
    messages[3].subtype                           = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[3].has_test_id                       = 1u;
    messages[3].test_id                           = test_id;
    messages[3].body.test_instruction.tick_number = 0u;
    messages[3].body.test_instruction.digital_outputs[2].high             = 1u;
    messages[3].body.test_instruction.analog_outputs[1].microvolts        = 1250000;
    messages[3].body.test_instruction.pwm_outputs[1].period_nanoseconds   = 1000000u;
    messages[3].body.test_instruction.pwm_outputs[1].duty_cycle_permyriad = 5000u;
    messages[3].body.test_instruction.variable_data                       = instruction_data.data();
    messages[3].body.test_instruction.variable_data_count                 = instruction_data.size();

    messages[4].type        = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA;
    messages[4].subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[4].has_test_id = 1u;
    messages[4].test_id     = test_id;
    messages[4].body.variable_instruction_data.tick_number = 0u;
    messages[4].body.variable_instruction_data.channel =
        HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_UART, 0u };
    messages[4].body.variable_instruction_data.data =
        HIL_Application_Byte_Span_T{ variable_bytes.data(), variable_bytes.size() };

    messages[5].type                           = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    messages[5].subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[5].has_test_id                    = 1u;
    messages[5].test_id                        = test_id;
    messages[5].body.execution_control.command = HIL_APPLICATION_CONTROL_START;
    messages[5].body.execution_control.flags   = 0u;

    messages[6].type                        = HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL;
    messages[6].subtype                     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[6].has_test_id                 = 0u;
    messages[6].body.global_control.command = HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION;
    messages[6].body.global_control.flags   = 0u;

    messages[7].type                                    = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    messages[7].subtype                                 = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[7].has_test_id                             = 1u;
    messages[7].test_id                                 = test_id;
    messages[7].body.test_result.tick_number            = 0u;
    messages[7].body.test_result.digital_inputs[4].high = 1u;
    messages[7].body.test_result.analog_inputs[1].microvolts        = 1210000;
    messages[7].body.test_result.pwm_inputs[1].period_nanoseconds   = 1000100u;
    messages[7].body.test_result.pwm_inputs[1].duty_cycle_permyriad = 4990u;
    messages[7].body.test_result.variable_data                      = result_data.data();
    messages[7].body.test_result.variable_data_count                = result_data.size();
    messages[7].body.test_result.condition = HIL_APPLICATION_RESULT_CONDITION_OK;

    messages[8].type        = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA;
    messages[8].subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[8].has_test_id = 1u;
    messages[8].test_id     = test_id;
    messages[8].body.variable_result_data.tick_number = 0u;
    messages[8].body.variable_result_data.channel =
        HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_UART, 0u };
    messages[8].body.variable_result_data.data =
        HIL_Application_Byte_Span_T{ variable_bytes.data(), variable_bytes.size() };

    messages[9].type                                 = HIL_APPLICATION_MESSAGE_TYPE_RESPONSE;
    messages[9].subtype                              = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[9].has_test_id                          = 1u;
    messages[9].test_id                              = test_id;
    messages[9].body.response.scope                  = HIL_APPLICATION_RESPONSE_SCOPE_TICK;
    messages[9].body.response.outcome                = HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED;
    messages[9].body.response.reason                 = HIL_APPLICATION_RESPONSE_REASON_NONE;
    messages[9].body.response.tick_number            = 0u;
    messages[9].body.response.control_command        = HIL_APPLICATION_CONTROL_INVALID;
    messages[9].body.response.global_control_command = HIL_APPLICATION_GLOBAL_CONTROL_INVALID;

    return messages;
}

}  // namespace

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
    AlignedDecodeStorageBuffer      decode_storage{};
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
    configuration.body.test_configuration.flags                     = 0u;
    configuration.body.test_configuration.extension_data =
        HIL_Application_Byte_Span_T{ nullptr, 0u };

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
                                            &decoded );

    /* Context remains codec-only; endpoint transaction data is never supplied. */
}

void CompileUploadConformanceScenarios()
{
    const HIL_Application_Test_Id_T    test_a = ExampleTestId( 0x21u );
    const HIL_Application_Test_Id_T    test_b = ExampleTestId( 0x22u );
    const std::array<std::uint8_t, 6u> uart_bytes{ 1u, 2u, 3u, 4u, 5u, 6u };
    const std::array<std::uint8_t, 4u> spi_bytes{ 7u, 8u, 9u, 10u };

    HIL_Application_Message_T configuration{};
    configuration.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    configuration.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    configuration.has_test_id = 1u;
    configuration.test_id     = test_a;
    configuration.body.test_configuration.tick_duration.nanoseconds = 1000000u;
    configuration.body.test_configuration.expected_tick_count       = 2u;
    configuration.body.test_configuration.flags                     = 0u;
    configuration.body.test_configuration.extension_data =
        HIL_Application_Byte_Span_T{ nullptr, 0u };

    /* ACCEPTED creates upload A; REJECTED configuration B creates no transaction. */
    const HIL_Application_Message_T configuration_accepted = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TEST_CONFIGURATION,
        HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED, HIL_APPLICATION_RESPONSE_REASON_NONE );
    const HIL_Application_Message_T configuration_rejected =
        TestResponse( test_b, HIL_APPLICATION_RESPONSE_SCOPE_TEST_CONFIGURATION,
                      HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED,
                      HIL_APPLICATION_RESPONSE_REASON_HARDWARE_NOT_READY );

    const std::array<HIL_Application_Data_Declaration_T, 2u> declarations{
        HIL_Application_Data_Declaration_T{
            HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_UART, 0u },
            HIL_Application_Byte_Span_T{ uart_bytes.data(), uart_bytes.size() } },
        HIL_Application_Data_Declaration_T{
            HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_SPI, 1u },
            HIL_Application_Byte_Span_T{ spi_bytes.data(), spi_bytes.size() } },
    };

    HIL_Application_Message_T fixed_tick{};
    fixed_tick.type                                = HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION;
    fixed_tick.subtype                             = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    fixed_tick.has_test_id                         = 1u;
    fixed_tick.test_id                             = test_a;
    fixed_tick.body.test_instruction.tick_number   = 0u;
    fixed_tick.body.test_instruction.variable_data = declarations.data();
    fixed_tick.body.test_instruction.variable_data_count = declarations.size();

    HIL_Application_Message_T variable_tick{};
    variable_tick.type        = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA;
    variable_tick.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    variable_tick.has_test_id = 1u;
    variable_tick.test_id     = test_a;
    variable_tick.body.variable_instruction_data.tick_number = 0u;
    variable_tick.body.variable_instruction_data.channel     = declarations[0].channel;
    variable_tick.body.variable_instruction_data.data =
        HIL_Application_Byte_Span_T{ uart_bytes.data(), uart_bytes.size() };

    HIL_Application_Message_T second_variable_tick{};
    second_variable_tick.type        = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA;
    second_variable_tick.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    second_variable_tick.has_test_id = 1u;
    second_variable_tick.test_id     = test_a;
    second_variable_tick.body.variable_instruction_data.tick_number = 0u;
    second_variable_tick.body.variable_instruction_data.channel     = declarations[1].channel;
    second_variable_tick.body.variable_instruction_data.data =
        HIL_Application_Byte_Span_T{ spi_bytes.data(), spi_bytes.size() };

    /* Tick ACCEPTED represents the complete fixed-plus-variable acceptance. */
    const HIL_Application_Message_T tick_accepted = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TICK, HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED,
        HIL_APPLICATION_RESPONSE_REASON_NONE, 0u );

    /* Stop-and-wait: tick 1 is constructed as permitted only after this ACCEPTED. */
    HIL_Application_Message_T fixed_tick_1                 = fixed_tick;
    fixed_tick_1.body.test_instruction.tick_number         = 1u;
    fixed_tick_1.body.test_instruction.variable_data       = nullptr;
    fixed_tick_1.body.test_instruction.variable_data_count = 0u;
    const HIL_Application_Message_T tick_1_accepted        = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TICK, HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED,
        HIL_APPLICATION_RESPONSE_REASON_NONE, 1u );

    const std::array<HIL_Application_Message_T, 5u> stop_and_wait_sequence{
        fixed_tick, variable_tick, second_variable_tick, tick_accepted, fixed_tick_1,
    };

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
    ( void )second_variable_tick;
    ( void )tick_accepted;
    ( void )fixed_tick_1;
    ( void )tick_1_accepted;
    ( void )stop_and_wait_sequence;
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
    start.body.execution_control.flags   = 0u;

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

void CompileSuccessfulResultConformanceScenario()
{
    const HIL_Application_Test_Id_T    test_a = ExampleTestId( 0x41u );
    const std::array<std::uint8_t, 4u> can_bytes{ 0x10u, 0x20u, 0x30u, 0x40u };
    const std::array<std::uint8_t, 3u> uart_bytes{ 0x50u, 0x60u, 0x70u };

    const std::array<HIL_Application_Data_Declaration_T, 2u> result_declarations{
        HIL_Application_Data_Declaration_T{
            HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_CAN, 0u },
            HIL_Application_Byte_Span_T{ can_bytes.data(), can_bytes.size() } },
        HIL_Application_Data_Declaration_T{
            HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_UART, 1u },
            HIL_Application_Byte_Span_T{ uart_bytes.data(), uart_bytes.size() } },
    };

    HIL_Application_Message_T fixed_result_0{};
    fixed_result_0.type                         = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    fixed_result_0.subtype                      = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    fixed_result_0.has_test_id                  = 1u;
    fixed_result_0.test_id                      = test_a;
    fixed_result_0.body.test_result.tick_number = 0u;
    fixed_result_0.body.test_result.analog_inputs[0].microvolts = 125000;
    fixed_result_0.body.test_result.analog_inputs[1].microvolts = 250000;
    fixed_result_0.body.test_result.variable_data               = result_declarations.data();
    fixed_result_0.body.test_result.variable_data_count         = result_declarations.size();
    fixed_result_0.body.test_result.condition = HIL_APPLICATION_RESULT_CONDITION_OK;

    HIL_Application_Message_T variable_result_0{};
    variable_result_0.type        = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA;
    variable_result_0.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    variable_result_0.has_test_id = 1u;
    variable_result_0.test_id     = test_a;
    variable_result_0.body.variable_result_data.tick_number = 0u;
    variable_result_0.body.variable_result_data.channel     = result_declarations[0].channel;
    variable_result_0.body.variable_result_data.data =
        HIL_Application_Byte_Span_T{ can_bytes.data(), can_bytes.size() };

    HIL_Application_Message_T variable_result_1{};
    variable_result_1.type        = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA;
    variable_result_1.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    variable_result_1.has_test_id = 1u;
    variable_result_1.test_id     = test_a;
    variable_result_1.body.variable_result_data.tick_number = 0u;
    variable_result_1.body.variable_result_data.channel     = result_declarations[1].channel;
    variable_result_1.body.variable_result_data.data =
        HIL_Application_Byte_Span_T{ uart_bytes.data(), uart_bytes.size() };

    HIL_Application_Message_T fixed_result_1                    = fixed_result_0;
    fixed_result_1.body.test_result.tick_number                 = 1u;
    fixed_result_1.body.test_result.analog_inputs[0].microvolts = 126000;
    fixed_result_1.body.test_result.analog_inputs[1].microvolts = 251000;
    fixed_result_1.body.test_result.variable_data               = nullptr;
    fixed_result_1.body.test_result.variable_data_count         = 0u;

    /*
     * Shared order for N=2: fixed tick 0, variables in declaration order, then
     * fixed tick 1. Result messages have no Application Response.
     */
    const std::array<HIL_Application_Message_T, 4u> complete_result_set{
        fixed_result_0,
        variable_result_0,
        variable_result_1,
        fixed_result_1,
    };
    ( void )complete_result_set;
}

void CompilePartialVariableResultScenario()
{
    const HIL_Application_Test_Id_T          test_a = ExampleTestId( 0x43u );
    const std::array<std::uint8_t, 2u>       valid_can_bytes{ 0x11u, 0x22u };
    const HIL_Application_Data_Declaration_T valid_can_declaration{
        HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_CAN, 0u },
        HIL_Application_Byte_Span_T{ valid_can_bytes.data(), valid_can_bytes.size() },
    };

    HIL_Application_Message_T partial_result{};
    partial_result.type                         = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    partial_result.subtype                      = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    partial_result.has_test_id                  = 1u;
    partial_result.test_id                      = test_a;
    partial_result.body.test_result.tick_number = 0u;
    partial_result.body.test_result.digital_inputs[0].high      = 1u;
    partial_result.body.test_result.analog_inputs[0].microvolts = 125000;
    partial_result.body.test_result.variable_data               = &valid_can_declaration;
    partial_result.body.test_result.variable_data_count         = 1u;
    partial_result.body.test_result.condition = HIL_APPLICATION_RESULT_CONDITION_PARTIAL;

    HIL_Application_Message_T valid_variable_result{};
    valid_variable_result.type        = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA;
    valid_variable_result.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    valid_variable_result.has_test_id = 1u;
    valid_variable_result.test_id     = test_a;
    valid_variable_result.body.variable_result_data.tick_number = 0u;
    valid_variable_result.body.variable_result_data.channel     = valid_can_declaration.channel;
    valid_variable_result.body.variable_result_data.data =
        HIL_Application_Byte_Span_T{ valid_can_bytes.data(), valid_can_bytes.size() };

    /* All configured fixed captures remain valid; only failed variable data is omitted. */
    const std::array<HIL_Application_Message_T, 2u> ordered_partial_result{
        partial_result,
        valid_variable_result,
    };
    ( void )ordered_partial_result;
}

void CompileEarlyExecutionFailureResultScenario()
{
    constexpr std::uint32_t                                    expected_tick_count = 3u;
    const HIL_Application_Test_Id_T                            test_a = ExampleTestId( 0x42u );
    std::array<HIL_Application_Message_T, expected_tick_count> fixed_results{};

    fixed_results[0].type                         = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    fixed_results[0].subtype                      = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    fixed_results[0].has_test_id                  = 1u;
    fixed_results[0].test_id                      = test_a;
    fixed_results[0].body.test_result.tick_number = 0u;
    fixed_results[0].body.test_result.analog_inputs[0].microvolts = 125000;
    fixed_results[0].body.test_result.analog_inputs[1].microvolts = 250000;
    fixed_results[0].body.test_result.condition = HIL_APPLICATION_RESULT_CONDITION_OK;

    for ( std::uint32_t tick = 1u; tick < expected_tick_count; ++tick )
    {
        auto& result                                = fixed_results[tick];
        result.type                                 = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
        result.subtype                              = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
        result.has_test_id                          = 1u;
        result.test_id                              = test_a;
        result.body.test_result.tick_number         = tick;
        result.body.test_result.variable_data       = nullptr;
        result.body.test_result.variable_data_count = 0u;
        result.body.test_result.condition = HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM;
        /* Zero-initialized fixed captures are present but Python must ignore them. */
    }

    HIL_Application_Message_T execution_error{};
    execution_error.type                       = HIL_APPLICATION_MESSAGE_TYPE_ERROR;
    execution_error.subtype                    = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    execution_error.has_test_id                = 1u;
    execution_error.test_id                    = test_a;
    execution_error.body.error.category        = HIL_APPLICATION_ERROR_CATEGORY_EXECUTION;
    execution_error.body.error.has_tick_number = 1u;
    execution_error.body.error.tick_number     = 1u;

    /* The Error is optional and does not replace or reorder fixed results 1 and 2. */
    ( void )fixed_results;
    ( void )execution_error;
}

void CompileSerializedOperationScenario()
{
    const HIL_Application_Test_Id_T test_a = ExampleTestId( 0x44u );

    HIL_Application_Message_T system_info_request{};
    system_info_request.type        = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST;
    system_info_request.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    system_info_request.has_test_id = 0u;
    system_info_request.body.system_info_request.query = HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC;

    HIL_Application_Message_T system_info_response{};
    system_info_response.type        = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE;
    system_info_response.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    system_info_response.has_test_id = 0u;
    system_info_response.body.system_info_response.application_protocol_major =
        HIL_APPLICATION_PROTOCOL_VERSION_MAJOR;
    system_info_response.body.system_info_response.application_protocol_minor =
        HIL_APPLICATION_PROTOCOL_VERSION_MINOR;

    HIL_Application_Message_T configuration{};
    configuration.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    configuration.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    configuration.has_test_id = 1u;
    configuration.test_id     = test_a;
    configuration.body.test_configuration.tick_duration.nanoseconds = 1000000u;
    configuration.body.test_configuration.expected_tick_count       = 1u;
    configuration.body.test_configuration.flags                     = 0u;
    configuration.body.test_configuration.extension_data =
        HIL_Application_Byte_Span_T{ nullptr, 0u };

    const HIL_Application_Message_T configuration_accepted = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TEST_CONFIGURATION,
        HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED, HIL_APPLICATION_RESPONSE_REASON_NONE );

    HIL_Application_Message_T tick{};
    tick.type                              = HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION;
    tick.subtype                           = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    tick.has_test_id                       = 1u;
    tick.test_id                           = test_a;
    tick.body.test_instruction.tick_number = 0u;

    const HIL_Application_Message_T tick_accepted = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TICK, HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED,
        HIL_APPLICATION_RESPONSE_REASON_NONE, 0u );
    const HIL_Application_Message_T complete_test_accepted = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_COMPLETE_TEST,
        HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED, HIL_APPLICATION_RESPONSE_REASON_NONE );

    HIL_Application_Message_T start{};
    start.type                           = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    start.subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    start.has_test_id                    = 1u;
    start.test_id                        = test_a;
    start.body.execution_control.command = HIL_APPLICATION_CONTROL_START;
    start.body.execution_control.flags   = 0u;
    const HIL_Application_Message_T start_completed =
        TestResponse( test_a, HIL_APPLICATION_RESPONSE_SCOPE_EXECUTION_CONTROL,
                      HIL_APPLICATION_RESPONSE_OUTCOME_COMPLETED,
                      HIL_APPLICATION_RESPONSE_REASON_NONE, 0u, HIL_APPLICATION_CONTROL_START );

    /*
     * Each response-requiring operation completes before the next request.
     * The automatic Complete Test Response precedes START. No request ID or
     * Application sequence field is needed by this serialized MVP exchange.
     */
    const std::array<HIL_Application_Message_T, 9u> serialized_exchange{
        system_info_request, system_info_response,   configuration, configuration_accepted, tick,
        tick_accepted,       complete_test_accepted, start,         start_completed,
    };
    ( void )serialized_exchange;
}

void CompileRecoveryConformanceScenarios()
{
    const HIL_Application_Test_Id_T invalidated_test = ExampleTestId( 0x51u );
    const HIL_Application_Test_Id_T restarted_test   = ExampleTestId( 0x52u );

    /* A Transport failure made the prior operation uncertain; Python abandons it. */
    const HIL_Application_Message_T late_abandoned_start_response =
        TestResponse( invalidated_test, HIL_APPLICATION_RESPONSE_SCOPE_EXECUTION_CONTROL,
                      HIL_APPLICATION_RESPONSE_OUTCOME_COMPLETED,
                      HIL_APPLICATION_RESPONSE_REASON_NONE, 0u, HIL_APPLICATION_CONTROL_START );

    HIL_Application_Message_T reset{};
    reset.type                        = HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL;
    reset.subtype                     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    reset.has_test_id                 = 0u;
    reset.body.global_control.command = HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION;
    reset.body.global_control.flags   = 0u;

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
    restarted_configuration.body.test_configuration.flags                     = 0u;
    restarted_configuration.body.test_configuration.extension_data =
        HIL_Application_Byte_Span_T{ nullptr, 0u };

    /* RESET is sent only after abandonment; a later prior Response is ignored. */
    ( void )late_abandoned_start_response;
    ( void )reset;
    ( void )reset_completed;
    ( void )in_flight_rejected;
    ( void )restarted_configuration;
}

TEST( ApplicationFacadeApiDesign, IntentionalStubsRemainExplicit )
{
    HIL_Application_Context_T context{};
    HIL_Application_Config_T  config{};
    EXPECT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
}

TEST( ApplicationFacadeApiDesign, DocumentedTransactionScenariosCompile )
{
    const std::array<void ( * )(), 8u> scenarios{
        &CompileCodecFacadeUsage,
        &CompileUploadConformanceScenarios,
        &CompileControlConformanceScenarios,
        &CompileSuccessfulResultConformanceScenario,
        &CompilePartialVariableResultScenario,
        &CompileEarlyExecutionFailureResultScenario,
        &CompileSerializedOperationScenario,
        &CompileRecoveryConformanceScenarios,
    };

    for ( const auto scenario : scenarios )
    {
        EXPECT_NE( scenario, nullptr );
    }
}

TEST( ApplicationDefaultConfig, RejectsNullPointer )
{
    EXPECT_EQ( HIL_APPLICATION_Default_Config( nullptr ), HIL_APPLICATION_STATUS_INVALID_ARGUMENT );
}

TEST( ApplicationDefaultConfig, ProducesProtocolMaximumConfiguration )
{
    HIL_Application_Config_T config{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    EXPECT_EQ( config.max_encoded_message_size, HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE );

    EXPECT_EQ( config.max_variable_data_size, HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE );

    EXPECT_EQ( config.max_peripheral_config_count, HIL_APPLICATION_ABSOLUTE_MAX_PERIPHERAL_COUNT );

    EXPECT_EQ( config.max_variable_transfers_per_tick,
               HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK );

    EXPECT_EQ( config.max_expected_tick_count, HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT );
}

TEST( ApplicationInit, RejectsNullContext )
{
    HIL_Application_Config_T config{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    EXPECT_EQ( HIL_APPLICATION_Init( nullptr, &config ), HIL_APPLICATION_STATUS_INVALID_ARGUMENT );
}

TEST( ApplicationInit, RejectsNullConfig )
{
    HIL_Application_Context_T context{};

    EXPECT_EQ( HIL_APPLICATION_Init( &context, nullptr ), HIL_APPLICATION_STATUS_INVALID_ARGUMENT );
}

TEST( ApplicationInit, AcceptsDefaultConfiguration )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );

    EXPECT_EQ( context.initialized, 1u );

    EXPECT_EQ( context.config.max_encoded_message_size, config.max_encoded_message_size );

    EXPECT_EQ( context.config.max_variable_data_size, config.max_variable_data_size );

    EXPECT_EQ( context.config.max_peripheral_config_count, config.max_peripheral_config_count );

    EXPECT_EQ( context.config.max_variable_transfers_per_tick,
               config.max_variable_transfers_per_tick );

    EXPECT_EQ( context.config.max_expected_tick_count, config.max_expected_tick_count );
}

TEST( ApplicationInit, RejectsExcessiveExpectedTickCount )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    config.max_expected_tick_count = HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT + 1u;

    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_INVALID_LENGTH );
}

TEST( ApplicationInit, RejectsInsufficientEncodedMessageSize )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    config.max_encoded_message_size = HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE - 1u;

    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
}

TEST( ApplicationInit, RejectsExcessiveVariableDataSize )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    config.max_variable_data_size = HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE + 1u;

    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_INVALID_COUNT );
}

TEST( ApplicationInit, RejectsExcessivePeripheralCount )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    config.max_peripheral_config_count = HIL_APPLICATION_ABSOLUTE_MAX_PERIPHERAL_COUNT + 1u;

    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_INVALID_COUNT );
}

TEST( ApplicationInit, RejectsExcessiveVariableTransfersPerTick )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    config.max_variable_transfers_per_tick =
        HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK + 1u;

    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_INVALID_COUNT );
}

TEST( ApplicationEncode, RejectsNullArguments )
{
    HIL_Application_Context_T context = MakeContext();
    HIL_Application_Message_T message{};

    std::array<std::uint8_t, 256u> buffer{};
    std::size_t                    output_size = 0u;

    EXPECT_EQ( HIL_APPLICATION_Encode_Message( nullptr, &message, buffer.data(), buffer.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, nullptr, buffer.data(), buffer.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, nullptr, buffer.size(), &output_size ),
        HIL_APPLICATION_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, buffer.data(), buffer.size(), nullptr ),
        HIL_APPLICATION_STATUS_INVALID_ARGUMENT );
}

TEST( ApplicationEncode, RejectsBufferSmallerThanHeader )
{
    HIL_Application_Context_T context = MakeContext();

    HIL_Application_Message_T message{};
    message.type                           = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    message.subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id                    = 1u;
    message.test_id                        = ExampleTestId( 0x11u );
    message.body.execution_control.command = HIL_APPLICATION_CONTROL_START;

    std::array<std::uint8_t, HIL_APPLICATION_HEADER_SIZE_BYTES - 1u> buffer{};

    std::size_t output_size = 0u;

    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, buffer.data(), buffer.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
}

TEST( ApplicationEncodeDecode, EverySupportedCodecRoundTrips )
{
    HIL_Application_Context_T context = MakeContext();

    const auto messages = ConstructCodecMessages();

    for ( const auto& original : messages )
    {
        SCOPED_TRACE( static_cast<int>( original.type ) );

        std::array<std::uint8_t, 4096u> encoded{};
        std::size_t                     encoded_size = 0u;

        ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &original, encoded.data(),
                                                   encoded.size(), &encoded_size ),
                   HIL_APPLICATION_STATUS_OK );

        ASSERT_GT( encoded_size, 0u );
        ASSERT_LE( encoded_size, encoded.size() );

        HIL_Application_Message_T decoded{};

        ASSERT_EQ(
            HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded ),
            HIL_APPLICATION_STATUS_OK );

        ExpectMessagesEqual( original, decoded );
    }
}

TEST( ApplicationEncodeDecode, TestConfigurationRoundTrips )
{
    const auto  messages = ConstructCodecMessages();
    const auto& original = messages[2];

    HIL_Application_Context_T context = MakeContext();

    std::array<std::uint8_t, 4096u> encoded{};
    std::size_t                     encoded_size = 0u;

    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &original, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );

    HIL_Application_Message_T decoded{};

    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded ),
               HIL_APPLICATION_STATUS_OK );

    ExpectMessagesEqual( original, decoded );

    EXPECT_EQ( decoded.body.test_configuration.peripheral_count, 3u );
}

TEST( ApplicationEncodeDecode, VariableInstructionDataRoundTrips )
{
    const auto  messages = ConstructCodecMessages();
    const auto& original = messages[4];

    HIL_Application_Context_T context = MakeContext();

    std::array<std::uint8_t, 4096u> encoded{};
    std::size_t                     encoded_size = 0u;

    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &original, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );

    HIL_Application_Message_T decoded{};

    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded ),
               HIL_APPLICATION_STATUS_OK );

    ExpectMessagesEqual( original, decoded );
}

TEST( ApplicationEncodeDecode, TestInstructionWithFixedAndVariableDataRoundTrips )
{
    const auto  messages = ConstructCodecMessages();
    const auto& original = messages[3];

    HIL_Application_Context_T context = MakeContext();

    std::array<std::uint8_t, 4096u> encoded{};
    std::size_t                     encoded_size = 0u;

    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &original, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );

    HIL_Application_Message_T decoded{};

    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded ),
               HIL_APPLICATION_STATUS_OK );

    ExpectMessagesEqual( original, decoded );
}

TEST( ApplicationEncodeDecode, TestResultWithFixedAndVariableDataRoundTrips )
{
    const auto  messages = ConstructCodecMessages();
    const auto& original = messages[7];

    HIL_Application_Context_T context = MakeContext();

    std::array<std::uint8_t, 4096u> encoded{};
    std::size_t                     encoded_size = 0u;

    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &original, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );

    HIL_Application_Message_T decoded{};

    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded ),
               HIL_APPLICATION_STATUS_OK );

    ExpectMessagesEqual( original, decoded );
}

TEST( ApplicationDecode, RejectsNullArguments )
{
    HIL_Application_Context_T context = MakeContext();

    std::array<std::uint8_t, 64u> encoded{};
    HIL_Application_Message_T     message{};

    EXPECT_EQ( HIL_APPLICATION_Decode_Message( nullptr, encoded.data(), encoded.size(), &message ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, nullptr, encoded.size(), &message ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded.size(), nullptr ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );
}

TEST( ApplicationEncode, RejectsInvalidMessageType )
{
    HIL_Application_Context_T context = MakeContext();

    HIL_Application_Message_T message{};
    message.type = HIL_APPLICATION_MESSAGE_TYPE_INVALID;

    std::array<std::uint8_t, 256u> buffer{};
    std::size_t                    output_size = 0u;

    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, buffer.data(), buffer.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE );
}

TEST( ApplicationEncode, RejectsReservedMessageType )
{
    HIL_Application_Context_T context = MakeContext();

    HIL_Application_Message_T message{};
    message.type = HIL_APPLICATION_MESSAGE_TYPE_RESERVED;

    std::array<std::uint8_t, 256u> buffer{};
    std::size_t                    output_size = 0u;

    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, buffer.data(), buffer.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
}

TEST( ApplicationEncode, ErrorMessageReturnsInternalError )
{
    HIL_Application_Context_T context = MakeContext();

    HIL_Application_Message_T message{};
    message.type = HIL_APPLICATION_MESSAGE_TYPE_ERROR;

    std::array<std::uint8_t, 256u> buffer{};
    std::size_t                    output_size = 0u;

    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, buffer.data(), buffer.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_INTERNAL_ERROR );
}

TEST( ApplicationDecode, InvalidHeaderIsRejected )
{
    HIL_Application_Context_T context = MakeContext();

    /*
     * Deliberately provide an undersized message. The header decoder
     * should reject it rather than attempting to dispatch the body.
     */
    std::array<std::uint8_t, HIL_APPLICATION_HEADER_SIZE_BYTES - 1u> encoded{};

    HIL_Application_Message_T decoded{};

    EXPECT_NE( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded.size(), &decoded ),
               HIL_APPLICATION_STATUS_OK );
}

TEST( ApplicationValidate, RejectsNullArguments )
{
    HIL_Application_Context_T context = MakeContext();
    HIL_Application_Message_T message{};

    EXPECT_EQ( HIL_APPLICATION_Validate_Message( nullptr, &message ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, nullptr ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );
}

TEST( ApplicationValidateEncoded, RejectsNullArguments )
{
    HIL_Application_Context_T     context = MakeContext();
    std::array<std::uint8_t, 64u> encoded{};
    std::size_t                   required_storage = 0u;

    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( nullptr, encoded.data(), encoded.size(),
                                                         &required_storage ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, nullptr, encoded.size(),
                                                         &required_storage ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, encoded.data(), encoded.size(),
                                                         nullptr ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );
}
