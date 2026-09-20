#include <array>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <iostream>
#include <iomanip>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"
#include "hil_rig_protocol/application/application_test_config.h"
#include "hil_rig_protocol/version.h"

namespace {
struct alignas( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT ) AlignedDecodeStorageBuffer
{
    std::array<std::uint8_t, 4096u> bytes{};
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

void PrintByteSpan( const HIL_Application_Byte_Span_T& data )
{
    std::cout << "    size: " << static_cast<unsigned>( data.size ) << "\n";

    if ( data.data == nullptr || data.size == 0u )
    {
        std::cout << "    data: <empty>\n";
        return;
    }

    std::cout << "    data: ";

    for ( std::size_t i = 0u; i < data.size; ++i )
    {
        std::cout << std::hex << std::setw( 2 ) << std::setfill( '0' )
                  << static_cast<unsigned>( data.data[i] ) << " ";
    }

    std::cout << std::dec << "\n";
}

void PrintTestId( const HIL_Application_Test_Id_T& test_id )
{
    std::cout << "  Test ID: ";

    for ( std::size_t i = 0u; i < HIL_APPLICATION_TEST_ID_SIZE; ++i )
    {
        std::cout << std::hex << std::setw( 2 ) << std::setfill( '0' )
                  << static_cast<unsigned>( test_id.bytes[i] ) << " ";
    }

    std::cout << std::dec << "\n";
}

void PrintMessage( const HIL_Application_Message_T& message )
{
    std::cout << "\n========================================\n";
    std::cout << "HIL Application Message\n";
    std::cout << "========================================\n";

    std::cout << "Type: " << static_cast<unsigned>( message.type ) << "\n";

    std::cout << "Subtype: " << static_cast<unsigned>( message.subtype ) << "\n";

    std::cout << "Has Test ID: " << static_cast<unsigned>( message.has_test_id ) << "\n";

    if ( message.has_test_id )
    {
        PrintTestId( message.test_id );
    }

    std::cout << "\nPayload:\n";

    switch ( message.type )
    {
        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST: {
            const auto& data = message.body.system_info_request;

            std::cout << "  System Info Request\n";
            std::cout << "    query: " << static_cast<unsigned>( data.query ) << "\n";

            std::cout << "    request_firmware_git_hash: "
                      << static_cast<unsigned>( data.request_firmware_git_hash ) << "\n";
            break;
        }

        case HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE: {
            const auto& data = message.body.system_info_response;

            std::cout << "  System Info Response\n";

            std::cout << "    application_protocol_major: "
                      << static_cast<unsigned>( data.application_protocol_major ) << "\n";

            std::cout << "    application_protocol_minor: "
                      << static_cast<unsigned>( data.application_protocol_minor ) << "\n";

            std::cout << "    application_protocol_patch: "
                      << static_cast<unsigned>( data.application_protocol_patch ) << "\n";

            std::cout << "    firmware_version_major: "
                      << static_cast<unsigned>( data.firmware_version_major ) << "\n";

            std::cout << "    firmware_version_minor: "
                      << static_cast<unsigned>( data.firmware_version_minor ) << "\n";

            std::cout << "    firmware_version_patch: "
                      << static_cast<unsigned>( data.firmware_version_patch ) << "\n";

            std::cout << "    firmware_git_hash:\n";
            PrintByteSpan( data.firmware_git_hash );

            std::cout << "    diagnostic_data:\n";
            PrintByteSpan( data.diagnostic_data );

            break;
        }

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION: {
            const auto& data = message.body.test_configuration;
            std::cout << "  Test Configuration\n";
            std::cout << "    tick_duration_us.microseconds: " << data.tick_duration_us.microseconds
                      << "\n";
            std::cout << "    expected_tick_count: " << data.expected_tick_count << "\n";
            std::cout << "    flags: " << data.flags << "\n";
            std::cout << "    extension_data:\n";
            PrintByteSpan( data.extension_data );
            break;
        }

        case HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL: {
            const auto& data = message.body.execution_control;

            std::cout << "  Execution Control\n";

            std::cout << "    command: " << static_cast<unsigned>( data.command ) << "\n";

            std::cout << "    flags: " << data.flags << "\n";

            break;
        }

        case HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL: {
            const auto& data = message.body.global_control;

            std::cout << "  Global Control\n";

            std::cout << "    command: " << static_cast<unsigned>( data.command ) << "\n";

            std::cout << "    flags: " << data.flags << "\n";

            break;
        }

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT: {
            const auto& data = message.body.test_result;

            std::cout << "  Test Result\n";

            std::cout << "    tick_number: " << data.tick_number << "\n";

            std::cout << "    condition: " << static_cast<unsigned>( data.condition ) << "\n";
            break;
        }
        case HIL_APPLICATION_MESSAGE_TYPE_RESPONSE: {
            const auto& data = message.body.response;

            std::cout << "  Response\n";

            std::cout << "    scope: " << static_cast<unsigned>( data.scope ) << "\n";

            std::cout << "    outcome: " << static_cast<unsigned>( data.outcome ) << "\n";

            std::cout << "    reason: " << static_cast<unsigned>( data.reason ) << "\n";

            std::cout << "    tick_number: " << data.tick_number << "\n";

            std::cout << "    control_command: " << static_cast<unsigned>( data.control_command )
                      << "\n";

            std::cout << "    global_control_command: "
                      << static_cast<unsigned>( data.global_control_command ) << "\n";

            break;
        }

        case HIL_APPLICATION_MESSAGE_TYPE_ERROR: {
            const auto& data = message.body.error;

            std::cout << "  Error\n";

            std::cout << "    category: " << static_cast<unsigned>( data.category ) << "\n";

            std::cout << "    recoverable: " << static_cast<unsigned>( data.recoverable ) << "\n";

            std::cout << "    has_tick_number: " << static_cast<unsigned>( data.has_tick_number )
                      << "\n";

            std::cout << "    tick_number: " << data.tick_number << "\n";

            std::cout << "    diagnostic_data:\n";
            PrintByteSpan( data.diagnostic_data );

            break;
        }

        default: {
            std::cout << "  Unknown/Unsupported Message Type\n";
            break;
        }
    }

    std::cout << "========================================\n";
}

void printEncodedArr( std::array<std::uint8_t, 4096u> encoded, std::size_t encoded_size )
{
    std::cout << "Encoded message (" << encoded_size << " bytes):\n";
    for ( std::size_t i = 0; i < encoded_size; ++i )
    {
        std::cout << "  [" << i << "] = 0x" << std::hex << std::setw( 2 ) << std::setfill( '0' )
                  << static_cast<unsigned>( encoded[i] ) << std::dec << "\n";
    }
}

template <typename T> void ExpectUint32Encoded( T expected, const std::uint8_t* encoded )
{
    const std::uint32_t value = static_cast<std::uint32_t>( expected );

    EXPECT_EQ( encoded[0], static_cast<std::uint8_t>( value & 0xFFu ) );

    EXPECT_EQ( encoded[1], static_cast<std::uint8_t>( ( value >> 8u ) & 0xFFu ) );

    EXPECT_EQ( encoded[2], static_cast<std::uint8_t>( ( value >> 16u ) & 0xFFu ) );

    EXPECT_EQ( encoded[3], static_cast<std::uint8_t>( ( value >> 24u ) & 0xFFu ) );
}

void ExpectByteSpanEqual( const HIL_Application_Byte_Span_T& expected,
                          const HIL_Application_Byte_Span_T& actual )
{
    // std::cout << "\n--- Byte Span Comparison ---\n";

    // std::cout << "Expected:\n";
    // std::cout << "  data pointer: " << static_cast<const void*>( expected.data ) << "\n";
    // std::cout << "  size: " << static_cast<unsigned>( expected.size ) << "\n";

    // std::cout << "Actual:\n";
    // std::cout << "  data pointer: " << static_cast<const void*>( actual.data ) << "\n";
    // std::cout << "  size: " << static_cast<unsigned>( actual.size ) << "\n";

    // if (actual.size == 0 ) {
    //     ASSERT_EQ( expected.data, nullptr );
    //     ASSERT_EQ( actual.data, nullptr );
    // } else {
    //     ASSERT_NE( expected.data, nullptr );
    //     ASSERT_NE( actual.data, nullptr );
    // }

    // ASSERT_EQ( expected.size, actual.size );

    // ASSERT_EQ( std::memcmp( expected.data, actual.data, expected.size ), 0 );
    ASSERT_EQ( expected.size, actual.size );

    if ( expected.size == 0u )
    {
        return;
    }

    ASSERT_NE( expected.data, nullptr );
    ASSERT_NE( actual.data, nullptr );

    EXPECT_EQ( std::memcmp( expected.data, actual.data, expected.size ), 0 );
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
            EXPECT_EQ( expected.body.system_info_response.application_protocol_patch,
                       actual.body.system_info_response.application_protocol_patch );
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
            EXPECT_EQ( expected.body.test_configuration.tick_duration_us.microseconds,
                       actual.body.test_configuration.tick_duration_us.microseconds );
            EXPECT_EQ( expected.body.test_configuration.expected_tick_count,
                       actual.body.test_configuration.expected_tick_count );
            EXPECT_EQ( expected.body.test_configuration.flags,
                       actual.body.test_configuration.flags );

            for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.digital_in[i].enabled,
                           actual.body.test_configuration.digital_in[i].enabled );
                EXPECT_EQ( expected.body.test_configuration.digital_in[i].voltage_level,
                           actual.body.test_configuration.digital_in[i].voltage_level );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.digital_out[i].enabled,
                           actual.body.test_configuration.digital_out[i].enabled );
                EXPECT_EQ( expected.body.test_configuration.digital_out[i].voltage_level,
                           actual.body.test_configuration.digital_out[i].voltage_level );
                EXPECT_EQ( expected.body.test_configuration.digital_out[i].initial_high,
                           actual.body.test_configuration.digital_out[i].initial_high );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.analog_in[i].enabled,
                           actual.body.test_configuration.analog_in[i].enabled );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.analog_out[i].enabled,
                           actual.body.test_configuration.analog_out[i].enabled );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.pwm_in[i].enabled,
                           actual.body.test_configuration.pwm_in[i].enabled );
                EXPECT_EQ( expected.body.test_configuration.pwm_in[i].voltage_level,
                           actual.body.test_configuration.pwm_in[i].voltage_level );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.pwm_out[i].enabled,
                           actual.body.test_configuration.pwm_out[i].enabled );
                EXPECT_EQ( expected.body.test_configuration.pwm_out[i].voltage_level,
                           actual.body.test_configuration.pwm_out[i].voltage_level );
                EXPECT_EQ( expected.body.test_configuration.pwm_out[i].initial_period_nanoseconds,
                           actual.body.test_configuration.pwm_out[i].initial_period_nanoseconds );
                EXPECT_EQ( expected.body.test_configuration.pwm_out[i].initial_duty_cycle_permyriad,
                           actual.body.test_configuration.pwm_out[i].initial_duty_cycle_permyriad );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_CAN_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.can[i].enabled,
                           actual.body.test_configuration.can[i].enabled );
                EXPECT_EQ( expected.body.test_configuration.can[i].bit_rate,
                           actual.body.test_configuration.can[i].bit_rate );
                EXPECT_EQ( expected.body.test_configuration.can[i].filter_id,
                           actual.body.test_configuration.can[i].filter_id );
                EXPECT_EQ( expected.body.test_configuration.can[i].filter_mask,
                           actual.body.test_configuration.can[i].filter_mask );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_SPI_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.spi[i].enabled,
                           actual.body.test_configuration.spi[i].enabled );
                EXPECT_EQ( expected.body.test_configuration.spi[i].bit_rate,
                           actual.body.test_configuration.spi[i].bit_rate );
                EXPECT_EQ( expected.body.test_configuration.spi[i].role,
                           actual.body.test_configuration.spi[i].role );
                EXPECT_EQ( expected.body.test_configuration.spi[i].data_width,
                           actual.body.test_configuration.spi[i].data_width );
                EXPECT_EQ( expected.body.test_configuration.spi[i].bit_order,
                           actual.body.test_configuration.spi[i].bit_order );
                EXPECT_EQ( expected.body.test_configuration.spi[i].clock_polarity,
                           actual.body.test_configuration.spi[i].clock_polarity );
                EXPECT_EQ( expected.body.test_configuration.spi[i].clock_phase,
                           actual.body.test_configuration.spi[i].clock_phase );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_UART_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.uart[i].enabled,
                           actual.body.test_configuration.uart[i].enabled );
                EXPECT_EQ( expected.body.test_configuration.uart[i].baud_rate,
                           actual.body.test_configuration.uart[i].baud_rate );
                EXPECT_EQ( expected.body.test_configuration.uart[i].electrical_mode,
                           actual.body.test_configuration.uart[i].electrical_mode );
                EXPECT_EQ( expected.body.test_configuration.uart[i].word_length,
                           actual.body.test_configuration.uart[i].word_length );
                EXPECT_EQ( expected.body.test_configuration.uart[i].parity,
                           actual.body.test_configuration.uart[i].parity );
                EXPECT_EQ( expected.body.test_configuration.uart[i].stop_bits,
                           actual.body.test_configuration.uart[i].stop_bits );
                EXPECT_EQ( expected.body.test_configuration.uart[i].rx_enabled,
                           actual.body.test_configuration.uart[i].rx_enabled );
                EXPECT_EQ( expected.body.test_configuration.uart[i].tx_enabled,
                           actual.body.test_configuration.uart[i].tx_enabled );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_I2C_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_configuration.i2c[i].enabled,
                           actual.body.test_configuration.i2c[i].enabled );
                EXPECT_EQ( expected.body.test_configuration.i2c[i].bit_rate,
                           actual.body.test_configuration.i2c[i].bit_rate );
                EXPECT_EQ( expected.body.test_configuration.i2c[i].role,
                           actual.body.test_configuration.i2c[i].role );
                EXPECT_EQ( expected.body.test_configuration.i2c[i].own_address_7bit,
                           actual.body.test_configuration.i2c[i].own_address_7bit );
                EXPECT_EQ( expected.body.test_configuration.i2c[i].voltage_level,
                           actual.body.test_configuration.i2c[i].voltage_level );
                EXPECT_EQ( expected.body.test_configuration.i2c[i].pull_up,
                           actual.body.test_configuration.i2c[i].pull_up );
            }
            ExpectByteSpanEqual( expected.body.test_configuration.extension_data,
                                 actual.body.test_configuration.extension_data );
            break;

        case HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION:
            EXPECT_EQ( expected.body.test_instruction.tick_number,
                       actual.body.test_instruction.tick_number );

            for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_instruction.digital_outputs[i].high,
                           actual.body.test_instruction.digital_outputs[i].high );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_instruction.analog_outputs[i].microvolts,
                           actual.body.test_instruction.analog_outputs[i].microvolts );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_instruction.pwm_outputs[i].period_nanoseconds,
                           actual.body.test_instruction.pwm_outputs[i].period_nanoseconds );
                EXPECT_EQ( expected.body.test_instruction.pwm_outputs[i].duty_cycle_permyriad,
                           actual.body.test_instruction.pwm_outputs[i].duty_cycle_permyriad );
            }
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

            for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_result.digital_inputs[i].high,
                           actual.body.test_result.digital_inputs[i].high );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_result.analog_inputs[i].microvolts,
                           actual.body.test_result.analog_inputs[i].microvolts );
            }
            for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
            {
                EXPECT_EQ( expected.body.test_result.pwm_inputs[i].period_nanoseconds,
                           actual.body.test_result.pwm_inputs[i].period_nanoseconds );
                EXPECT_EQ( expected.body.test_result.pwm_inputs[i].duty_cycle_permyriad,
                           actual.body.test_result.pwm_inputs[i].duty_cycle_permyriad );
            }

            EXPECT_EQ( expected.body.test_result.condition, actual.body.test_result.condition );
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
std::array<HIL_Application_Message_T, 9u> ConstructCodecMessages()
{
    const HIL_Application_Test_Id_T test_id = ExampleTestId( 0x11u );  // CHANGED

    static const std::array<std::uint8_t, 8u> git_hash{ 'd', 'e', 'a', 'd', 'b', 'e', 'e', 'f' };

    static const std::array<std::uint8_t, 3u> diagnostic{ 1u, 2u, 3u };

    static const std::array<std::uint8_t, 2u> error_bytes{ 0xaau, 0x55u };

    std::array<HIL_Application_Message_T, 9u> messages{};

    messages[0].type                           = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST;
    messages[0].subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    messages[0].has_test_id                    = 0u;
    messages[0].body.system_info_request.query = HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC;
    messages[0].body.system_info_request.request_firmware_git_hash = 1u;
    messages[0].body.system_info_request.application_protocol_major =
        HIL_RIG_PROTOCOL_VERSION_MAJOR;
    messages[0].body.system_info_request.application_protocol_minor =
        HIL_RIG_PROTOCOL_VERSION_MINOR;
    messages[0].body.system_info_request.application_protocol_patch =
        HIL_RIG_PROTOCOL_VERSION_PATCH;

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
        HIL_Application_Byte_Span_T{ git_hash.data(), ( uint8_t )git_hash.size() };
    messages[1].body.system_info_response.diagnostic_data =
        HIL_Application_Byte_Span_T{ diagnostic.data(), ( uint8_t )diagnostic.size() };

    messages[2].type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    messages[2].subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[2].has_test_id = 1u;
    messages[2].test_id     = test_id;
    messages[2].body.test_configuration.tick_duration_us.microseconds = 1000u;
    messages[2].body.test_configuration.expected_tick_count           = 100u;
    messages[2].body.test_configuration.flags                         = 0u;
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
    messages[4].type                           = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    messages[4].subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[4].has_test_id                    = 1u;
    messages[4].test_id                        = test_id;
    messages[4].body.execution_control.command = HIL_APPLICATION_CONTROL_START;
    messages[4].body.execution_control.flags   = 0u;

    messages[5].type                        = HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL;
    messages[5].subtype                     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[5].has_test_id                 = 0u;
    messages[5].body.global_control.command = HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION;
    messages[5].body.global_control.flags   = 0u;

    messages[6].type                                    = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    messages[6].subtype                                 = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[6].has_test_id                             = 1u;
    messages[6].test_id                                 = test_id;
    messages[6].body.test_result.tick_number            = 0u;
    messages[6].body.test_result.digital_inputs[4].high = 1u;
    messages[6].body.test_result.analog_inputs[1].microvolts        = 1210000;
    messages[6].body.test_result.pwm_inputs[1].period_nanoseconds   = 1000100u;
    messages[6].body.test_result.pwm_inputs[1].duty_cycle_permyriad = 4990u;
    messages[6].body.test_result.condition = HIL_APPLICATION_RESULT_CONDITION_OK;

    messages[7].type                                 = HIL_APPLICATION_MESSAGE_TYPE_RESPONSE;
    messages[7].subtype                              = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[7].has_test_id                          = 1u;
    messages[7].test_id                              = test_id;
    messages[7].body.response.scope                  = HIL_APPLICATION_RESPONSE_SCOPE_TICK;
    messages[7].body.response.outcome                = HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED;
    messages[7].body.response.reason                 = HIL_APPLICATION_RESPONSE_REASON_NONE;
    messages[7].body.response.tick_number            = 0u;
    messages[7].body.response.control_command        = HIL_APPLICATION_CONTROL_INVALID;
    messages[7].body.response.global_control_command = HIL_APPLICATION_GLOBAL_CONTROL_INVALID;

    messages[8].type                       = HIL_APPLICATION_MESSAGE_TYPE_ERROR;
    messages[8].subtype                    = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    messages[8].has_test_id                = 1u;
    messages[8].test_id                    = test_id;
    messages[8].body.error.category        = HIL_APPLICATION_ERROR_CATEGORY_EXECUTION;
    messages[8].body.error.recoverable     = 1u;
    messages[8].body.error.has_tick_number = 1u;
    messages[8].body.error.tick_number     = 0u;
    messages[8].body.error.diagnostic_data = HIL_Application_Byte_Span_T{
        error_bytes.data(), static_cast<std::uint8_t>( error_bytes.size() ) };

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

    std::array<std::uint8_t, 4096u> encoded_message{};
    std::array<std::uint8_t, 4096u> decode_storage{};
    std::size_t                     used_decoded_size{};
    std::size_t                     encoded_size            = 0u;
    std::size_t                     required_decode_storage = 0u;
    HIL_Application_Message_T       decoded{};

    ( void )HIL_APPLICATION_Default_Config( &config );
    config.max_encoded_message_size = encoded_message.size();
    config.max_variable_data_size   = 512u;
    config.max_expected_tick_count  = 1000u;
    ( void )HIL_APPLICATION_Init( &context, &config );

    HIL_Application_Message_T configuration{};
    configuration.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    configuration.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    configuration.has_test_id = 1u;
    configuration.test_id     = ExampleTestId( 0x11u );
    configuration.body.test_configuration.tick_duration_us.microseconds = 1000u;
    configuration.body.test_configuration.expected_tick_count           = 2u;
    configuration.body.test_configuration.flags                         = 0u;
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
                                            &decoded, decode_storage.data(), decode_storage.size(),
                                            &used_decoded_size );

    /* Context remains codec-only; endpoint transaction data is never supplied. */
}

