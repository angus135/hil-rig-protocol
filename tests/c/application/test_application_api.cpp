#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"
#include "hil_rig_protocol/version.h"

namespace {
constexpr std::array<HIL_Application_Control_Command_T, 2u> kInitialExecutionControlCommands{
    HIL_APPLICATION_CONTROL_START,
    HIL_APPLICATION_CONTROL_ABORT,
};

constexpr std::array<HIL_Application_Message_Type_T, 6u> kPythonToFirmwareMessageTypes{
    HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST,
    HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION,
    HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION,
    HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA,
    HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL,
    HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL,
};

constexpr std::array<HIL_Application_Message_Type_T, 5u> kFirmwareToPythonMessageTypes{
    HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE,
    HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT,
    HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA,
    HIL_APPLICATION_MESSAGE_TYPE_RESPONSE,
    HIL_APPLICATION_MESSAGE_TYPE_ERROR,
};

template <std::size_t LeftSize, std::size_t RightSize> constexpr bool
DirectionSetsAreDisjoint( const std::array<HIL_Application_Message_Type_T, LeftSize>&  left,
                          const std::array<HIL_Application_Message_Type_T, RightSize>& right )
{
    for ( const auto left_type : left )
    {
        for ( const auto right_type : right )
        {
            if ( left_type == right_type )
            {
                return false;
            }
        }
    }
    return true;
}

HIL_Application_Test_Id_T ExampleTestId()
{
    HIL_Application_Test_Id_T test_id{};
    test_id.bytes[0]  = 0x13u;
    test_id.bytes[15] = 0x9au;
    return test_id;
}

std::array<HIL_Application_Message_T, 11u> ConstructEveryMessageFamily()
{
    const HIL_Application_Test_Id_T test_id = ExampleTestId();

    static const std::array<std::uint8_t, 8u> git_hash{ 'd', 'e', 'a', 'd', 'b', 'e', 'e', 'f' };
    static const std::array<std::uint8_t, 3u> diagnostic{ 1u, 2u, 3u };
    static const std::array<std::uint8_t, 5u> variable_bytes{ 9u, 8u, 7u, 6u, 5u };
    static const std::array<std::uint8_t, 2u> error_bytes{ 0xaau, 0x55u };

    static const std::array<HIL_Application_Peripheral_Config_T, 3u> peripherals{
        [] {
            HIL_Application_Peripheral_Config_T peripheral{};
            peripheral.type                             = HIL_APPLICATION_PERIPHERAL_CONFIG_DIGITAL;
            peripheral.value.digital.channel.peripheral = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
            peripheral.value.digital.channel.channel    = 2u;
            peripheral.value.digital.voltage_level      = HIL_APPLICATION_PERIPHERAL_CONFIG_3V3;
            return peripheral;
        }(),
        [] {
            HIL_Application_Peripheral_Config_T peripheral{};
            peripheral.type                            = HIL_APPLICATION_PERIPHERAL_CONFIG_ANALOG;
            peripheral.value.analog.channel.peripheral = HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT;
            peripheral.value.analog.channel.channel    = 0u;
            peripheral.value.analog.voltage_level      = HIL_APPLICATION_PERIPHERAL_CONFIG_12V;
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
        }(),
    };

    static const std::array<HIL_Application_Data_Declaration_T, 1u> instruction_data{
        HIL_Application_Data_Declaration_T{
            HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_UART, 0u },
            HIL_Application_Byte_Span_T{ variable_bytes.data(), variable_bytes.size() } } };

    static const std::array<HIL_Application_Data_Declaration_T, 1u> result_data{
        HIL_Application_Data_Declaration_T{
            HIL_Application_Channel_Id_T{ HIL_APPLICATION_PERIPHERAL_UART, 0u },
            HIL_Application_Byte_Span_T{ variable_bytes.data(), variable_bytes.size() } } };

    std::array<HIL_Application_Message_T, 11u> messages{};

    messages[0].type                           = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST;
    messages[0].subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    messages[0].has_test_id                    = 0u;
    messages[0].body.system_info_request.query = HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC;
    messages[0].body.system_info_request.request_firmware_git_hash = 1u;

    messages[1].type        = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE;
    messages[1].subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    messages[1].has_test_id = 0u;
    messages[1].body.system_info_response.application_protocol_major =
        HIL_RIG_PROTOCOL_VERSION_MAJOR;
    messages[1].body.system_info_response.application_protocol_minor =
        HIL_RIG_PROTOCOL_VERSION_MINOR;
    messages[1].body.system_info_response.application_protocol_patch =
        HIL_RIG_PROTOCOL_VERSION_PATCH;
    messages[1].body.system_info_response.firmware_version_major = 1u;
    messages[1].body.system_info_response.firmware_git_hash =
        HIL_Application_Byte_Span_T{ git_hash.data(), git_hash.size() };
    messages[1].body.system_info_response.diagnostic_data =
        HIL_Application_Byte_Span_T{ diagnostic.data(), diagnostic.size() };

