#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"
#include "hil_rig_protocol/version.h"

namespace {

// Exercise both dispatch paths with independently constructed wire messages.
class ApplicationAlignedRecords : public ::testing::TestWithParam<bool>
{
protected:
    HIL_Application_Context_T           context{};
    HIL_Application_Logical_Operation_T operation{};
    HIL_Application_Captured_Record_T   record{};
    alignas( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT ) std::array<std::uint8_t, 4096u> storage{};

    void SetUp() override
    {
        HIL_Application_Config_T config{};
        ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
        ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    }

    std::size_t HeaderSize() const
    {
        return GetParam() ? 12u : 8u;
    }

    HIL_Application_Message_T Message( unsigned type, unsigned channel,
                                       const std::vector<std::uint8_t>& payload )
    {
        HIL_Application_Message_T message{};
        message.has_test_id                    = 1u;
        message.subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
        const HIL_Application_Byte_Span_T span = { payload.data(),
                                                   static_cast<std::uint8_t>( payload.size() ) };
        if ( GetParam() )
        {
            record       = { static_cast<HIL_Application_Peripheral_Type_T>( type ),
                             static_cast<std::uint8_t>( channel ), span };
            message.type = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT;
            message.body.variable_test_result.record_count = 1u;
            message.body.variable_test_result.records      = &record;
            message.body.variable_test_result.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
        }
        else
        {
            operation    = { static_cast<HIL_Application_Peripheral_Type_T>( type ),
                             static_cast<std::uint8_t>( channel ), span };
            message.type = HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION;
            message.body.update_instruction.operation_count = 1u;
            message.body.update_instruction.operations      = &operation;
        }
        return message;
    }

    static void Length( std::vector<std::uint8_t>& wire )
    {
        const auto size = wire.size() - 23u;
        wire[21]        = static_cast<std::uint8_t>( size & 255u );
        wire[22]        = static_cast<std::uint8_t>( size >> 8u );
    }

    std::vector<std::uint8_t> Wire( unsigned type, unsigned channel,
                                    const std::vector<std::uint8_t>& payload ) const
    {
        std::vector<std::uint8_t> wire( 23u + HeaderSize(), 0u );
        wire[0]  = HIL_RIG_PROTOCOL_VERSION_MAJOR;
        wire[1]  = HIL_RIG_PROTOCOL_VERSION_MINOR;
        wire[2]  = 1u;
        wire[19] = GetParam() ? 34u : 21u;
        wire[27] = 1u;
        wire.push_back( static_cast<std::uint8_t>( type ) );
        wire.push_back( static_cast<std::uint8_t>( channel ) );
        wire.push_back( static_cast<std::uint8_t>( payload.size() & 255u ) );
        wire.push_back( static_cast<std::uint8_t>( payload.size() >> 8u ) );
        wire.insert( wire.end(), payload.begin(), payload.end() );
        while ( ( wire.size() - 23u ) % 4u != 0u )
            wire.push_back( 0u );
        Length( wire );
        return wire;
    }