void CompileUploadConformanceScenarios()
{
    const HIL_Application_Test_Id_T test_a = ExampleTestId( 0x21u );
    const HIL_Application_Test_Id_T test_b = ExampleTestId( 0x22u );

    HIL_Application_Message_T configuration{};
    configuration.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    configuration.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    configuration.has_test_id = 1u;
    configuration.test_id     = test_a;
    configuration.body.test_configuration.tick_duration_us.microseconds = 1000u;
    configuration.body.test_configuration.expected_tick_count           = 2u;
    configuration.body.test_configuration.flags                         = 0u;
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

    HIL_Application_Message_T fixed_tick{};
    fixed_tick.type                               = HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION;
    fixed_tick.subtype                            = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    fixed_tick.has_test_id                        = 1u;
    fixed_tick.test_id                            = test_a;
    fixed_tick.body.test_instruction.tick_number  = 0u;
    const HIL_Application_Message_T tick_accepted = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TICK, HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED,
        HIL_APPLICATION_RESPONSE_REASON_NONE, 0u );

    /* Stop-and-wait: tick 1 is constructed as permitted only after this ACCEPTED. */
    HIL_Application_Message_T fixed_tick_1          = fixed_tick;
    fixed_tick_1.body.test_instruction.tick_number  = 1u;
    const HIL_Application_Message_T tick_1_accepted = TestResponse(
        test_a, HIL_APPLICATION_RESPONSE_SCOPE_TICK, HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED,
        HIL_APPLICATION_RESPONSE_REASON_NONE, 1u );

    const std::array<HIL_Application_Message_T, 3u> stop_and_wait_sequence{
        fixed_tick,
        tick_accepted,
        fixed_tick_1,
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
    const HIL_Application_Test_Id_T test_a = ExampleTestId( 0x41u );
    HIL_Application_Message_T       fixed_result_0{};
    fixed_result_0.type                         = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    fixed_result_0.subtype                      = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    fixed_result_0.has_test_id                  = 1u;
    fixed_result_0.test_id                      = test_a;
    fixed_result_0.body.test_result.tick_number = 0u;
    fixed_result_0.body.test_result.analog_inputs[0].microvolts = 125000;
    fixed_result_0.body.test_result.analog_inputs[1].microvolts = 250000;
    fixed_result_0.body.test_result.condition = HIL_APPLICATION_RESULT_CONDITION_OK;

    HIL_Application_Message_T fixed_result_1                    = fixed_result_0;
    fixed_result_1.body.test_result.tick_number                 = 1u;
    fixed_result_1.body.test_result.analog_inputs[0].microvolts = 126000;
    fixed_result_1.body.test_result.analog_inputs[1].microvolts = 251000;
    const std::array<HIL_Application_Message_T, 2u> complete_result_set{
        fixed_result_0,
        fixed_result_1,
    };
    ( void )complete_result_set;
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
        auto& result                        = fixed_results[tick];
        result.type                         = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
        result.subtype                      = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
        result.has_test_id                  = 1u;
        result.test_id                      = test_a;
        result.body.test_result.tick_number = tick;
        result.body.test_result.condition   = HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM;
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
    system_info_request.body.system_info_request.application_protocol_major =
        HIL_RIG_PROTOCOL_VERSION_MAJOR;
    system_info_request.body.system_info_request.application_protocol_minor =
        HIL_RIG_PROTOCOL_VERSION_MINOR;
    system_info_request.body.system_info_request.application_protocol_patch =
        HIL_RIG_PROTOCOL_VERSION_PATCH;

    HIL_Application_Message_T system_info_response{};
    system_info_response.type        = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE;
    system_info_response.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    system_info_response.has_test_id = 0u;
    system_info_response.body.system_info_response.application_protocol_major =
        HIL_RIG_PROTOCOL_VERSION_MAJOR;
    system_info_response.body.system_info_response.application_protocol_minor =
        HIL_RIG_PROTOCOL_VERSION_MINOR;
    system_info_response.body.system_info_response.application_protocol_patch =
        HIL_RIG_PROTOCOL_VERSION_PATCH;

    HIL_Application_Message_T configuration{};
    configuration.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    configuration.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    configuration.has_test_id = 1u;
    configuration.test_id     = test_a;
    configuration.body.test_configuration.tick_duration_us.microseconds = 1000u;
    configuration.body.test_configuration.expected_tick_count           = 1u;
    configuration.body.test_configuration.flags                         = 0u;
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
    HIL_Application_Message_T finalize_upload{};
    finalize_upload.type        = HIL_APPLICATION_MESSAGE_TYPE_FINALIZE_TEST_UPLOAD;
    finalize_upload.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    finalize_upload.has_test_id = 1u;
    finalize_upload.test_id     = test_a;
    finalize_upload.body.finalize_test_upload.flags        = 0u;
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
     * The host sends FINALIZE_TEST_UPLOAD before the Complete Test Response
     * that precedes START. No request ID or Application sequence field is
     * needed by this serialized MVP exchange.
     */
    const std::array<HIL_Application_Message_T, 10u> serialized_exchange{
        system_info_request,
        system_info_response,
        configuration,
        configuration_accepted,
        tick,
        tick_accepted,
        finalize_upload,
        complete_test_accepted,
        start,
        start_completed,
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
    restarted_configuration.body.test_configuration.tick_duration_us.microseconds = 1000u;
    restarted_configuration.body.test_configuration.expected_tick_count           = 2u;
    restarted_configuration.body.test_configuration.flags                         = 0u;
    restarted_configuration.body.test_configuration.extension_data =
        HIL_Application_Byte_Span_T{ nullptr, 0u };

    /* RESET is sent only after abandonment; a later prior Response is ignored. */
    ( void )late_abandoned_start_response;
    ( void )reset;
    ( void )reset_completed;
    ( void )in_flight_rejected;
    ( void )restarted_configuration;
}

TEST( ApplicationFacadeApiDesign, DocumentedTransactionScenariosCompile )
{
    const std::array<void ( * )(), 7u> scenarios{
        &CompileCodecFacadeUsage,
        &CompileUploadConformanceScenarios,
        &CompileControlConformanceScenarios,
        &CompileSuccessfulResultConformanceScenario,
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

TEST( ApplicationDefaultConfig, ProducesDefaultOperationalConfiguration )
{
    HIL_Application_Config_T config{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    EXPECT_EQ( config.max_encoded_message_size, HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE );

    EXPECT_EQ( config.max_variable_data_size, HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE );

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

TEST( ApplicationInit, RejectsEncodedMessageSizeBelowStructuralMinimum )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    config.max_encoded_message_size = HIL_APPLICATION_MIN_COMPLETE_MESSAGE_SIZE - 1u;

    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( context.initialized, 0u );
}

TEST( ApplicationInit, AcceptsReducedValidEncodedMessageSize )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_encoded_message_size = 128u;

    ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( context.config.max_encoded_message_size, 128u );
}

TEST( ApplicationInit, RejectsEncodedMessageSizeAboveRepresentableMaximum )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_encoded_message_size = HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE + 1u;

    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_INVALID_LENGTH );
    EXPECT_EQ( context.initialized, 0u );
}

TEST( ApplicationInit, RejectsExcessiveVariableDataSize )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};

    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );

    config.max_variable_data_size = HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE + 1u;

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
        if ( original.type != HIL_APPLICATION_MESSAGE_TYPE_RESPONSE
             && original.type != HIL_APPLICATION_MESSAGE_TYPE_ERROR )
        {
            std::array<std::uint8_t, 4096u> encoded{};
            std::size_t                     encoded_size = 0u;
            std::array<std::uint8_t, 4096u> decode_storage{};
            std::size_t                     used_decoded_size{};

            ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &original, encoded.data(),
                                                       encoded.size(), &encoded_size ),
                       HIL_APPLICATION_STATUS_OK );

            ASSERT_GT( encoded_size, 0u );
            ASSERT_LE( encoded_size, encoded.size() );

            HIL_Application_Message_T decoded{};

            ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size,
                                                       &decoded, decode_storage.data(),
                                                       decode_storage.size(), &used_decoded_size ),
                       HIL_APPLICATION_STATUS_OK );

            if ( original.type == HIL_APPLICATION_MESSAGE_TYPE_ERROR )
            {
                PrintMessage( original );
                PrintMessage( decoded );
                printEncodedArr( encoded, 90 );
            }
            ExpectMessagesEqual( original, decoded );
        }
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

    /* 1000 us is encoded directly as little-endian uint32_t 0x000003E8. */
    ASSERT_GT( encoded_size, HIL_APPLICATION_HEADER_SIZE_BYTES + 3u );
    EXPECT_EQ( encoded[HIL_APPLICATION_HEADER_SIZE_BYTES + 0u], 0xe8u );
    EXPECT_EQ( encoded[HIL_APPLICATION_HEADER_SIZE_BYTES + 1u], 0x03u );
    EXPECT_EQ( encoded[HIL_APPLICATION_HEADER_SIZE_BYTES + 2u], 0x00u );
    EXPECT_EQ( encoded[HIL_APPLICATION_HEADER_SIZE_BYTES + 3u], 0x00u );

    HIL_Application_Message_T       decoded{};
    std::array<std::uint8_t, 4096u> decode_storage{};
    std::size_t                     used_decoded_size{};

    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded,
                                               decode_storage.data(), decode_storage.size(),
                                               &used_decoded_size ),
               HIL_APPLICATION_STATUS_OK );

    ExpectMessagesEqual( original, decoded );
}

