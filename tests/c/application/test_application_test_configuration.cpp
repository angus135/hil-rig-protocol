#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"

namespace {
constexpr std::size_t kHeaderSize            = 23u;
constexpr std::size_t kFixedPayloadSize      = 197u;
constexpr std::size_t kEmptyCompleteSize     = 220u;
constexpr std::size_t kDigitalInputOffset    = 12u;
constexpr std::size_t kDigitalOutputOffset   = 32u;
constexpr std::size_t kAnalogInputOffset     = 62u;
constexpr std::size_t kAnalogOutputOffset    = 64u;
constexpr std::size_t kPwmInputOffset        = 70u;
constexpr std::size_t kPwmOutputOffset       = 74u;
constexpr std::size_t kCanOffset             = 90u;
constexpr std::size_t kSpiOffset             = 110u;
constexpr std::size_t kUartOffset            = 138u;
constexpr std::size_t kI2cOffset             = 168u;
constexpr std::size_t kExtensionLengthOffset = 196u;
constexpr std::size_t kExtensionDataOffset   = 197u;
constexpr std::size_t kPayloadLengthOffset   = 21u;

void PutU16Le( std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value )
{
    bytes[offset]     = static_cast<std::uint8_t>( value & 0xffu );
    bytes[offset + 1] = static_cast<std::uint8_t>( ( value >> 8u ) & 0xffu );
}

void PutU32Le( std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value )
{
    bytes[offset]     = static_cast<std::uint8_t>( value & 0xffu );
    bytes[offset + 1] = static_cast<std::uint8_t>( ( value >> 8u ) & 0xffu );
    bytes[offset + 2] = static_cast<std::uint8_t>( ( value >> 16u ) & 0xffu );
    bytes[offset + 3] = static_cast<std::uint8_t>( ( value >> 24u ) & 0xffu );
}

HIL_Application_Test_Id_T TestId()
{
    HIL_Application_Test_Id_T id{};
    for ( std::size_t i = 0u; i < HIL_APPLICATION_TEST_ID_SIZE; ++i )
    {
        id.bytes[i] = static_cast<std::uint8_t>( 0xa0u + i );
    }
    return id;
}

HIL_Application_Context_T
MakeContext( std::size_t   max_message  = HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE,
             std::size_t   max_variable = 255u,
             std::uint32_t max_ticks    = HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};
    EXPECT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_encoded_message_size = max_message;
    config.max_variable_data_size   = max_variable;
    config.max_expected_tick_count  = max_ticks;
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    return context;
}

HIL_Application_Test_Configuration_T CanonicalConfiguration()
{
    HIL_Application_Test_Configuration_T config{};
    config.tick_duration_us.microseconds = 1000u;
    config.expected_tick_count           = 1u;
    config.flags                         = 0u;
    config.extension_data                = HIL_Application_Byte_Span_T{ nullptr, 0u };
    return config;
}

HIL_Application_Test_Configuration_T
RepresentativeConfiguration( const std::uint8_t* extension_data = nullptr,
                             std::uint8_t        extension_size = 0u )
{
    auto config                = CanonicalConfiguration();
    config.expected_tick_count = 0x00010203u;

    config.digital_in[3].enabled       = 1u;
    config.digital_in[3].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_24V;

    config.digital_out[4].enabled       = 1u;
    config.digital_out[4].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_5V;
    config.digital_out[4].initial_high  = 1u;

    config.analog_in[1].enabled  = 1u;
    config.analog_out[5].enabled = 1u;

    config.pwm_in[1].enabled       = 1u;
    config.pwm_in[1].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_3V3;

    config.pwm_out[0].enabled                      = 1u;
    config.pwm_out[0].voltage_level                = HIL_APPLICATION_PERIPHERAL_CONFIG_12V;
    config.pwm_out[0].initial_period_nanoseconds   = 0x11223344u;
    config.pwm_out[0].initial_duty_cycle_permyriad = 10000u;

    config.can[1].enabled             = 1u;
    config.can[1].bit_rate            = 500000u;
    config.can[1].termination_enabled = 1u;
    config.can[1].capture_limit_bytes = 0x5au;

    config.spi[0].enabled             = 1u;
    config.spi[0].bit_rate            = 0x01020304u;
    config.spi[0].role                = HIL_APPLICATION_BUS_ROLE_SLAVE;
    config.spi[0].data_width          = HIL_APPLICATION_SPI_DATA_WIDTH_16_BITS;
    config.spi[0].bit_order           = HIL_APPLICATION_SPI_BIT_ORDER_LSB_FIRST;
    config.spi[0].clock_polarity      = HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_HIGH;
    config.spi[0].clock_phase         = HIL_APPLICATION_SPI_CLOCK_PHASE_SECOND_EDGE;
    config.spi[0].capture_limit_bytes = 0x22u;

    config.uart[1].enabled             = 1u;
    config.uart[1].baud_rate           = 115200u;
    config.uart[1].electrical_mode     = HIL_APPLICATION_UART_ELECTRICAL_MODE_RS232;
    config.uart[1].word_length         = HIL_APPLICATION_UART_WORD_LENGTH_9_BITS;
    config.uart[1].parity              = HIL_APPLICATION_UART_PARITY_ODD;
    config.uart[1].stop_bits           = HIL_APPLICATION_UART_STOP_BITS_2;
    config.uart[1].rx_enabled          = 1u;
    config.uart[1].tx_enabled          = 0u;
    config.uart[1].capture_limit_bytes = 0x44u;

    config.i2c[0].enabled             = 1u;
    config.i2c[0].bit_rate            = 400000u;
    config.i2c[0].role                = HIL_APPLICATION_BUS_ROLE_SLAVE;
    config.i2c[0].own_address_7bit    = 0x52u;
    config.i2c[0].voltage_level       = HIL_APPLICATION_I2C_VOLTAGE_5V;
    config.i2c[0].pull_up             = HIL_APPLICATION_I2C_PULL_UP_4K7;
    config.i2c[0].capture_limit_bytes = 0x33u;

    config.extension_data = HIL_Application_Byte_Span_T{ extension_data, extension_size };
    return config;
}