    void DecodeFailure( const std::vector<std::uint8_t>& wire, HIL_Application_Status_T expected )
    {
        std::size_t required = 99u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(),
                                                             &required ),
                   expected );
        EXPECT_EQ( required, 0u );
        HIL_Application_Message_T decoded{};
        decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
        std::size_t used = 99u;
        EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                                   storage.data(), storage.size(), &used ),
                   expected );
        EXPECT_EQ( used, 0u );
        EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    }

    void StructuralFailure( const std::vector<std::uint8_t>& wire )
    {
        std::size_t required = 99u;
        EXPECT_EQ(
            HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
            HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
        EXPECT_EQ( required, 0u );
        DecodeFailure( wire, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    }

    void CheckValue( unsigned type, unsigned channel, const std::vector<std::uint8_t>& payload,
                     bool valid )
    {
        SCOPED_TRACE( ::testing::Message() << "type=" << type << " channel=" << channel
                                           << " length=" << payload.size() );
        const auto message = Message( type, channel, payload );
        const auto status  = valid ? HIL_APPLICATION_STATUS_OK
                                   : ( type == HIL_APPLICATION_PERIPHERAL_I2C
                                           ? HIL_APPLICATION_STATUS_NOT_IMPLEMENTED
                                           : HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), status );
        std::array<std::uint8_t, 512u> encoded{};
        std::size_t                    used = 99u;
        ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(),
                                                   encoded.size(), &used ),
                   status );
        const auto wire = Wire( type, channel, payload );
        if ( !valid )
        {
            EXPECT_EQ( used, 0u );
            std::size_t required = 99u;
            // Storage sizing and full decoding share the encoded record validation rules.
            EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(),
                                                            &required ),
                       payload.empty() ? HIL_APPLICATION_STATUS_MALFORMED_MESSAGE : status );
            DecodeFailure( wire,
                           payload.empty() ? HIL_APPLICATION_STATUS_MALFORMED_MESSAGE : status );
            return;
        }
        EXPECT_EQ( std::vector<std::uint8_t>(
                       encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>( used ) ),
                   wire );
        std::size_t required  = 99u;
        std::size_t validated = 99u;
        EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &required ), status );
        EXPECT_EQ( required, wire.size() );
        EXPECT_EQ(
            HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
            status );
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(),
                                                             &validated ),
                   status );
        EXPECT_EQ( validated, required );
        HIL_Application_Message_T decoded{};
        ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                                   storage.data(), storage.size(), &used ),
                   status );
        const auto span = GetParam() ? decoded.body.variable_test_result.records[0].data
                                     : decoded.body.update_instruction.operations[0].payload;
        EXPECT_EQ( span.size, payload.size() );
        EXPECT_EQ( std::vector<std::uint8_t>( span.data, span.data + span.size ), payload );
    }
};

TEST_P( ApplicationAlignedRecords, EveryFixedPayloadLengthAndChannelBoundary )
{
    for ( const unsigned kind : { 1u, 3u, 5u } )
    {
        const unsigned type     = GetParam() ? kind : kind + 1u;
        const unsigned width    = kind + 1u;
        const unsigned channels = kind == 1u ? 1u : ( kind == 3u && !GetParam() ? 6u : 2u );
        for ( unsigned size = 0u; size <= 255u; ++size )
            CheckValue( type, 0u, std::vector<std::uint8_t>( size, 0u ), size == width );
        for ( unsigned channel = 0u; channel <= 255u; ++channel )
            CheckValue( type, channel, std::vector<std::uint8_t>( width, 0u ), channel < channels );
    }
}

TEST_P( ApplicationAlignedRecords, EveryRawStreamLengthAndPaddingResidue )
{
    for ( unsigned size = 0u; size <= 255u; ++size )
    {
        std::vector<std::uint8_t> payload( size );
        for ( unsigned i = 0u; i < size; ++i )
            payload[i] = static_cast<std::uint8_t>( i );
        for ( const unsigned channel : { 0u, 1u, 2u, 255u } )
        {
            CheckValue( 16u, channel, payload, size != 0u && channel < 2u );
            if ( GetParam() )
                CheckValue( 17u, channel, payload, size != 0u && channel < 2u );
        }
    }
}

TEST_P( ApplicationAlignedRecords, UnsupportedAndWrongDirectionTypes )
{
    for ( unsigned type = 0u; type <= 255u; ++type )
    {
        const unsigned digital = GetParam() ? 1u : 2u;
        if ( type == digital || type == digital + 2u || type == digital + 4u || type == 16u
             || type == 17u || type == 19u )
            continue;
        CheckValue( type, 0u, { 0u, 0u }, false );
    }
}

TEST_P( ApplicationAlignedRecords, EveryCanLengthAndEveryFrameValidated )
{
    for ( unsigned size = 0u; size <= 255u; ++size )
        CheckValue( 19u, 1u, std::vector<std::uint8_t>( size, 0u ),
                    size != 0u && size % 12u == 0u );
    std::vector<std::uint8_t> frames( 252u, 0u );
    for ( std::size_t offset = 0u; offset < frames.size(); offset += 12u )
    {
        SCOPED_TRACE( offset / 12u );
        frames[offset]      = 255u;
        frames[offset + 1u] = 7u;
        frames[offset + 2u] = 8u;
        CheckValue( 19u, 1u, frames, true );
        for ( const auto field : { 1u, 2u, 11u } )
        {
            const auto saved       = frames[offset + field];
            frames[offset + field] = field == 1u ? 8u : ( field == 2u ? 9u : 1u );
            CheckValue( 19u, 1u, frames, false );
            frames[offset + field] = saved;
        }
    }
    CheckValue( 19u, 2u, frames, false );
}

