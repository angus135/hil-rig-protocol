#include "support/application_test_codec.hpp"

#include <algorithm>
#include <cstring>

namespace hil_rig_protocol::test {

HIL_Application_Status_T ApplicationTestCodec::Initialize( const HIL_Application_Config_T& config )
{
    context_         = {};
    decoded_message_ = {};
    decode_storage_.fill( 0u );
    const HIL_Application_Status_T status = HIL_APPLICATION_Init( &context_, &config );
    if ( status == HIL_APPLICATION_STATUS_OK )
    {
        config_ = config;
    }
    else
    {
        config_ = {};
    }
    return status;
}

HIL_Application_Status_T
ApplicationTestCodec::Initialize( const std::size_t   max_encoded_message_size,
                                  const std::size_t   max_variable_data_size,
                                  const std::uint32_t max_expected_tick_count )
{
    HIL_Application_Config_T       config{};
    const HIL_Application_Status_T default_status = HIL_APPLICATION_Default_Config( &config );
    if ( default_status != HIL_APPLICATION_STATUS_OK )
    {
        return default_status;
    }
    config.max_encoded_message_size = max_encoded_message_size;
    config.max_variable_data_size   = max_variable_data_size;
    config.max_expected_tick_count  = max_expected_tick_count;
    return Initialize( config );
}

ApplicationEncodeResult
ApplicationTestCodec::EncodeSupportedMessage( const HIL_Application_Message_T& message )
{
    ApplicationEncodeResult result{};
    result.validation_status = HIL_APPLICATION_Validate_Message( &context_, &message );
    result.sizing_status =
        HIL_APPLICATION_Encoded_Size( &context_, &message, &result.encoded_size );
    if ( result.sizing_status != HIL_APPLICATION_STATUS_OK )
    {
        return result;
    }

    result.bytes.resize( result.encoded_size );
    result.encoding_status = HIL_APPLICATION_Encode_Message(
        &context_, &message, result.bytes.data(), result.bytes.size(), &result.output_size );
    if ( result.encoding_status != HIL_APPLICATION_STATUS_OK )
    {
        result.bytes.clear();
        return result;
    }
    result.bytes.resize( result.output_size );
    return result;
}

ApplicationDirectEncodeResult
ApplicationTestCodec::EncodeMessageWithCapacity( const HIL_Application_Message_T& message,
                                                 const std::size_t                buffer_capacity )
{
    ApplicationDirectEncodeResult result{};
    result.validation_status = HIL_APPLICATION_Validate_Message( &context_, &message );
    result.bytes.resize( buffer_capacity );
    std::uint8_t* const output = buffer_capacity == 0u ? nullptr : result.bytes.data();
    result.encoding_status     = HIL_APPLICATION_Encode_Message( &context_, &message, output,
                                                                 buffer_capacity, &result.output_size );
    if ( result.encoding_status != HIL_APPLICATION_STATUS_OK )
    {
        result.bytes.clear();
        return result;
    }
    result.bytes.resize( result.output_size );
    return result;
}

ApplicationDecodeResult
ApplicationTestCodec::DecodeMessage( const std::vector<std::uint8_t>& encoded_message,
                                     const std::optional<std::size_t> storage_capacity )
{
    ApplicationDecodeResult result{};
    result.storage_status = HIL_APPLICATION_Decode_Storage_Size(
        &context_, encoded_message.data(), encoded_message.size(), &result.required_storage_size );
    result.encoded_validation_status = HIL_APPLICATION_Validate_Encoded_Message(
        &context_, encoded_message.data(), encoded_message.size(),
        &result.validation_storage_size );

    decoded_message_      = {};
    decoded_message_.type = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    decode_storage_.fill( 0u );
    result.supplied_storage_size =
        storage_capacity.has_value()
            ? *storage_capacity
            : ( result.storage_status == HIL_APPLICATION_STATUS_OK ? result.required_storage_size
                                                                   : 0u );
    std::uint8_t* const storage =
        result.supplied_storage_size == 0u ? nullptr : decode_storage_.data();
    result.decode_status = HIL_APPLICATION_Decode_Message(
        &context_, encoded_message.data(), encoded_message.size(), &decoded_message_, storage,
        result.supplied_storage_size, &result.used_storage_size );
    return result;
}

const HIL_Application_Message_T& ApplicationTestCodec::DecodedMessage() const
{
    return decoded_message_;
}

const HIL_Application_Config_T& ApplicationTestCodec::Config() const
{
    return config_;
}

HIL_Application_Test_Id_T ApplicationFixtureTestId()
{
    HIL_Application_Test_Id_T                                        test_id{};
    constexpr std::array<std::uint8_t, HIL_APPLICATION_TEST_ID_SIZE> kBytes{
        0x13u, 0x57u, 0x9bu, 0xdfu, 0x24u, 0x68u, 0xacu, 0xe0u,
        0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
    };
    std::copy( kBytes.begin(), kBytes.end(), std::begin( test_id.bytes ) );
    return test_id;
}

HIL_Application_Test_Id_T ApplicationAlternateTestId()
{
    HIL_Application_Test_Id_T                                        test_id{};
    constexpr std::array<std::uint8_t, HIL_APPLICATION_TEST_ID_SIZE> kBytes{
        0x88u, 0x77u, 0x66u, 0x55u, 0x44u, 0x33u, 0x22u, 0x11u,
        0xe0u, 0xacu, 0x68u, 0x24u, 0xdfu, 0x9bu, 0x57u, 0x13u,
    };
    std::copy( kBytes.begin(), kBytes.end(), std::begin( test_id.bytes ) );
    return test_id;
}

HIL_Application_Message_T
MakeApplicationConfigurationMessage( const std::uint8_t* const extension_data,
                                     const std::uint8_t        extension_size )
{
    HIL_Application_Message_T message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id = 1u;
    message.test_id     = ApplicationFixtureTestId();

    auto& config                         = message.body.test_configuration;
    config.tick_duration_us.microseconds = 1000u;
    config.expected_tick_count           = 3u;
    config.flags                         = 0u;

    config.digital_in[2].enabled       = 1u;
    config.digital_in[2].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_24V;

    config.digital_out[7].enabled       = 1u;
    config.digital_out[7].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_5V;
    config.digital_out[7].initial_high  = 1u;

    config.analog_in[1].enabled  = 1u;
    config.analog_out[4].enabled = 1u;

    config.pwm_in[0].enabled       = 1u;
    config.pwm_in[0].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_3V3;

    config.pwm_out[1].enabled                      = 1u;
    config.pwm_out[1].voltage_level                = HIL_APPLICATION_PERIPHERAL_CONFIG_12V;
    config.pwm_out[1].initial_period_nanoseconds   = 50000u;
    config.pwm_out[1].initial_duty_cycle_permyriad = 3750u;

    config.can[0].enabled             = 1u;
    config.can[0].bit_rate            = 500000u;
    config.can[0].termination_enabled = 1u;
    config.can[0].capture_limit_bytes = 64u;

    config.spi[1].enabled             = 1u;
    config.spi[1].bit_rate            = 1000000u;
    config.spi[1].role                = HIL_APPLICATION_BUS_ROLE_MASTER;
    config.spi[1].data_width          = HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS;
    config.spi[1].bit_order           = HIL_APPLICATION_SPI_BIT_ORDER_MSB_FIRST;
    config.spi[1].clock_polarity      = HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_LOW;
    config.spi[1].clock_phase         = HIL_APPLICATION_SPI_CLOCK_PHASE_FIRST_EDGE;
    config.spi[1].capture_limit_bytes = 48u;

    config.uart[0].enabled             = 1u;
    config.uart[0].baud_rate           = 115200u;
    config.uart[0].electrical_mode     = HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3;
    config.uart[0].word_length         = HIL_APPLICATION_UART_WORD_LENGTH_8_BITS;
    config.uart[0].parity              = HIL_APPLICATION_UART_PARITY_NONE;
    config.uart[0].stop_bits           = HIL_APPLICATION_UART_STOP_BITS_1;
    config.uart[0].rx_enabled          = 1u;
    config.uart[0].tx_enabled          = 1u;
    config.uart[0].capture_limit_bytes = 32u;

    config.i2c[1].enabled             = 1u;
    config.i2c[1].bit_rate            = 400000u;
    config.i2c[1].role                = HIL_APPLICATION_BUS_ROLE_MASTER;
    config.i2c[1].own_address_7bit    = 0u;
    config.i2c[1].voltage_level       = HIL_APPLICATION_I2C_VOLTAGE_3V3;
    config.i2c[1].pull_up             = HIL_APPLICATION_I2C_PULL_UP_4K7;
    config.i2c[1].capture_limit_bytes = 16u;

    config.extension_data = HIL_Application_Byte_Span_T{ extension_data, extension_size };
    return message;
}

HIL_Application_Message_T
MakeApplicationInstructionMessage( const std::uint32_t             tick_number,
                                   const HIL_Application_Test_Id_T test_id )
{
    HIL_Application_Message_T message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id = 1u;
    message.test_id     = test_id;

    auto& instruction       = message.body.test_instruction;
    instruction.tick_number = tick_number;
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        instruction.digital_outputs[i].high = static_cast<std::uint8_t>( ( i + tick_number ) % 2u );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
    {
        instruction.analog_outputs[i].microvolts =
            100000u * ( tick_number + 1u ) + static_cast<std::uint32_t>( i * 1234u );
    }
    instruction.pwm_outputs[0].period_nanoseconds = 1000000u + tick_number * 100000u;
    instruction.pwm_outputs[0].duty_cycle_permyriad =
        static_cast<std::uint16_t>( 2500u + tick_number * 1000u );
    instruction.pwm_outputs[1].period_nanoseconds = 2000000u + tick_number * 100000u;
    instruction.pwm_outputs[1].duty_cycle_permyriad =
        static_cast<std::uint16_t>( 7500u - tick_number * 500u );
    return message;
}

HIL_Application_Message_T MakeApplicationResultMessage( const std::uint32_t             tick_number,
                                                        const HIL_Application_Test_Id_T test_id )
{
    HIL_Application_Message_T message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id = 1u;
    message.test_id     = test_id;

    auto& result       = message.body.test_result;
    result.tick_number = tick_number;
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
    {
        result.digital_inputs[i].high = static_cast<std::uint8_t>( ( i + tick_number + 1u ) % 2u );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; ++i )
    {
        result.analog_inputs[i].microvolts =
            200000u * ( tick_number + 1u ) + static_cast<std::uint32_t>( i * 4321u );
    }
    result.pwm_inputs[0].period_nanoseconds = 1500000u + tick_number * 125000u;
    result.pwm_inputs[0].duty_cycle_permyriad =
        static_cast<std::uint16_t>( 3000u + tick_number * 750u );
    result.pwm_inputs[1].period_nanoseconds = 2500000u + tick_number * 125000u;
    result.pwm_inputs[1].duty_cycle_permyriad =
        static_cast<std::uint16_t>( 8000u - tick_number * 600u );

    constexpr std::array<HIL_Application_Result_Condition_T, 3u> kConditions{
        HIL_APPLICATION_RESULT_CONDITION_OK,
        HIL_APPLICATION_RESULT_CONDITION_PARTIAL,
        HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM,
    };
    if ( tick_number < kConditions.size() )
    {
        result.condition = kConditions[tick_number];
    }
    else
    {
        result.condition = HIL_APPLICATION_RESULT_CONDITION_OK;
    }
    result.problem_detail = 0xa5000000u | tick_number;
    return message;
}

HIL_Application_Message_T MakeApplicationStartMessage()
{
    HIL_Application_Message_T message{};
    message.type                           = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    message.subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id                    = 1u;
    message.test_id                        = ApplicationFixtureTestId();
    message.body.execution_control.command = HIL_APPLICATION_CONTROL_START;
    message.body.execution_control.flags   = 0u;
    return message;
}

std::array<std::uint8_t, HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE>
MakeMaximumApplicationConfigurationExtension()
{
    std::array<std::uint8_t, HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE> extension{};
    for ( std::size_t i = 0u; i < extension.size(); ++i )
    {
        extension[i] = static_cast<std::uint8_t>( ( i * 37u + 0x5au ) & 0xffu );
    }
    return extension;
}

bool ApplicationTestIdsEqual( const HIL_Application_Test_Id_T& expected,
                              const HIL_Application_Test_Id_T& actual )
{
    return std::equal( std::begin( expected.bytes ), std::end( expected.bytes ),
                       std::begin( actual.bytes ) );
}

bool ApplicationConfigurationsEqual( const HIL_Application_Message_T& expected,
                                     const HIL_Application_Message_T& actual )
{
    if ( actual.type != HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION
         || actual.subtype != HIL_APPLICATION_MESSAGE_SUBTYPE_NONE || actual.has_test_id != 1u
         || !ApplicationTestIdsEqual( expected.test_id, actual.test_id ) )
    {
        return false;
    }

    const auto& lhs = expected.body.test_configuration;
    const auto& rhs = actual.body.test_configuration;
    if ( lhs.tick_duration_us.microseconds != rhs.tick_duration_us.microseconds
         || lhs.expected_tick_count != rhs.expected_tick_count || lhs.flags != rhs.flags )
    {
        return false;
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.digital_in[i].enabled != rhs.digital_in[i].enabled
             || lhs.digital_in[i].voltage_level != rhs.digital_in[i].voltage_level )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.digital_out[i].enabled != rhs.digital_out[i].enabled
             || lhs.digital_out[i].voltage_level != rhs.digital_out[i].voltage_level
             || lhs.digital_out[i].initial_high != rhs.digital_out[i].initial_high )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.analog_in[i].enabled != rhs.analog_in[i].enabled )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.analog_out[i].enabled != rhs.analog_out[i].enabled )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.pwm_in[i].enabled != rhs.pwm_in[i].enabled
             || lhs.pwm_in[i].voltage_level != rhs.pwm_in[i].voltage_level )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.pwm_out[i].enabled != rhs.pwm_out[i].enabled
             || lhs.pwm_out[i].voltage_level != rhs.pwm_out[i].voltage_level
             || lhs.pwm_out[i].initial_period_nanoseconds
                    != rhs.pwm_out[i].initial_period_nanoseconds
             || lhs.pwm_out[i].initial_duty_cycle_permyriad
                    != rhs.pwm_out[i].initial_duty_cycle_permyriad )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_CAN_CHANNEL_COUNT; ++i )
    {
        if ( lhs.can[i].enabled != rhs.can[i].enabled || lhs.can[i].bit_rate != rhs.can[i].bit_rate
             || lhs.can[i].termination_enabled != rhs.can[i].termination_enabled
             || lhs.can[i].capture_limit_bytes != rhs.can[i].capture_limit_bytes )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_SPI_CHANNEL_COUNT; ++i )
    {
        if ( lhs.spi[i].enabled != rhs.spi[i].enabled || lhs.spi[i].bit_rate != rhs.spi[i].bit_rate
             || lhs.spi[i].role != rhs.spi[i].role || lhs.spi[i].data_width != rhs.spi[i].data_width
             || lhs.spi[i].bit_order != rhs.spi[i].bit_order
             || lhs.spi[i].clock_polarity != rhs.spi[i].clock_polarity
             || lhs.spi[i].clock_phase != rhs.spi[i].clock_phase
             || lhs.spi[i].capture_limit_bytes != rhs.spi[i].capture_limit_bytes )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_UART_CHANNEL_COUNT; ++i )
    {
        if ( lhs.uart[i].enabled != rhs.uart[i].enabled
             || lhs.uart[i].baud_rate != rhs.uart[i].baud_rate
             || lhs.uart[i].electrical_mode != rhs.uart[i].electrical_mode
             || lhs.uart[i].word_length != rhs.uart[i].word_length
             || lhs.uart[i].parity != rhs.uart[i].parity
             || lhs.uart[i].stop_bits != rhs.uart[i].stop_bits
             || lhs.uart[i].rx_enabled != rhs.uart[i].rx_enabled
             || lhs.uart[i].tx_enabled != rhs.uart[i].tx_enabled
             || lhs.uart[i].capture_limit_bytes != rhs.uart[i].capture_limit_bytes )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_I2C_CHANNEL_COUNT; ++i )
    {
        if ( lhs.i2c[i].enabled != rhs.i2c[i].enabled || lhs.i2c[i].bit_rate != rhs.i2c[i].bit_rate
             || lhs.i2c[i].role != rhs.i2c[i].role
             || lhs.i2c[i].own_address_7bit != rhs.i2c[i].own_address_7bit
             || lhs.i2c[i].voltage_level != rhs.i2c[i].voltage_level
             || lhs.i2c[i].pull_up != rhs.i2c[i].pull_up
             || lhs.i2c[i].capture_limit_bytes != rhs.i2c[i].capture_limit_bytes )
        {
            return false;
        }
    }
    if ( lhs.extension_data.size != rhs.extension_data.size )
    {
        return false;
    }
    if ( lhs.extension_data.size == 0u )
    {
        return true;
    }
    if ( lhs.extension_data.data == nullptr || rhs.extension_data.data == nullptr )
    {
        return false;
    }
    return std::equal( lhs.extension_data.data, lhs.extension_data.data + lhs.extension_data.size,
                       rhs.extension_data.data );
}