HIL_Application_Test_Configuration_T AllChannelsEnabledConfiguration()
{
    auto config                          = CanonicalConfiguration();
    config.tick_duration_us.microseconds = 10u;
    config.expected_tick_count           = HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT;

    for ( auto& record : config.digital_in )
    {
        record.enabled       = 1u;
        record.voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_24V;
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        config.digital_out[i].enabled       = 1u;
        config.digital_out[i].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_12V;
        config.digital_out[i].initial_high  = static_cast<std::uint8_t>( i & 1u );
    }
    for ( auto& record : config.analog_in )
        record.enabled = 1u;
    for ( auto& record : config.analog_out )
        record.enabled = 1u;
    for ( auto& record : config.pwm_in )
    {
        record.enabled       = 1u;
        record.voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_5V;
    }
    config.pwm_out[0].enabled                      = 1u;
    config.pwm_out[0].voltage_level                = HIL_APPLICATION_PERIPHERAL_CONFIG_24V;
    config.pwm_out[0].initial_period_nanoseconds   = 0u;
    config.pwm_out[0].initial_duty_cycle_permyriad = 0u;
    config.pwm_out[1].enabled                      = 1u;
    config.pwm_out[1].voltage_level                = HIL_APPLICATION_PERIPHERAL_CONFIG_24V;
    config.pwm_out[1].initial_period_nanoseconds   = UINT32_MAX;
    config.pwm_out[1].initial_duty_cycle_permyriad = 10000u;
    for ( std::size_t i = 0u; i < HIL_APPLICATION_CAN_CHANNEL_COUNT; ++i )
    {
        config.can[i].enabled             = 1u;
        config.can[i].bit_rate            = UINT32_MAX;
        config.can[i].termination_enabled = static_cast<std::uint8_t>( i & 1u );
        config.can[i].capture_limit_bytes = 255u;
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_SPI_CHANNEL_COUNT; ++i )
    {
        config.spi[i].enabled  = 1u;
        config.spi[i].bit_rate = UINT32_MAX;
        config.spi[i].role =
            i == 0u ? HIL_APPLICATION_BUS_ROLE_MASTER : HIL_APPLICATION_BUS_ROLE_SLAVE;
        config.spi[i].data_width          = i == 0u ? HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS
                                                    : HIL_APPLICATION_SPI_DATA_WIDTH_16_BITS;
        config.spi[i].bit_order           = i == 0u ? HIL_APPLICATION_SPI_BIT_ORDER_MSB_FIRST
                                                    : HIL_APPLICATION_SPI_BIT_ORDER_LSB_FIRST;
        config.spi[i].clock_polarity      = i == 0u ? HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_LOW
                                                    : HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_HIGH;
        config.spi[i].clock_phase         = i == 0u ? HIL_APPLICATION_SPI_CLOCK_PHASE_FIRST_EDGE
                                                    : HIL_APPLICATION_SPI_CLOCK_PHASE_SECOND_EDGE;
        config.spi[i].capture_limit_bytes = 255u;
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_UART_CHANNEL_COUNT; ++i )
    {
        config.uart[i].enabled         = 1u;
        config.uart[i].baud_rate       = UINT32_MAX;
        config.uart[i].electrical_mode = i == 0u ? HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3
                                                 : HIL_APPLICATION_UART_ELECTRICAL_MODE_RS232;
        config.uart[i].word_length     = i == 0u ? HIL_APPLICATION_UART_WORD_LENGTH_8_BITS
                                                 : HIL_APPLICATION_UART_WORD_LENGTH_9_BITS;
        config.uart[i].parity =
            i == 0u ? HIL_APPLICATION_UART_PARITY_NONE : HIL_APPLICATION_UART_PARITY_EVEN;
        config.uart[i].stop_bits =
            i == 0u ? HIL_APPLICATION_UART_STOP_BITS_1 : HIL_APPLICATION_UART_STOP_BITS_2;
        config.uart[i].rx_enabled          = i == 0u ? 0u : 1u;
        config.uart[i].tx_enabled          = 1u;
        config.uart[i].capture_limit_bytes = i == 0u ? 0u : 255u;
    }
    config.i2c[0].enabled             = 1u;
    config.i2c[0].bit_rate            = UINT32_MAX;
    config.i2c[0].role                = HIL_APPLICATION_BUS_ROLE_MASTER;
    config.i2c[0].own_address_7bit    = 0u;
    config.i2c[0].voltage_level       = HIL_APPLICATION_I2C_VOLTAGE_3V3;
    config.i2c[0].pull_up             = HIL_APPLICATION_I2C_PULL_UP_1K;
    config.i2c[0].capture_limit_bytes = 255u;
    config.i2c[1].enabled             = 1u;
    config.i2c[1].bit_rate            = UINT32_MAX;
    config.i2c[1].role                = HIL_APPLICATION_BUS_ROLE_SLAVE;
    config.i2c[1].own_address_7bit    = 0x7fu;
    config.i2c[1].voltage_level       = HIL_APPLICATION_I2C_VOLTAGE_5V;
    config.i2c[1].pull_up             = HIL_APPLICATION_I2C_PULL_UP_10K;
    config.i2c[1].capture_limit_bytes = 255u;
    return config;
}

HIL_Application_Message_T ConfigurationMessage( const HIL_Application_Test_Configuration_T& config )
{
    HIL_Application_Message_T message{};
    message.type                    = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    message.subtype                 = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id             = 1u;
    message.test_id                 = TestId();
    message.body.test_configuration = config;
    return message;
}

std::vector<std::uint8_t> EncodeConfiguration( const HIL_Application_Context_T&            context,
                                               const HIL_Application_Test_Configuration_T& config )
{
    const auto                     message = ConfigurationMessage( config );
    std::array<std::uint8_t, 512u> encoded{};
    std::size_t                    used = 0u;
    EXPECT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(), &used ),
        HIL_APPLICATION_STATUS_OK );
    return std::vector<std::uint8_t>( encoded.begin(),
                                      encoded.begin() + static_cast<std::ptrdiff_t>( used ) );
}

std::vector<std::uint8_t> EmptyGolden()
{
    std::vector<std::uint8_t> expected( kEmptyCompleteSize, 0u );
    expected[0] = 0u;
    expected[1] = 1u;
    expected[2] = 1u;
    for ( std::size_t i = 0u; i < 16u; ++i )
        expected[3u + i] = static_cast<std::uint8_t>( 0xa0u + i );
    expected[19] = 0x10u;
    expected[20] = 0x00u;
    expected[21] = 0xc5u;
    expected[22] = 0x00u;
    PutU32Le( expected, kHeaderSize + 0u, 1000u );
    PutU32Le( expected, kHeaderSize + 4u, 1u );
    PutU32Le( expected, kHeaderSize + 8u, 0u );
    expected[kHeaderSize + kExtensionLengthOffset] = 0u;
    return expected;
}