TEST_P( ApplicationAlignedRecords, DigitalReservedBitsAndPwmValueBoundaries )
{
    const unsigned digital = GetParam() ? 1u : 2u;
    CheckValue( digital, 0u, { 0xffu, 3u }, true );
    for ( unsigned bit = 10u; bit < 16u; ++bit )
        CheckValue( digital, 0u, { 0u, static_cast<std::uint8_t>( 1u << ( bit - 8u ) ) }, false );
    const unsigned pwm = GetParam() ? 5u : 6u;
    for ( const std::uint32_t period : { 0u, 1u, 0xffffffffu } )
    {
        for ( const unsigned duty : { 0u, 1u, 9999u, 10000u, 10001u, 65535u } )
        {
            SCOPED_TRACE( ::testing::Message() << "period=" << period << " duty=" << duty );
            const std::vector<std::uint8_t> payload = {
                static_cast<std::uint8_t>( period & 255u ),
                static_cast<std::uint8_t>( ( period >> 8u ) & 255u ),
                static_cast<std::uint8_t>( ( period >> 16u ) & 255u ),
                static_cast<std::uint8_t>( period >> 24u ),
                static_cast<std::uint8_t>( duty & 255u ),
                static_cast<std::uint8_t>( duty >> 8u ) };
            CheckValue( pwm, 1u, payload, duty <= 10000u && ( period != 0u || duty == 0u ) );
        }
    }
}

TEST_P( ApplicationAlignedRecords, NonAdjacentDuplicatePairRejectedOnWire )
{
    auto       wire          = Wire( 16u, 0u, { 0xabu } );
    const auto second        = Wire( 16u, 1u, { 0xcdu } );
    const auto record_offset = static_cast<std::ptrdiff_t>( 23u + HeaderSize() );
    const std::vector<std::uint8_t> first_record( wire.begin() + record_offset, wire.end() );
    wire.insert( wire.end(), second.begin() + record_offset, second.end() );
    wire.insert( wire.end(), first_record.begin(), first_record.end() );
    wire[27] = 3u;
    Length( wire );
    std::size_t required = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    DecodeFailure( wire, HIL_APPLICATION_STATUS_VALIDATION_FAILED );
}

TEST_P( ApplicationAlignedRecords, EveryBodyTruncationAndInvalidTlvLength )
{
    const auto complete = Wire( 16u, 0u, { 1u, 2u, 3u, 4u, 5u } );
    for ( std::size_t size = 23u; size < complete.size(); ++size )
    {
        SCOPED_TRACE( size );
        auto wire = complete;
        wire.resize( size );
        Length( wire );  // Reach the body scanner rather than envelope-length rejection.
        StructuralFailure( wire );
    }
    for ( const unsigned length : { 0u, 256u, 65535u } )
    {
        auto wire                     = complete;
        wire[23u + HeaderSize() + 2u] = static_cast<std::uint8_t>( length & 255u );
        wire[23u + HeaderSize() + 3u] = static_cast<std::uint8_t>( length >> 8u );
        StructuralFailure( wire );
    }
    auto wire = complete;
    wire[27]  = 2u;
    StructuralFailure( wire );
    wire = complete;
    wire.push_back( 0u );
    Length( wire );
    StructuralFailure( wire );
}

TEST_P( ApplicationAlignedRecords, EveryPaddingByteMustBeZero )
{
    for ( const unsigned length : { 1u, 2u, 3u } )
    {
        const auto complete = Wire( 16u, 0u, std::vector<std::uint8_t>( length, 0xabu ) );
        for ( std::size_t offset = 23u + HeaderSize() + 4u + length; offset < complete.size();
              ++offset )
        {
            SCOPED_TRACE( offset );
            auto wire    = complete;
            wire[offset] = 1u;
            StructuralFailure( wire );
        }
    }
}