TEST( ApplicationEncodeDecode, TestInstructionFixedBodyRoundTrips )
{
    const auto  messages = ConstructCodecMessages();
    const auto& original = messages[3];

    HIL_Application_Context_T context = MakeContext();

    std::array<std::uint8_t, 4096u> encoded{};
    std::size_t                     encoded_size = 0u;

    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &original, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );

    HIL_Application_Message_T       decoded{};
    std::array<std::uint8_t, 4096u> decode_storage{};
    std::size_t                     used_decoded_size{};

    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded,
                                               decode_storage.data(), decode_storage.size(),
                                               &used_decoded_size ),
               HIL_APPLICATION_STATUS_OK );

    ExpectMessagesEqual( original, decoded );
}

TEST( ApplicationEncodeDecode, TestResultFixedBodyRoundTrips )
{
    const auto  messages = ConstructCodecMessages();
    const auto& original = messages[7];

    HIL_Application_Context_T context = MakeContext();

    std::array<std::uint8_t, 4096u> encoded{};
    std::size_t                     encoded_size = 0u;

    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &original, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );

    HIL_Application_Message_T       decoded{};
    std::array<std::uint8_t, 4096u> decode_storage{};
    std::size_t                     used_decoded_size{};

    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded,
                                               decode_storage.data(), decode_storage.size(),
                                               &used_decoded_size ),
               HIL_APPLICATION_STATUS_OK );

    ExpectMessagesEqual( original, decoded );
}