std::vector<std::uint8_t> RepresentativeGolden( const std::array<std::uint8_t, 3u>& extension )
{
    auto expected = EmptyGolden();
    expected.resize( kEmptyCompleteSize + extension.size(), 0u );
    PutU16Le( expected, kPayloadLengthOffset,
              static_cast<std::uint16_t>( kFixedPayloadSize + extension.size() ) );
    const std::size_t p = kHeaderSize;
    PutU32Le( expected, p + 4u, 0x00010203u );

    expected[p + kDigitalInputOffset + 3u * 2u + 0u]  = 1u;
    expected[p + kDigitalInputOffset + 3u * 2u + 1u]  = 4u;
    expected[p + kDigitalOutputOffset + 4u * 3u + 0u] = 1u;
    expected[p + kDigitalOutputOffset + 4u * 3u + 1u] = 2u;
    expected[p + kDigitalOutputOffset + 4u * 3u + 2u] = 1u;
    expected[p + kAnalogInputOffset + 1u]             = 1u;
    expected[p + kAnalogOutputOffset + 5u]            = 1u;
    expected[p + kPwmInputOffset + 1u * 2u + 0u]      = 1u;
    expected[p + kPwmInputOffset + 1u * 2u + 1u]      = 1u;

    const std::size_t pwm = p + kPwmOutputOffset;
    expected[pwm + 0u]    = 1u;
    expected[pwm + 1u]    = 3u;
    PutU32Le( expected, pwm + 2u, 0x11223344u );
    PutU16Le( expected, pwm + 6u, 10000u );

    const std::size_t can = p + kCanOffset + 10u;
    expected[can + 0u]    = 1u;
    PutU32Le( expected, can + 1u, 500000u );
    expected[can + 5u] = 1u;
    PutU32Le( expected, can + 6u, 0x5au );

    const std::size_t                   spi = p + kSpiOffset;
    const std::array<std::uint8_t, 14u> spi_bytes{ 1u, 4u, 3u, 2u,    1u, 2u, 2u,
                                                   2u, 2u, 2u, 0x22u, 0u, 0u, 0u };
    for ( std::size_t i = 0u; i < spi_bytes.size(); ++i )
        expected[spi + i] = spi_bytes[i];

    const std::size_t                   uart = p + kUartOffset + 15u;
    const std::array<std::uint8_t, 15u> uart_bytes{ 1u, 0x00u, 0xc2u, 0x01u, 0x00u, 3u, 2u, 3u,
                                                    2u, 1u,    0u,    0x44u, 0u,    0u, 0u };
    for ( std::size_t i = 0u; i < uart_bytes.size(); ++i )
        expected[uart + i] = uart_bytes[i];

    const std::size_t                   i2c = p + kI2cOffset;
    const std::array<std::uint8_t, 14u> i2c_bytes{ 1u,    0x80u, 0x1au, 0x06u, 0x00u, 2u, 0x52u,
                                                   0x00u, 2u,    3u,    0x33u, 0u,    0u, 0u };
    for ( std::size_t i = 0u; i < i2c_bytes.size(); ++i )
        expected[i2c + i] = i2c_bytes[i];

    expected[p + kExtensionLengthOffset] = static_cast<std::uint8_t>( extension.size() );
    for ( std::size_t i = 0u; i < extension.size(); ++i )
        expected[p + kExtensionDataOffset + i] = extension[i];
    return expected;
}

void ExpectConfigurationEqual( const HIL_Application_Test_Configuration_T& expected,
                               const HIL_Application_Test_Configuration_T& actual )
{
    EXPECT_EQ( actual.tick_duration_us.microseconds, expected.tick_duration_us.microseconds );
    EXPECT_EQ( actual.expected_tick_count, expected.expected_tick_count );
    EXPECT_EQ( actual.flags, expected.flags );
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.digital_in[i].enabled, expected.digital_in[i].enabled );
        EXPECT_EQ( actual.digital_in[i].voltage_level, expected.digital_in[i].voltage_level );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.digital_out[i].enabled, expected.digital_out[i].enabled );
        EXPECT_EQ( actual.digital_out[i].voltage_level, expected.digital_out[i].voltage_level );
        EXPECT_EQ( actual.digital_out[i].initial_high, expected.digital_out[i].initial_high );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; ++i )
        EXPECT_EQ( actual.analog_in[i].enabled, expected.analog_in[i].enabled );
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
        EXPECT_EQ( actual.analog_out[i].enabled, expected.analog_out[i].enabled );
    for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.pwm_in[i].enabled, expected.pwm_in[i].enabled );
        EXPECT_EQ( actual.pwm_in[i].voltage_level, expected.pwm_in[i].voltage_level );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.pwm_out[i].enabled, expected.pwm_out[i].enabled );
        EXPECT_EQ( actual.pwm_out[i].voltage_level, expected.pwm_out[i].voltage_level );
        EXPECT_EQ( actual.pwm_out[i].initial_period_nanoseconds,
                   expected.pwm_out[i].initial_period_nanoseconds );
        EXPECT_EQ( actual.pwm_out[i].initial_duty_cycle_permyriad,
                   expected.pwm_out[i].initial_duty_cycle_permyriad );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_CAN_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.can[i].enabled, expected.can[i].enabled );
        EXPECT_EQ( actual.can[i].bit_rate, expected.can[i].bit_rate );
        EXPECT_EQ( actual.can[i].termination_enabled, expected.can[i].termination_enabled );
        EXPECT_EQ( actual.can[i].capture_limit_bytes, expected.can[i].capture_limit_bytes );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_SPI_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.spi[i].enabled, expected.spi[i].enabled );
        EXPECT_EQ( actual.spi[i].bit_rate, expected.spi[i].bit_rate );
        EXPECT_EQ( actual.spi[i].role, expected.spi[i].role );
        EXPECT_EQ( actual.spi[i].data_width, expected.spi[i].data_width );
        EXPECT_EQ( actual.spi[i].bit_order, expected.spi[i].bit_order );
        EXPECT_EQ( actual.spi[i].clock_polarity, expected.spi[i].clock_polarity );
        EXPECT_EQ( actual.spi[i].clock_phase, expected.spi[i].clock_phase );
        EXPECT_EQ( actual.spi[i].capture_limit_bytes, expected.spi[i].capture_limit_bytes );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_UART_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.uart[i].enabled, expected.uart[i].enabled );
        EXPECT_EQ( actual.uart[i].baud_rate, expected.uart[i].baud_rate );
        EXPECT_EQ( actual.uart[i].electrical_mode, expected.uart[i].electrical_mode );
        EXPECT_EQ( actual.uart[i].word_length, expected.uart[i].word_length );
        EXPECT_EQ( actual.uart[i].parity, expected.uart[i].parity );
        EXPECT_EQ( actual.uart[i].stop_bits, expected.uart[i].stop_bits );
        EXPECT_EQ( actual.uart[i].rx_enabled, expected.uart[i].rx_enabled );
        EXPECT_EQ( actual.uart[i].tx_enabled, expected.uart[i].tx_enabled );
        EXPECT_EQ( actual.uart[i].capture_limit_bytes, expected.uart[i].capture_limit_bytes );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_I2C_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.i2c[i].enabled, expected.i2c[i].enabled );
        EXPECT_EQ( actual.i2c[i].bit_rate, expected.i2c[i].bit_rate );
        EXPECT_EQ( actual.i2c[i].role, expected.i2c[i].role );
        EXPECT_EQ( actual.i2c[i].own_address_7bit, expected.i2c[i].own_address_7bit );
        EXPECT_EQ( actual.i2c[i].voltage_level, expected.i2c[i].voltage_level );
        EXPECT_EQ( actual.i2c[i].pull_up, expected.i2c[i].pull_up );
        EXPECT_EQ( actual.i2c[i].capture_limit_bytes, expected.i2c[i].capture_limit_bytes );
    }
    EXPECT_EQ( actual.extension_data.size, expected.extension_data.size );
    for ( std::size_t i = 0u; i < expected.extension_data.size; ++i )
        EXPECT_EQ( actual.extension_data.data[i], expected.extension_data.data[i] );
}