TEST_P( ApplicationAlignedRecords, ExactStorageOwnsPayloadAndPreservesGuardBytes )
{
    auto        wire     = Wire( 16u, 1u, { 0u, 0xffu, 0x80u, 1u, 2u } );
    const auto  original = wire;
    std::size_t required = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
               HIL_APPLICATION_STATUS_OK );
    const std::size_t alignment  = HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT;
    const std::size_t descriptor = GetParam() ? sizeof( HIL_Application_Captured_Record_T )
                                              : sizeof( HIL_Application_Logical_Operation_T );
    EXPECT_EQ( required, ( ( descriptor + alignment - 1u ) / alignment ) * alignment + 5u );
    ASSERT_LT( required + alignment, storage.size() );
    storage.fill( 0xa5u );
    auto*                     start = storage.data() + alignment;
    HIL_Application_Message_T decoded{};
    std::size_t               used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded, start,
                                               required, &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, required );
    const auto span = GetParam() ? decoded.body.variable_test_result.records[0].data
                                 : decoded.body.update_instruction.operations[0].payload;
    EXPECT_EQ( span.data, start + required - 5u );
    EXPECT_TRUE( std::all_of( storage.begin(),
                              storage.begin() + static_cast<std::ptrdiff_t>( alignment ),
                              []( std::uint8_t b ) { return b == 0xa5u; } ) );
    EXPECT_TRUE( std::all_of( storage.begin() + static_cast<std::ptrdiff_t>( alignment + required ),
                              storage.end(), []( std::uint8_t b ) { return b == 0xa5u; } ) );
    std::fill( wire.begin(), wire.end(), 0u );
    std::vector<std::uint8_t> encoded( original.size() );
    ASSERT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &decoded, encoded.data(), encoded.size(), &used ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded, original );
}