bool ApplicationInstructionsEqual( const HIL_Application_Message_T& expected,
                                   const HIL_Application_Message_T& actual )
{
    if ( actual.type != HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION
         || actual.subtype != HIL_APPLICATION_MESSAGE_SUBTYPE_NONE || actual.has_test_id != 1u
         || !ApplicationTestIdsEqual( expected.test_id, actual.test_id ) )
    {
        return false;
    }
    const auto& lhs = expected.body.test_instruction;
    const auto& rhs = actual.body.test_instruction;
    if ( lhs.tick_number != rhs.tick_number )
    {
        return false;
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.digital_outputs[i].high != rhs.digital_outputs[i].high )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.analog_outputs[i].microvolts != rhs.analog_outputs[i].microvolts )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.pwm_outputs[i].period_nanoseconds != rhs.pwm_outputs[i].period_nanoseconds
             || lhs.pwm_outputs[i].duty_cycle_permyriad != rhs.pwm_outputs[i].duty_cycle_permyriad )
        {
            return false;
        }
    }
    return true;
}

bool ApplicationResultsEqual( const HIL_Application_Message_T& expected,
                              const HIL_Application_Message_T& actual )
{
    if ( actual.type != HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT
         || actual.subtype != HIL_APPLICATION_MESSAGE_SUBTYPE_NONE || actual.has_test_id != 1u
         || !ApplicationTestIdsEqual( expected.test_id, actual.test_id ) )
    {
        return false;
    }
    const auto& lhs = expected.body.test_result;
    const auto& rhs = actual.body.test_result;
    if ( lhs.tick_number != rhs.tick_number || lhs.condition != rhs.condition
         || lhs.problem_detail != rhs.problem_detail )
    {
        return false;
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.digital_inputs[i].high != rhs.digital_inputs[i].high )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.analog_inputs[i].microvolts != rhs.analog_inputs[i].microvolts )
        {
            return false;
        }
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
    {
        if ( lhs.pwm_inputs[i].period_nanoseconds != rhs.pwm_inputs[i].period_nanoseconds
             || lhs.pwm_inputs[i].duty_cycle_permyriad != rhs.pwm_inputs[i].duty_cycle_permyriad )
        {
            return false;
        }
    }
    return true;
}

}  // namespace hil_rig_protocol::test