void ExpectRoundTrip( const HIL_Application_Context_T&            context,
                      const HIL_Application_Test_Configuration_T& config )
{
    const auto                     first = EncodeConfiguration( context, config );
    std::array<std::uint8_t, 255u> storage{};
    HIL_Application_Message_T      decoded{};
    std::size_t                    required = 99u;
    ASSERT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, first.data(), first.size(), &required ),
        HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( required, config.extension_data.size );
    std::size_t used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, first.data(), first.size(), &decoded,
                                               required == 0u ? nullptr : storage.data(), required,
                                               &used ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( used, required );
    ASSERT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION );
    ExpectConfigurationEqual( config, decoded.body.test_configuration );
    const auto second = EncodeConfiguration( context, decoded.body.test_configuration );
    EXPECT_EQ( second, first );
}

void ExpectValidationFailure( const HIL_Application_Context_T&            context,
                              const HIL_Application_Test_Configuration_T& config )
{
    const auto message = ConfigurationMessage( config );
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    std::array<std::uint8_t, 512u> output{};
    std::size_t                    used = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, output.data(), output.size(), &used ),
        HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( used, 0u );
}

void ExpectDecodeFailure( const HIL_Application_Context_T& context,
                          const std::vector<std::uint8_t>& bytes,
                          HIL_Application_Status_T         expected_status )
{
    HIL_Application_Message_T decoded{};
    decoded.type = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    std::array<std::uint8_t, 255u> storage{};
    std::size_t                    used = 77u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, bytes.data(), bytes.size(), &decoded,
                                               storage.data(), storage.size(), &used ),
               expected_status );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );
}
}  // namespace

TEST( ApplicationTestConfigurationGolden, CanonicalAllDisabledBytesAreExact )
{
    const auto context = MakeContext();
    const auto actual  = EncodeConfiguration( context, CanonicalConfiguration() );
    EXPECT_EQ( actual, EmptyGolden() );
}

TEST( ApplicationTestConfigurationGolden, RepresentativeEnabledRecordsHaveExactOffsetsAndEndian )
{
    const std::array<std::uint8_t, 3u> extension{ 0xaau, 0x55u, 0x10u };
    const auto                         context = MakeContext();
    const auto                         config  = RepresentativeConfiguration(
        extension.data(), static_cast<std::uint8_t>( extension.size() ) );
    const auto actual   = EncodeConfiguration( context, config );
    const auto expected = RepresentativeGolden( extension );
    ASSERT_EQ( actual.size(), kEmptyCompleteSize + extension.size() );
    EXPECT_EQ( actual, expected );

    /* Keep explicit offset checks alongside the full independent golden vector. */
    const std::size_t p = kHeaderSize;
    EXPECT_EQ( actual[p + kDigitalInputOffset + 3u * 2u], 1u );
    EXPECT_EQ( actual[p + kDigitalOutputOffset + 4u * 3u], 1u );
    EXPECT_EQ( actual[p + kAnalogInputOffset + 1u], 1u );
    EXPECT_EQ( actual[p + kAnalogOutputOffset + 5u], 1u );
    EXPECT_EQ( actual[p + kPwmInputOffset + 2u], 1u );
    EXPECT_EQ( actual[p + kPwmOutputOffset], 1u );
    EXPECT_EQ( actual[p + kCanOffset + 10u], 1u );
    EXPECT_EQ( actual[p + kSpiOffset], 1u );
    EXPECT_EQ( actual[p + kUartOffset + 15u], 1u );
    EXPECT_EQ( actual[p + kI2cOffset], 1u );
    EXPECT_EQ( actual[p + kExtensionLengthOffset], extension.size() );
}

TEST( ApplicationTestConfigurationSize, EmptyNonemptyAndMaximumExtensionsHaveExactCompleteSizes )
{
    auto                           context = MakeContext();
    std::array<std::uint8_t, 255u> extension{};
    for ( const std::size_t n : { 0u, 7u, 255u } )
    {
        auto config           = CanonicalConfiguration();
        config.extension_data = HIL_Application_Byte_Span_T{ n == 0u ? nullptr : extension.data(),
                                                             static_cast<std::uint8_t>( n ) };
        auto        message = ConfigurationMessage( config );
        std::size_t size    = 99u;
        ASSERT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &size ),
                   HIL_APPLICATION_STATUS_OK );
        EXPECT_EQ( size, kEmptyCompleteSize + n );
    }
}