TEST_P( ApplicationAlignedRecords, MaximumMinimumSizeRecordsFitThe512ByteProfile )
{
    const std::vector<std::pair<HIL_Application_Peripheral_Type_T, std::uint8_t>> descriptors =
        GetParam()
            ? std::vector<std::pair<HIL_Application_Peripheral_Type_T, std::uint8_t>>{
                  { HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT, 0u },
                  { HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT, 0u },
                  { HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT, 1u },
                  { HIL_APPLICATION_PERIPHERAL_PWM_INPUT, 0u },
                  { HIL_APPLICATION_PERIPHERAL_PWM_INPUT, 1u },
                  { HIL_APPLICATION_PERIPHERAL_UART, 0u },
                  { HIL_APPLICATION_PERIPHERAL_UART, 1u },
                  { HIL_APPLICATION_PERIPHERAL_SPI, 0u },
                  { HIL_APPLICATION_PERIPHERAL_SPI, 1u },
                  { HIL_APPLICATION_PERIPHERAL_CAN, 0u },
                  { HIL_APPLICATION_PERIPHERAL_CAN, 1u },
              }
            : std::vector<std::pair<HIL_Application_Peripheral_Type_T, std::uint8_t>>{
                  { HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT, 0u },
                  { HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT, 0u },
                  { HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT, 1u },
                  { HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT, 2u },
                  { HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT, 3u },
                  { HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT, 4u },
                  { HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT, 5u },
                  { HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT, 0u },
                  { HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT, 1u },
                  { HIL_APPLICATION_PERIPHERAL_UART, 0u },
                  { HIL_APPLICATION_PERIPHERAL_UART, 1u },
                  { HIL_APPLICATION_PERIPHERAL_SPI, 0u },
                  { HIL_APPLICATION_PERIPHERAL_SPI, 1u },
                  { HIL_APPLICATION_PERIPHERAL_CAN, 0u },
                  { HIL_APPLICATION_PERIPHERAL_CAN, 1u },
              };
    const std::size_t                                record_count = descriptors.size();
    std::vector<std::vector<std::uint8_t>>           payloads( record_count );
    std::vector<HIL_Application_Logical_Operation_T> operations;
    std::vector<HIL_Application_Captured_Record_T>   records;
    operations.resize( GetParam() ? 0u : record_count );
    records.resize( GetParam() ? record_count : 0u );

    for ( std::size_t i = 0u; i < record_count; ++i )
    {
        const auto [peripheral_type, channel] = descriptors[i];
        const std::size_t payload_size =
            peripheral_type == HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT
                    || peripheral_type == HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT
                ? 2u
            : peripheral_type == HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT
                    || peripheral_type == HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT
                ? 4u
            : peripheral_type == HIL_APPLICATION_PERIPHERAL_PWM_INPUT
                    || peripheral_type == HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT
                ? 6u
            : peripheral_type == HIL_APPLICATION_PERIPHERAL_CAN                ? 12u
            : peripheral_type == HIL_APPLICATION_PERIPHERAL_SPI && !GetParam() ? 3u
                                                                               : 1u;
        payloads[i].assign( payload_size, 0u );
        if ( peripheral_type == HIL_APPLICATION_PERIPHERAL_SPI && !GetParam() )
        {
            payloads[i][0] = 1u;
            payloads[i][1] = 1u;
            payloads[i][2] = 0xa5u;
        }
        else
        {
            payloads[i][0] = 0xa5u;
        }
        if ( GetParam() )
        {
            records[i] = { peripheral_type,
                           channel,
                           { payloads[i].data(), static_cast<std::uint8_t>( payload_size ) } };
        }
        else
        {
            operations[i] = { peripheral_type,
                              channel,
                              { payloads[i].data(), static_cast<std::uint8_t>( payload_size ) } };
        }
    }

    HIL_Application_Message_T message{};
    message.type        = GetParam() ? HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT
                                     : HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id = 1u;
    if ( GetParam() )
    {
        message.body.variable_test_result.record_count = static_cast<std::uint8_t>( record_count );
        message.body.variable_test_result.records      = records.data();
        message.body.variable_test_result.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
    }
    else
    {
        message.body.update_instruction.operation_count = static_cast<std::uint8_t>( record_count );
        message.body.update_instruction.operations      = operations.data();
    }

    std::size_t encoded_size = 99u;
    ASSERT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, GetParam() ? 147u : 175u );
    std::vector<std::uint8_t> encoded( encoded_size );
    std::size_t               used = 99u;
    ASSERT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(), &used ),
        HIL_APPLICATION_STATUS_OK );

    std::size_t required = 99u;
    ASSERT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, encoded.data(), encoded.size(),
                                                         &required ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_GT( required, encoded.size() );
    ASSERT_LT( required, storage.size() );
    HIL_Application_Message_T decoded{};
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded.size(), &decoded,
                                               storage.data(), storage.size(), &used ),
               HIL_APPLICATION_STATUS_OK );
}

TEST_P( ApplicationAlignedRecords, FlagsAndTickBoundsAcrossTypedAndWireValidation )
{
    const std::vector<std::uint8_t> payload = { 0xabu };
    for ( unsigned flags = 0u; flags <= 255u; ++flags )
    {
        SCOPED_TRACE( flags );
        auto message = Message( 16u, 0u, payload );
        if ( GetParam() )
            message.body.variable_test_result.flags = static_cast<std::uint8_t>( flags );
        else
            message.body.update_instruction.flags = static_cast<std::uint8_t>( flags );
        auto wire                            = Wire( 16u, 0u, payload );
        wire[23u + ( GetParam() ? 6u : 5u )] = static_cast<std::uint8_t>( flags );
        const auto expected =
            flags <= 1u ? HIL_APPLICATION_STATUS_OK : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), expected );
        std::size_t required = 99u;
        EXPECT_EQ(
            HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
            expected );
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(),
                                                             &required ),
                   expected );
        if ( flags > 1u )
            DecodeFailure( wire, expected );
    }
    HIL_Application_Config_T config{};
    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_expected_tick_count = 100u;
    ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    for ( const std::uint32_t tick : { 0u, 99u, 100u, 101u, 0xffffffffu } )
    {
        SCOPED_TRACE( tick );
        auto message = Message( 16u, 0u, payload );
        if ( GetParam() )
            message.body.variable_test_result.tick_number = tick;
        else
            message.body.update_instruction.tick_number = tick;
        auto wire = Wire( 16u, 0u, payload );
        for ( unsigned byte = 0u; byte < 4u; ++byte )
            wire[23u + byte] = static_cast<std::uint8_t>( ( tick >> ( 8u * byte ) ) & 255u );
        const auto expected =
            tick < 100u ? HIL_APPLICATION_STATUS_OK : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), expected );
        std::size_t required = 99u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(),
                                                             &required ),
                   expected );
        HIL_Application_Message_T decoded{};
        EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                                   storage.data(), storage.size(), &required ),
                   expected );
    }
}