TEST( ApplicationDecode, RejectsNullArguments )
{
    HIL_Application_Context_T context = MakeContext();

    std::array<std::uint8_t, 64u>   encoded{};
    HIL_Application_Message_T       message{};
    std::array<std::uint8_t, 4096u> decode_storage{};
    std::size_t                     used_decoded_size{};

    EXPECT_EQ( HIL_APPLICATION_Decode_Message( nullptr, encoded.data(), encoded.size(), &message,
                                               decode_storage.data(), decode_storage.size(),
                                               &used_decoded_size ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, nullptr, encoded.size(), &message,
                                               decode_storage.data(), decode_storage.size(),
                                               &used_decoded_size ),
               HIL_APPLICATION_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded.size(), nullptr,
                                               decode_storage.data(), decode_storage.size(),
                                               &used_decoded_size ),
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
               HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE );
    EXPECT_EQ( output_size, 0u );
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
               HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE );
    EXPECT_EQ( output_size, 0u );
}

TEST( ApplicationDecode, InvalidHeaderIsRejected )
{
    HIL_Application_Context_T context = MakeContext();

    /*
     * Deliberately provide an undersized message. The header decoder
     * should reject it rather than attempting to dispatch the body.
     */
    std::array<std::uint8_t, HIL_APPLICATION_HEADER_SIZE_BYTES - 1u> encoded{};

    HIL_Application_Message_T       decoded{};
    std::array<std::uint8_t, 4096u> decode_storage{};
    std::size_t                     used_decoded_size{};

    EXPECT_NE( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded.size(), &decoded,
                                               decode_storage.data(), decode_storage.size(),
                                               &used_decoded_size ),
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