TEST( ApplicationTestConfigurationSize, ExactCapacitySucceedsAndOneByteShortClearsOutput )
{
    auto       context = MakeContext( kEmptyCompleteSize );
    const auto message = ConfigurationMessage( CanonicalConfiguration() );
    std::array<std::uint8_t, kEmptyCompleteSize> exact{};
    std::size_t                                  used = 99u;
    ASSERT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, exact.data(), exact.size(), &used ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, kEmptyCompleteSize );

    std::array<std::uint8_t, kEmptyCompleteSize - 1u> short_buffer{};
    used = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, short_buffer.data(),
                                               short_buffer.size(), &used ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( used, 0u );

    auto        short_context = MakeContext( kEmptyCompleteSize - 1u );
    std::size_t size          = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &short_context, &message, &size ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( size, 0u );
}

TEST( ApplicationTestConfigurationRoundTrip, AllDisabledIsStable )
{
    ExpectRoundTrip( MakeContext(), CanonicalConfiguration() );
}

TEST( ApplicationTestConfigurationRoundTrip, OneEnabledChannelFromEveryFamilyIsStable )
{
    const std::array<std::uint8_t, 4u> extension{ 1u, 2u, 3u, 4u };
    ExpectRoundTrip( MakeContext(),
                     RepresentativeConfiguration( extension.data(),
                                                  static_cast<std::uint8_t>( extension.size() ) ) );
}

TEST( ApplicationTestConfigurationRoundTrip, AllChannelsAndBoundaryValuesAreStable )
{
    ExpectRoundTrip( MakeContext(), AllChannelsEnabledConfiguration() );
}

TEST( ApplicationTestConfigurationDecodeStorage, ExactStorageSucceedsAndOneByteShortFailsCleanly )
{
    const std::array<std::uint8_t, 5u> extension{ 9u, 8u, 7u, 6u, 5u };
    const auto                         context = MakeContext();
    const auto                         bytes   = EncodeConfiguration(
        context, RepresentativeConfiguration( extension.data(),
                                                                        static_cast<std::uint8_t>( extension.size() ) ) );
    std::size_t required = 99u;
    ASSERT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, bytes.data(), bytes.size(), &required ),
        HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( required, extension.size() );

    HIL_Application_Message_T    decoded{};
    std::array<std::uint8_t, 5u> exact{};
    std::size_t                  used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, bytes.data(), bytes.size(), &decoded,
                                               exact.data(), exact.size(), &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, extension.size() );

    decoded.type = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    used         = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, bytes.data(), bytes.size(), &decoded,
                                               exact.data(), exact.size() - 1u, &used ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );
}

TEST( ApplicationTestConfigurationDecodeStorage, EncodedValidationSupportsNonemptyExtension )
{
    const std::array<std::uint8_t, 3u> extension{ 1u, 2u, 3u };
    const auto                         context = MakeContext();
    const auto                         bytes   = EncodeConfiguration(
        context, RepresentativeConfiguration( extension.data(),
                                                                        static_cast<std::uint8_t>( extension.size() ) ) );
    std::size_t required = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Validate_Encoded_Message( &context, bytes.data(), bytes.size(), &required ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required, extension.size() );
}

TEST( ApplicationTestConfigurationExtensionLimit, ExactConfiguredLimitWorksAcrossFacade )
{
    constexpr std::size_t              kConfiguredLimit = 4u;
    const std::array<std::uint8_t, 4u> extension{ 0x11u, 0x22u, 0x33u, 0x44u };
    const auto context = MakeContext( HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE, kConfiguredLimit );
    auto       config  = CanonicalConfiguration();
    config.extension_data = HIL_Application_Byte_Span_T{
        extension.data(), static_cast<std::uint8_t>( extension.size() ) };
    const auto message = ConfigurationMessage( config );

    ASSERT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );

    std::array<std::uint8_t, 512u> encoded{};
    std::size_t                    encoded_size = 99u;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_size, kEmptyCompleteSize + extension.size() );

    std::size_t required = 99u;
    ASSERT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, encoded.data(), encoded_size, &required ),
        HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( required, kConfiguredLimit );

    HIL_Application_Message_T    decoded{};
    std::array<std::uint8_t, 4u> decode_storage{};
    std::size_t                  used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded,
                                               decode_storage.data(), decode_storage.size(),
                                               &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, kConfiguredLimit );
    ExpectConfigurationEqual( config, decoded.body.test_configuration );

    required = 99u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, encoded.data(), encoded_size,
                                                         &required ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required, kConfiguredLimit );
}

TEST( ApplicationTestConfigurationExtensionLimit, OneByteOverConfiguredLimitFailsTypedAndEncode )
{
    constexpr std::size_t              kConfiguredLimit = 4u;
    const std::array<std::uint8_t, 5u> extension{ 1u, 2u, 3u, 4u, 5u };
    const auto context = MakeContext( HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE, kConfiguredLimit );
    auto       config  = CanonicalConfiguration();
    config.extension_data = HIL_Application_Byte_Span_T{
        extension.data(), static_cast<std::uint8_t>( extension.size() ) };
    const auto message = ConfigurationMessage( config );

    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );

    /* Encoded_Size intentionally sizes the typed body without performing full body validation. */
    std::size_t encoded_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, kEmptyCompleteSize + extension.size() );

    std::array<std::uint8_t, 512u> encoded{};
    encoded_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( encoded_size, 0u );
}

TEST( ApplicationTestConfigurationExtensionLimit,
      RestrictiveContextRejectsMessageEncodedByPermissiveContext )
{
    constexpr std::size_t              kRestrictiveLimit = 4u;
    const std::array<std::uint8_t, 5u> extension{ 9u, 8u, 7u, 6u, 5u };
    const auto                         permissive_context = MakeContext();
    const auto                         restrictive_context =
        MakeContext( HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE, kRestrictiveLimit );
    auto config           = CanonicalConfiguration();
    config.extension_data = HIL_Application_Byte_Span_T{
        extension.data(), static_cast<std::uint8_t>( extension.size() ) };
    const auto encoded = EncodeConfiguration( permissive_context, config );

    std::size_t required = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &restrictive_context, encoded.data(),
                                                    encoded.size(), &required ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( required, 0u );

    HIL_Application_Message_T    decoded{};
    std::array<std::uint8_t, 5u> decode_storage{};
    decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    std::size_t used = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &restrictive_context, encoded.data(), encoded.size(),
                                               &decoded, decode_storage.data(),
                                               decode_storage.size(), &used ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );

    required = 99u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &restrictive_context, encoded.data(),
                                                         encoded.size(), &required ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( required, 0u );
}