TEST_P( ApplicationAlignedRecords, ConfiguredRecordLimitIsEnforcedByAllDecodeFacades )
{
    HIL_Application_Config_T config{};
    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_variable_data_size = 4u;
    ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    for ( const unsigned length : { 3u, 4u, 5u } )
    {
        const auto wire = Wire( 16u, 0u, std::vector<std::uint8_t>( length, 0xabu ) );
        const auto expected =
            length <= 4u ? HIL_APPLICATION_STATUS_OK : HIL_APPLICATION_STATUS_VALIDATION_FAILED;
        std::size_t required = 99u;
        EXPECT_EQ(
            HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
            expected );
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(),
                                                             &required ),
                   expected );
        HIL_Application_Message_T decoded{};
        EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                                   storage.data(), storage.size(), &required ),
                   expected );
        if ( length > 4u )
        {
            EXPECT_EQ( required, 0u );
            EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
        }
    }
}

TEST_P( ApplicationAlignedRecords, SpiPacketBoundariesAndZeroLengthPackets )
{
    if ( GetParam() )
        GTEST_SKIP()
            << "Packet framing applies only to UPDATE_INSTRUCTION; SPI results are raw bytes.";
    for ( const unsigned channel : { 0u, 1u, 2u, 255u } )
        CheckValue( 17u, channel, { 1u, 1u, 0xabu }, channel < 2u );
    for ( const std::vector<std::uint8_t>& payload : { std::vector<std::uint8_t>{ 0u },
                                                       { 1u },
                                                       { 1u, 1u },
                                                       { 2u, 0u, 1u, 0xabu },
                                                       { 2u, 1u, 0u, 0xabu },
                                                       { 2u, 255u, 255u, 0xabu },
                                                       { 255u, 1u, 0xabu },
                                                       { 1u, 1u, 0xabu, 0xcdu } } )
        CheckValue( 17u, 0u, payload, false );
    // The packet table and data together must fit the 255-byte span.
    for ( const unsigned packets : { 1u, 2u, 127u } )
    {
        std::vector<std::uint8_t> payload( 255u, 0xabu );
        payload[0] = static_cast<std::uint8_t>( packets );
        std::fill( payload.begin() + 1,
                   payload.begin() + static_cast<std::ptrdiff_t>( 1u + packets ), 1u );
        payload[1] = static_cast<std::uint8_t>( 255u - 2u * packets );
        CheckValue( 17u, 1u, payload, true );
    }
}

TEST_P( ApplicationAlignedRecords, ExactEncodeCapacityAndEveryShortDecodeCapacity )
{
    const std::vector<std::uint8_t> payload = { 1u, 2u, 3u };
    const auto                      message = Message( 16u, 0u, payload );
    const auto                      wire    = Wire( 16u, 0u, payload );
    std::vector<std::uint8_t>       output( wire.size() + 1u, 0xa5u );
    std::size_t                     used = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, output.data(), wire.size() - 1u,
                                               &used ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( used, 0u );
    ASSERT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, output.data(), wire.size(), &used ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, wire.size() );
    EXPECT_EQ( output.back(), 0xa5u );
    std::size_t required = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
               HIL_APPLICATION_STATUS_OK );
    for ( std::size_t capacity = 0u; capacity < required; ++capacity )
    {
        SCOPED_TRACE( capacity );
        HIL_Application_Message_T decoded{};
        used = 99u;
        EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                                   storage.data(), capacity, &used ),
                   HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
        EXPECT_EQ( used, 0u );
        EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    }
}

INSTANTIATE_TEST_SUITE_P( UpdateAndResult, ApplicationAlignedRecords, ::testing::Bool() );

}  // namespace