    messages[2].type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    messages[2].subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[2].has_test_id = 1u;
    messages[2].test_id     = test_id;
    messages[2].body.test_configuration.tick_duration_us.useconds = 1000000u;
    messages[2].body.test_configuration.expected_tick_count          = 100u;
    messages[2].body.test_configuration.flags                        = 0u;
    messages[2].body.test_configuration.peripherals                  = peripherals.data();
    messages[2].body.test_configuration.peripheral_count             = peripherals.size();
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
    // messages[3].body.test_instruction.variable_data                       =
    // instruction_data.data(); messages[3].body.test_instruction.variable_data_count =
    // instruction_data.size();

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
    // messages[7].body.test_result.variable_data                      = result_data.data();
    // messages[7].body.test_result.variable_data_count                = result_data.size();
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

    messages[10].type                       = HIL_APPLICATION_MESSAGE_TYPE_ERROR;
    messages[10].subtype                    = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[10].has_test_id                = 1u;
    messages[10].test_id                    = test_id;
    messages[10].body.error.category        = HIL_APPLICATION_ERROR_CATEGORY_EXECUTION;
    messages[10].body.error.recoverable     = 1u;
    messages[10].body.error.has_tick_number = 1u;
    messages[10].body.error.tick_number     = 0u;
    messages[10].body.error.diagnostic_data =
        HIL_Application_Byte_Span_T{ error_bytes.data(), error_bytes.size() };

    return messages;
}
}  // namespace

static_assert( HIL_APPLICATION_TEST_ID_SIZE == 16u );
static_assert( HIL_APPLICATION_PROTOCOL_VERSION_MAJOR == 1u );
static_assert( HIL_APPLICATION_PROTOCOL_VERSION_MINOR == 0u );
static_assert( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT > 0u );
static_assert( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT >= alignof( std::uint8_t ) );
static_assert( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT
                   % alignof( HIL_Application_Data_Declaration_T )
               == 0u );
static_assert( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT
                   % alignof( HIL_Application_Peripheral_Config_T )
               == 0u );
static_assert( HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT == 10u );
static_assert( HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT == 10u );
static_assert( HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT == 6u );
static_assert( HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT == 2u );
static_assert( HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT == 2u );
static_assert( HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT == 2u );
static_assert( std::extent_v<decltype( HIL_Application_Test_Instruction_T::digital_outputs )>
               == HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT );
static_assert( std::extent_v<decltype( HIL_Application_Test_Instruction_T::analog_outputs )>
               == HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT );
static_assert( std::extent_v<decltype( HIL_Application_Test_Instruction_T::pwm_outputs )>
               == HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT );
static_assert( std::extent_v<decltype( HIL_Application_Test_Result_T::digital_inputs )>
               == HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT );
static_assert( std::extent_v<decltype( HIL_Application_Test_Result_T::analog_inputs )>
               == HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT );
static_assert( std::extent_v<decltype( HIL_Application_Test_Result_T::pwm_inputs )>
               == HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT );
static_assert( std::is_standard_layout_v<HIL_Application_Test_Id_T> );
static_assert( std::is_standard_layout_v<HIL_Application_Message_T> );
static_assert( std::is_standard_layout_v<HIL_Application_Context_T> );
static_assert( sizeof( HIL_Application_Test_Id_T ) == HIL_APPLICATION_TEST_ID_SIZE );
static_assert( kInitialExecutionControlCommands.size() == 2u );
static_assert( HIL_APPLICATION_CONTROL_START != HIL_APPLICATION_CONTROL_ABORT );
static_assert( kPythonToFirmwareMessageTypes.size() == 6u );
static_assert( kFirmwareToPythonMessageTypes.size() == 5u );
static_assert( kPythonToFirmwareMessageTypes.size() + kFirmwareToPythonMessageTypes.size() == 11u );
static_assert( DirectionSetsAreDisjoint( kPythonToFirmwareMessageTypes,
                                         kFirmwareToPythonMessageTypes ) );

TEST( ApplicationApiDesign, EveryRequiredMessageFamilyIsConstructible )
{
    const auto messages = ConstructEveryMessageFamily();
    EXPECT_EQ( messages.front().type, HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST );
    EXPECT_EQ( messages.back().type, HIL_APPLICATION_MESSAGE_TYPE_ERROR );
    EXPECT_EQ( messages[2].body.test_configuration.flags, 0u );
    EXPECT_EQ( messages[2].body.test_configuration.extension_data.size, 0u );
    EXPECT_EQ( messages[2].body.test_configuration.peripherals[2].value.communication.flags, 0u );
    EXPECT_EQ( messages[5].body.execution_control.flags, 0u );
    EXPECT_EQ( messages[6].body.global_control.flags, 0u );
    EXPECT_EQ( messages[1].body.system_info_response.application_protocol_major,
               HIL_RIG_PROTOCOL_VERSION_MAJOR );
    EXPECT_EQ( messages[1].body.system_info_response.application_protocol_minor,
               HIL_RIG_PROTOCOL_VERSION_MINOR );
    EXPECT_EQ( messages[1].body.system_info_response.application_protocol_patch,
               HIL_RIG_PROTOCOL_VERSION_PATCH );
}