TEST( ApplicationTestConfigurationExtensionLimit, Maximum255ByteExtensionWorksEndToEnd )
{
    std::array<std::uint8_t, 255u> extension{};
    for ( std::size_t i = 0u; i < extension.size(); ++i )
    {
        extension[i] = static_cast<std::uint8_t>( i );
    }

    const auto context    = MakeContext();
    auto       config     = CanonicalConfiguration();
    config.extension_data = HIL_Application_Byte_Span_T{
        extension.data(), static_cast<std::uint8_t>( extension.size() ) };
    const auto message = ConfigurationMessage( config );

    std::size_t encoded_size = 99u;
    ASSERT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_size, 475u );

    std::array<std::uint8_t, 475u> encoded{};
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_size, encoded.size() );

    std::size_t required = 99u;
    ASSERT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, encoded.data(), encoded.size(), &required ),
        HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( required, extension.size() );

    HIL_Application_Message_T      decoded{};
    std::array<std::uint8_t, 255u> decode_storage{};
    std::size_t                    used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded.size(), &decoded,
                                               decode_storage.data(), decode_storage.size(),
                                               &used ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( used, extension.size() );
    ExpectConfigurationEqual( config, decoded.body.test_configuration );

    required = 99u;
    ASSERT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, encoded.data(), encoded.size(),
                                                         &required ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( required, extension.size() );

    const auto reencoded = EncodeConfiguration( context, decoded.body.test_configuration );
    EXPECT_TRUE( std::equal( encoded.begin(), encoded.end(), reencoded.begin(), reencoded.end() ) );
    EXPECT_EQ( reencoded.size(), encoded.size() );
}

TEST( ApplicationTestConfigurationValidation, RejectsInvalidEnableAndNoncanonicalDisabledRecords )
{
    const auto context = MakeContext();
    const auto reject  = [&]( auto mutate ) {
        auto config = CanonicalConfiguration();
        mutate( config );
        ExpectValidationFailure( context, config );
    };

    reject( []( auto& c ) { c.digital_in[0].enabled = 2u; } );
    reject( []( auto& c ) { c.digital_out[0].enabled = 2u; } );
    reject( []( auto& c ) { c.analog_in[0].enabled = 2u; } );
    reject( []( auto& c ) { c.analog_out[0].enabled = 2u; } );
    reject( []( auto& c ) { c.pwm_in[0].enabled = 2u; } );
    reject( []( auto& c ) { c.pwm_out[0].enabled = 2u; } );
    reject( []( auto& c ) { c.can[0].enabled = 2u; } );
    reject( []( auto& c ) { c.spi[0].enabled = 2u; } );
    reject( []( auto& c ) { c.uart[0].enabled = 2u; } );
    reject( []( auto& c ) { c.i2c[0].enabled = 2u; } );

    reject(
        []( auto& c ) { c.digital_in[0].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_3V3; } );
    reject(
        []( auto& c ) { c.digital_out[0].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_3V3; } );
    reject( []( auto& c ) { c.digital_out[0].initial_high = 1u; } );
    reject( []( auto& c ) { c.pwm_in[0].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_5V; } );
    reject( []( auto& c ) { c.pwm_out[0].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_5V; } );
    reject( []( auto& c ) { c.pwm_out[0].initial_period_nanoseconds = 1u; } );
    reject( []( auto& c ) { c.pwm_out[0].initial_duty_cycle_permyriad = 1u; } );

    reject( []( auto& c ) { c.can[0].bit_rate = 1u; } );
    reject( []( auto& c ) { c.can[0].termination_enabled = 1u; } );
    reject( []( auto& c ) { c.can[0].capture_limit_bytes = 1u; } );

    reject( []( auto& c ) { c.spi[0].bit_rate = 1u; } );
    reject( []( auto& c ) { c.spi[0].role = HIL_APPLICATION_BUS_ROLE_MASTER; } );
    reject( []( auto& c ) { c.spi[0].data_width = HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS; } );
    reject( []( auto& c ) { c.spi[0].bit_order = HIL_APPLICATION_SPI_BIT_ORDER_MSB_FIRST; } );
    reject(
        []( auto& c ) { c.spi[0].clock_polarity = HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_LOW; } );
    reject( []( auto& c ) { c.spi[0].clock_phase = HIL_APPLICATION_SPI_CLOCK_PHASE_FIRST_EDGE; } );
    reject( []( auto& c ) { c.spi[0].capture_limit_bytes = 1u; } );

    reject( []( auto& c ) { c.uart[0].baud_rate = 1u; } );
    reject( []( auto& c ) {
        c.uart[0].electrical_mode = HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3;
    } );
    reject( []( auto& c ) { c.uart[0].word_length = HIL_APPLICATION_UART_WORD_LENGTH_8_BITS; } );
    reject( []( auto& c ) { c.uart[0].parity = HIL_APPLICATION_UART_PARITY_NONE; } );
    reject( []( auto& c ) { c.uart[0].stop_bits = HIL_APPLICATION_UART_STOP_BITS_1; } );
    reject( []( auto& c ) { c.uart[0].rx_enabled = 1u; } );
    reject( []( auto& c ) { c.uart[0].tx_enabled = 1u; } );
    reject( []( auto& c ) { c.uart[0].capture_limit_bytes = 1u; } );

    reject( []( auto& c ) { c.i2c[0].bit_rate = 1u; } );
    reject( []( auto& c ) { c.i2c[0].role = HIL_APPLICATION_BUS_ROLE_MASTER; } );
    reject( []( auto& c ) { c.i2c[0].own_address_7bit = 1u; } );
    reject( []( auto& c ) { c.i2c[0].voltage_level = HIL_APPLICATION_I2C_VOLTAGE_3V3; } );
    reject( []( auto& c ) { c.i2c[0].pull_up = HIL_APPLICATION_I2C_PULL_UP_1K; } );
    reject( []( auto& c ) { c.i2c[0].capture_limit_bytes = 1u; } );
}

TEST( ApplicationTestConfigurationValidation, RejectsInvalidAndReservedEnums )
{
    const auto context = MakeContext();
    const auto reject  = [&]( auto mutate ) {
        auto config = RepresentativeConfiguration();
        mutate( config );
        ExpectValidationFailure( context, config );
    };

    reject( []( auto& c ) {
        c.digital_in[3].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID;
    } );
    reject( []( auto& c ) {
        c.digital_in[3].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_RESERVED;
    } );
    reject( []( auto& c ) {
        c.digital_out[4].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID;
    } );
    reject( []( auto& c ) {
        c.digital_out[4].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_RESERVED;
    } );
    reject( []( auto& c ) {
        c.pwm_in[1].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID;
    } );
    reject( []( auto& c ) {
        c.pwm_in[1].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_RESERVED;
    } );
    reject( []( auto& c ) {
        c.pwm_out[0].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID;
    } );
    reject( []( auto& c ) {
        c.pwm_out[0].voltage_level = HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_RESERVED;
    } );
    reject( []( auto& c ) { c.spi[0].role = HIL_APPLICATION_BUS_ROLE_INVALID; } );
    reject( []( auto& c ) { c.spi[0].role = HIL_APPLICATION_BUS_ROLE_RESERVED; } );
    reject( []( auto& c ) { c.spi[0].data_width = HIL_APPLICATION_SPI_DATA_WIDTH_INVALID; } );
    reject( []( auto& c ) { c.spi[0].data_width = HIL_APPLICATION_SPI_DATA_WIDTH_RESERVED; } );
    reject( []( auto& c ) { c.spi[0].bit_order = HIL_APPLICATION_SPI_BIT_ORDER_INVALID; } );
    reject( []( auto& c ) { c.spi[0].bit_order = HIL_APPLICATION_SPI_BIT_ORDER_RESERVED; } );
    reject(
        []( auto& c ) { c.spi[0].clock_polarity = HIL_APPLICATION_SPI_CLOCK_POLARITY_INVALID; } );
    reject(
        []( auto& c ) { c.spi[0].clock_polarity = HIL_APPLICATION_SPI_CLOCK_POLARITY_RESERVED; } );
    reject( []( auto& c ) { c.spi[0].clock_phase = HIL_APPLICATION_SPI_CLOCK_PHASE_INVALID; } );
    reject( []( auto& c ) { c.spi[0].clock_phase = HIL_APPLICATION_SPI_CLOCK_PHASE_RESERVED; } );
    reject( []( auto& c ) {
        c.uart[1].electrical_mode = HIL_APPLICATION_UART_ELECTRICAL_MODE_INVALID;
    } );
    reject( []( auto& c ) {
        c.uart[1].electrical_mode = HIL_APPLICATION_UART_ELECTRICAL_MODE_RESERVED;
    } );
    reject( []( auto& c ) { c.uart[1].word_length = HIL_APPLICATION_UART_WORD_LENGTH_INVALID; } );
    reject( []( auto& c ) { c.uart[1].word_length = HIL_APPLICATION_UART_WORD_LENGTH_RESERVED; } );
    reject( []( auto& c ) { c.uart[1].parity = HIL_APPLICATION_UART_PARITY_INVALID; } );
    reject( []( auto& c ) { c.uart[1].parity = HIL_APPLICATION_UART_PARITY_RESERVED; } );
    reject( []( auto& c ) { c.uart[1].stop_bits = HIL_APPLICATION_UART_STOP_BITS_INVALID; } );
    reject( []( auto& c ) { c.uart[1].stop_bits = HIL_APPLICATION_UART_STOP_BITS_RESERVED; } );
    reject( []( auto& c ) { c.i2c[0].role = HIL_APPLICATION_BUS_ROLE_INVALID; } );
    reject( []( auto& c ) { c.i2c[0].role = HIL_APPLICATION_BUS_ROLE_RESERVED; } );
    reject( []( auto& c ) { c.i2c[0].voltage_level = HIL_APPLICATION_I2C_VOLTAGE_INVALID; } );
    reject( []( auto& c ) { c.i2c[0].voltage_level = HIL_APPLICATION_I2C_VOLTAGE_RESERVED; } );
    reject( []( auto& c ) { c.i2c[0].pull_up = HIL_APPLICATION_I2C_PULL_UP_INVALID; } );
    reject( []( auto& c ) { c.i2c[0].pull_up = HIL_APPLICATION_I2C_PULL_UP_RESERVED; } );
}

TEST( ApplicationTestConfigurationValidation, RejectsInvalidTickFlagsAndExtension )
{
    const auto context         = MakeContext();
    auto       config          = CanonicalConfiguration();
    config.expected_tick_count = 0u;
    ExpectValidationFailure( context, config );
    config                     = CanonicalConfiguration();
    config.expected_tick_count = HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT + 1u;
    ExpectValidationFailure( context, config );
    const auto limited_context = MakeContext( HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE, 255u, 10u );
    config                     = CanonicalConfiguration();
    config.expected_tick_count = 11u;
    ExpectValidationFailure( limited_context, config );
    config                               = CanonicalConfiguration();
    config.tick_duration_us.microseconds = 11u;
    ExpectValidationFailure( context, config );
    config       = CanonicalConfiguration();
    config.flags = 1u;
    ExpectValidationFailure( context, config );
    config                = CanonicalConfiguration();
    config.extension_data = HIL_Application_Byte_Span_T{ nullptr, 1u };
    ExpectValidationFailure( context, config );
}

TEST( ApplicationTestConfigurationValidation, RejectsInvalidDigitalAndPwmState )
{
    const auto context                 = MakeContext();
    auto       config                  = RepresentativeConfiguration();
    config.digital_out[4].initial_high = 2u;
    ExpectValidationFailure( context, config );
    config                                         = RepresentativeConfiguration();
    config.pwm_out[0].initial_duty_cycle_permyriad = 10001u;
    ExpectValidationFailure( context, config );
    config                                         = RepresentativeConfiguration();
    config.pwm_out[0].initial_period_nanoseconds   = 0u;
    config.pwm_out[0].initial_duty_cycle_permyriad = 1u;
    ExpectValidationFailure( context, config );
}

TEST( ApplicationTestConfigurationValidation,
      RejectsInvalidCommunicationRatesBooleansAndCaptureLimits )
{
    const auto context     = MakeContext( 512u, 90u );
    auto       config      = RepresentativeConfiguration();
    config.can[1].bit_rate = 0u;
    ExpectValidationFailure( context, config );
    config                            = RepresentativeConfiguration();
    config.can[1].termination_enabled = 2u;
    ExpectValidationFailure( context, config );
    config                            = RepresentativeConfiguration();
    config.can[1].capture_limit_bytes = 91u;
    ExpectValidationFailure( context, config );
    config                 = RepresentativeConfiguration();
    config.spi[0].bit_rate = 0u;
    ExpectValidationFailure( context, config );
    config                            = RepresentativeConfiguration();
    config.spi[0].capture_limit_bytes = 91u;
    ExpectValidationFailure( context, config );
    config                   = RepresentativeConfiguration();
    config.uart[1].baud_rate = 0u;
    ExpectValidationFailure( context, config );
    config                             = RepresentativeConfiguration();
    config.uart[1].capture_limit_bytes = 91u;
    ExpectValidationFailure( context, config );
    config                 = RepresentativeConfiguration();
    config.i2c[0].bit_rate = 0u;
    ExpectValidationFailure( context, config );
    config                            = RepresentativeConfiguration();
    config.i2c[0].capture_limit_bytes = 91u;
    ExpectValidationFailure( context, config );
}

TEST( ApplicationTestConfigurationValidation, RejectsInvalidUartDirectionCombinations )
{
    const auto context                 = MakeContext();
    auto       config                  = RepresentativeConfiguration();
    config.uart[1].rx_enabled          = 0u;
    config.uart[1].tx_enabled          = 0u;
    config.uart[1].capture_limit_bytes = 0u;
    ExpectValidationFailure( context, config );
    config                             = RepresentativeConfiguration();
    config.uart[1].rx_enabled          = 0u;
    config.uart[1].tx_enabled          = 1u;
    config.uart[1].capture_limit_bytes = 1u;
    ExpectValidationFailure( context, config );
    config                    = RepresentativeConfiguration();
    config.uart[1].rx_enabled = 2u;
    ExpectValidationFailure( context, config );
    config                    = RepresentativeConfiguration();
    config.uart[1].tx_enabled = 2u;
    ExpectValidationFailure( context, config );
}

TEST( ApplicationTestConfigurationValidation, RejectsInvalidI2cRoleAndAddressCombinations )
{
    const auto context = MakeContext();
    auto       config  = RepresentativeConfiguration();
    config.i2c[0].role = HIL_APPLICATION_BUS_ROLE_MASTER;
    ExpectValidationFailure( context, config );
    config                         = RepresentativeConfiguration();
    config.i2c[0].own_address_7bit = 0u;
    ExpectValidationFailure( context, config );
    config                         = RepresentativeConfiguration();
    config.i2c[0].own_address_7bit = 0x80u;
    ExpectValidationFailure( context, config );
    config             = RepresentativeConfiguration();
    config.i2c[0].role = HIL_APPLICATION_BUS_ROLE_RESERVED;
    ExpectValidationFailure( context, config );
}

TEST( ApplicationTestConfigurationMalformed, TruncationAtMajorFamilyBoundariesIsBounded )
{
    const auto context  = MakeContext();
    const auto complete = EncodeConfiguration( context, CanonicalConfiguration() );
    const std::array<std::size_t, 11u> boundaries{ 12u, 32u,  62u,  64u,  70u, 74u,
                                                   90u, 110u, 138u, 168u, 196u };
    for ( const auto payload_length : boundaries )
    {
        SCOPED_TRACE( payload_length );
        auto bytes = complete;
        bytes.resize( kHeaderSize + payload_length );
        PutU16Le( bytes, kPayloadLengthOffset, static_cast<std::uint16_t>( payload_length ) );
        std::size_t required = 99u;
        EXPECT_EQ(
            HIL_APPLICATION_Decode_Storage_Size( &context, bytes.data(), bytes.size(), &required ),
            HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
        EXPECT_EQ( required, 0u );
        ExpectDecodeFailure( context, bytes, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    }
}

TEST( ApplicationTestConfigurationMalformed, EnvelopeDeclaredTruncationRemainsTruncated )
{
    const auto context = MakeContext();
    auto       bytes   = EncodeConfiguration( context, CanonicalConfiguration() );
    bytes.resize( bytes.size() - 1u );
    std::size_t required = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, bytes.data(), bytes.size(), &required ),
        HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE );
    EXPECT_EQ( required, 0u );
    ExpectDecodeFailure( context, bytes, HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE );
}

TEST( ApplicationTestConfigurationMalformed, TrailingPayloadByteIsMalformed )
{
    const auto context = MakeContext();
    auto       bytes   = EncodeConfiguration( context, CanonicalConfiguration() );
    bytes.push_back( 0xabu );
    PutU16Le( bytes, kPayloadLengthOffset, static_cast<std::uint16_t>( kFixedPayloadSize + 1u ) );
    std::size_t required = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, bytes.data(), bytes.size(), &required ),
        HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( required, 0u );
    ExpectDecodeFailure( context, bytes, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
}

TEST( ApplicationTestConfigurationMalformed, InvalidExtensionLengthIsMalformed )
{
    const auto context = MakeContext();
    auto       bytes   = EncodeConfiguration( context, CanonicalConfiguration() );
    bytes[kHeaderSize + kExtensionLengthOffset] = 1u;
    std::size_t required                        = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, bytes.data(), bytes.size(), &required ),
        HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( required, 0u );
    ExpectDecodeFailure( context, bytes, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
}

TEST( ApplicationTestConfigurationMalformed,
      InvalidBooleanEnumAndDisabledWireRecordsFailTypedValidation )
{
    const auto context = MakeContext();
    for ( const auto& mutation : std::array<std::pair<std::size_t, std::uint8_t>, 3u>{
              std::pair<std::size_t, std::uint8_t>{ kDigitalInputOffset, 2u },
              std::pair<std::size_t, std::uint8_t>{ kDigitalInputOffset + 1u, 255u },
              std::pair<std::size_t, std::uint8_t>{ kDigitalOutputOffset + 1u, 1u } } )
    {
        auto bytes = EncodeConfiguration( context, CanonicalConfiguration() );
        bytes[kHeaderSize + mutation.first] = mutation.second;
        std::size_t required                = 99u;
        ASSERT_EQ(
            HIL_APPLICATION_Decode_Storage_Size( &context, bytes.data(), bytes.size(), &required ),
            HIL_APPLICATION_STATUS_OK );
        EXPECT_EQ( required, 0u );
        ExpectDecodeFailure( context, bytes, HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        required = 99u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, bytes.data(), bytes.size(),
                                                             &required ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        EXPECT_EQ( required, 0u );
    }
}

TEST( ApplicationTestConfigurationMalformed, PayloadLengthMismatchAndFailureOutputsAreCleared )
{
    const auto context = MakeContext();
    auto       bytes   = EncodeConfiguration( context, CanonicalConfiguration() );
    PutU16Le( bytes, kPayloadLengthOffset, static_cast<std::uint16_t>( kFixedPayloadSize + 1u ) );
    std::size_t required = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, bytes.data(), bytes.size(), &required ),
        HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE );
    EXPECT_EQ( required, 0u );
}
