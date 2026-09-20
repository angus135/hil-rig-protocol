#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"
#include "hil_rig_protocol/version.h"

namespace {

constexpr std::size_t kHeaderSize          = HIL_APPLICATION_HEADER_SIZE_BYTES;
constexpr std::size_t kPayloadLengthOffset = 21u;
constexpr std::size_t kPayloadOffset       = HIL_APPLICATION_HEADER_SIZE_BYTES;
constexpr std::size_t kResultHeaderSize    = 12u;

struct alignas( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT ) AlignedDecodeStorage
{
    std::array<std::uint8_t, 4096u> bytes{};
};

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
        id.bytes[i] = static_cast<std::uint8_t>( 0x40u + i );
    }
    return id;
}

HIL_Application_Context_T
MakeContext( std::size_t   max_message          = HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE,
             std::uint32_t max_ticks            = HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT,
             std::size_t max_variable_data_size = HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};
    EXPECT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_encoded_message_size = max_message;
    config.max_expected_tick_count  = max_ticks;
    config.max_variable_data_size   = max_variable_data_size;
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    return context;
}

HIL_Application_Message_T
WrapVariableTestResult( const HIL_Application_Variable_Test_Result_T& variable_result )
{
    HIL_Application_Message_T message{};
    message.type                      = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT;
    message.subtype                   = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id               = 1u;
    message.test_id                   = TestId();
    message.body.variable_test_result = variable_result;
    return message;
}

std::vector<std::uint8_t>
EncodeVariableTestResult( const HIL_Application_Context_T&              context,
                          const HIL_Application_Variable_Test_Result_T& variable_result )
{
    const auto message = WrapVariableTestResult( variable_result );
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );
    std::size_t size = 0u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &size ),
               HIL_APPLICATION_STATUS_OK );
    std::vector<std::uint8_t> encoded( size );
    std::size_t               used = 0u;
    EXPECT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(), &used ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, size );
    return encoded;
}

std::size_t CalculateExpectedStorage( std::size_t record_count, std::size_t total_payload_bytes )
{
    if ( record_count == 0u )
    {
        return 0u;
    }
    const std::size_t struct_bytes = record_count * sizeof( HIL_Application_Captured_Record_T );
    const std::size_t align_mask   = HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT - 1u;
    const std::size_t aligned_struct_bytes = ( struct_bytes + align_mask ) & ~align_mask;
    return aligned_struct_bytes + total_payload_bytes;
}

void ExpectVariableTestResultEqual( const HIL_Application_Variable_Test_Result_T& expected,
                                    const HIL_Application_Variable_Test_Result_T& actual )
{
    EXPECT_EQ( actual.tick_number, expected.tick_number );
    EXPECT_EQ( actual.condition, expected.condition );
    EXPECT_EQ( actual.flags, expected.flags );
    EXPECT_EQ( actual.problem_detail, expected.problem_detail );
    ASSERT_EQ( actual.record_count, expected.record_count );
    if ( expected.record_count == 0u )
    {
        EXPECT_EQ( actual.records, nullptr );
    }
    else
    {
        ASSERT_NE( actual.records, nullptr );
        ASSERT_NE( expected.records, nullptr );
        for ( std::size_t i = 0u; i < expected.record_count; ++i )
        {
            EXPECT_EQ( actual.records[i].peripheral_type, expected.records[i].peripheral_type );
            EXPECT_EQ( actual.records[i].channel, expected.records[i].channel );
            ASSERT_EQ( actual.records[i].data.size, expected.records[i].data.size );
            for ( std::size_t j = 0u; j < expected.records[i].data.size; ++j )
            {
                EXPECT_EQ( actual.records[i].data.data[j], expected.records[i].data.data[j] );
            }
        }
    }
}

void ExpectRoundTrip( const HIL_Application_Context_T&              context,
                      const HIL_Application_Variable_Test_Result_T& res )
{
    const auto                first = EncodeVariableTestResult( context, res );
    AlignedDecodeStorage      storage{};
    HIL_Application_Message_T decoded{};
    std::size_t               required = 99u;
    ASSERT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, first.data(), first.size(), &required ),
        HIL_APPLICATION_STATUS_OK );
    std::size_t used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, first.data(), first.size(), &decoded,
                                               required == 0u ? nullptr : storage.bytes.data(),
                                               required == 0u ? 0u : storage.bytes.size(), &used ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( used, required );
    std::size_t validated_storage = 99u;
    ASSERT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, first.data(), first.size(),
                                                         &validated_storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( validated_storage, required );
    EXPECT_EQ( decoded.has_test_id, 1u );
    EXPECT_EQ( decoded.subtype, HIL_APPLICATION_MESSAGE_SUBTYPE_NONE );
    const auto expected_id = TestId();
    EXPECT_TRUE( std::equal( std::begin( expected_id.bytes ), std::end( expected_id.bytes ),
                             std::begin( decoded.test_id.bytes ) ) );
    ASSERT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT );
    ExpectVariableTestResultEqual( res, decoded.body.variable_test_result );
    const auto second = EncodeVariableTestResult( context, decoded.body.variable_test_result );
    EXPECT_EQ( second, first );
}

void ExpectValidationFailure( const HIL_Application_Context_T&              context,
                              const HIL_Application_Variable_Test_Result_T& res )
{
    const auto message = WrapVariableTestResult( res );
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
    std::size_t required = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, bytes.data(), bytes.size(), &required ),
        expected_status );
    EXPECT_EQ( required, 0u );
    required = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Validate_Encoded_Message( &context, bytes.data(), bytes.size(), &required ),
        expected_status );
    EXPECT_EQ( required, 0u );
    HIL_Application_Message_T decoded{};
    decoded.type = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    AlignedDecodeStorage storage{};
    std::size_t          used = 77u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, bytes.data(), bytes.size(), &decoded,
                                               storage.bytes.data(), storage.bytes.size(), &used ),
               expected_status );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );
}

}  // namespace

TEST( ApplicationVariableResultGolden, EmptyResultZeroRecordsMatchesLiteralWireLayout )
{
    const auto context = MakeContext();

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number    = 10u;
    res.condition      = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.flags          = HIL_APPLICATION_RESULT_FLAG_COMPLETE_TICK;
    res.problem_detail = 0u;
    res.record_count   = 0u;
    res.records        = nullptr;

    const auto encoded = EncodeVariableTestResult( context, res );

    /* Envelope 23 + header 12 = 35 bytes */
    ASSERT_EQ( encoded.size(), kHeaderSize + kResultHeaderSize );

    /* Envelope fields */
    EXPECT_EQ( encoded[0], static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MAJOR ) );
    EXPECT_EQ( encoded[1], static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MINOR ) );
    EXPECT_EQ( encoded[2], 1u );
    const auto test_id = TestId();
    for ( std::size_t i = 0u; i < HIL_APPLICATION_TEST_ID_SIZE; ++i )
    {
        EXPECT_EQ( encoded[3u + i], test_id.bytes[i] );
    }
    EXPECT_EQ( encoded[19],
               static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT ) );
    EXPECT_EQ( encoded[20], static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_SUBTYPE_NONE ) );
    EXPECT_EQ( encoded[21], 12u ); /* payload_length LE u16 */
    EXPECT_EQ( encoded[22], 0u );

    /* Payload header fields */
    const std::size_t p = kPayloadOffset;
    EXPECT_EQ( encoded[p + 0u], 10u ); /* tick_number LE u32 */
    EXPECT_EQ( encoded[p + 1u], 0u );
    EXPECT_EQ( encoded[p + 2u], 0u );
    EXPECT_EQ( encoded[p + 3u], 0u );
    EXPECT_EQ( encoded[p + 4u], 0u ); /* record_count */
    EXPECT_EQ( encoded[p + 5u], static_cast<std::uint8_t>( HIL_APPLICATION_RESULT_CONDITION_OK ) );
    EXPECT_EQ( encoded[p + 6u], 0u ); /* flags */
    EXPECT_EQ( encoded[p + 7u], 0u ); /* reserved */
    EXPECT_EQ( encoded[p + 8u], 0u ); /* problem_detail LE u32 */
    EXPECT_EQ( encoded[p + 9u], 0u );
    EXPECT_EQ( encoded[p + 10u], 0u );
    EXPECT_EQ( encoded[p + 11u], 0u );

    ExpectRoundTrip( context, res );
}

TEST( ApplicationVariableResultGolden, FaultReportZeroRecordsMatchesLiteralWireLayout )
{
    const auto context = MakeContext();

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number    = 55u;
    res.condition      = HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM;
    res.flags          = HIL_APPLICATION_RESULT_FLAG_COMPLETE_TICK;
    res.problem_detail = 0xdeadbeefu;
    res.record_count   = 0u;
    res.records        = nullptr;

    const auto encoded = EncodeVariableTestResult( context, res );
    ASSERT_EQ( encoded.size(), kHeaderSize + kResultHeaderSize );

    const std::size_t p = kPayloadOffset;
    EXPECT_EQ( encoded[p + 0u], 55u );
    EXPECT_EQ( encoded[p + 4u], 0u );
    EXPECT_EQ( encoded[p + 5u],
               static_cast<std::uint8_t>( HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM ) );
    EXPECT_EQ( encoded[p + 6u], 0u );
    EXPECT_EQ( encoded[p + 7u], 0u );
    EXPECT_EQ( encoded[p + 8u], 0xefu );
    EXPECT_EQ( encoded[p + 9u], 0xbeu );
    EXPECT_EQ( encoded[p + 10u], 0xadu );
    EXPECT_EQ( encoded[p + 11u], 0xdeu );

    ExpectRoundTrip( context, res );
}

TEST( ApplicationVariableResultGolden, RepresentativeMultiRecordMatchesLiteralWireLayout )
{
    const auto context = MakeContext();

    /* 1. DIGITAL_INPUT (ch 0): 2 bytes (0x00a5 = pins 0,2,5,7 high), 2 pad bytes */
    const std::array<std::uint8_t, 2u> digital = { 0xa5u, 0x00u };

    /* 2. ANALOG_INPUT (ch 1): 4 bytes microvolts (1800000 = 0x001b7740), 0 pad */
    const std::array<std::uint8_t, 4u> analog = { 0x40u, 0x77u, 0x1bu, 0x00u };

    /* 3. PWM_INPUT (ch 0): 6 bytes (period 500000 ns = 0x0007a120, duty 2500 = 0x09c4), 2 pad */
    const std::array<std::uint8_t, 6u> pwm = { 0x20u, 0xa1u, 0x07u, 0x00u, 0xc4u, 0x09u };

    /* 4. UART (ch 1): 3 bytes, 1 pad byte */
    const std::array<std::uint8_t, 3u> uart = { 0x01u, 0x02u, 0x03u };

    /* 5. CAN (ch 1): 24 bytes (2 CAN frames), 0 pad */
    const std::array<std::uint8_t, 24u> can = {
        0x50u, 0x00u, 2u, 0x11u, 0x22u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x01u, 8u, 0xaau, 0xbbu, 0xccu, 0xddu, 0xeeu, 0xffu, 0x12u, 0x34u, 0x00u };

    std::array<HIL_Application_Captured_Record_T, 5u> recs{};
    recs[0].peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT;
    recs[0].channel         = 0u;
    recs[0].data            = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    recs[1].peripheral_type = HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT;
    recs[1].channel         = 1u;
    recs[1].data            = { analog.data(), static_cast<std::uint8_t>( analog.size() ) };

    recs[2].peripheral_type = HIL_APPLICATION_PERIPHERAL_PWM_INPUT;
    recs[2].channel         = 0u;
    recs[2].data            = { pwm.data(), static_cast<std::uint8_t>( pwm.size() ) };

    recs[3].peripheral_type = HIL_APPLICATION_PERIPHERAL_UART;
    recs[3].channel         = 1u;
    recs[3].data            = { uart.data(), static_cast<std::uint8_t>( uart.size() ) };

    recs[4].peripheral_type = HIL_APPLICATION_PERIPHERAL_CAN;
    recs[4].channel         = 1u;
    recs[4].data            = { can.data(), static_cast<std::uint8_t>( can.size() ) };

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number    = 0x00023344u;
    res.condition      = HIL_APPLICATION_RESULT_CONDITION_PARTIAL;
    res.flags          = HIL_APPLICATION_RESULT_FLAG_HAS_MORE_CHUNKS;
    res.problem_detail = 0x00004321u;
    res.record_count   = static_cast<std::uint8_t>( recs.size() );
    res.records        = recs.data();

    const auto encoded = EncodeVariableTestResult( context, res );

    constexpr std::size_t kExpectedPayloadSize = 76u;
    constexpr std::size_t kExpectedTotalSize   = kHeaderSize + kExpectedPayloadSize;
    ASSERT_EQ( encoded.size(), kExpectedTotalSize );

    std::vector<std::uint8_t> golden( kExpectedTotalSize, 0u );
    /* Envelope */
    golden[0]          = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MAJOR );
    golden[1]          = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MINOR );
    golden[2]          = 1u;
    const auto test_id = TestId();
    std::copy( std::begin( test_id.bytes ), std::end( test_id.bytes ), golden.begin() + 3 );
    golden[19] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_TEST_RESULT );
    golden[20] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_SUBTYPE_NONE );
    PutU16Le( golden, kPayloadLengthOffset, static_cast<std::uint16_t>( kExpectedPayloadSize ) );

    /* Payload Header */
    const std::size_t p = kPayloadOffset;
    PutU32Le( golden, p + 0u, 0x00023344u );
    golden[p + 4u] = 5u; /* record_count */
    golden[p + 5u] = static_cast<std::uint8_t>( HIL_APPLICATION_RESULT_CONDITION_PARTIAL );
    golden[p + 6u] = HIL_APPLICATION_RESULT_FLAG_HAS_MORE_CHUNKS;
    golden[p + 7u] = 0u; /* reserved */
    PutU32Le( golden, p + 8u, 0x00004321u );

    /* Rec 0 (Digital) */
    std::size_t cur  = p + 12u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT );
    golden[cur + 1u] = 0u;
    PutU16Le( golden, cur + 2u, 2u );
    golden[cur + 4u] = digital[0];
    golden[cur + 5u] = digital[1];
    golden[cur + 6u] = 0u; /* pad */
    golden[cur + 7u] = 0u; /* pad */

    /* Rec 1 (Analog) */
    cur += 8u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT );
    golden[cur + 1u] = 1u;
    PutU16Le( golden, cur + 2u, 4u );
    std::copy( analog.begin(), analog.end(),
               golden.begin() + static_cast<std::ptrdiff_t>( cur + 4u ) );

    /* Rec 2 (PWM) */
    cur += 8u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_PWM_INPUT );
    golden[cur + 1u] = 0u;
    PutU16Le( golden, cur + 2u, 6u );
    std::copy( pwm.begin(), pwm.end(), golden.begin() + static_cast<std::ptrdiff_t>( cur + 4u ) );
    golden[cur + 10u] = 0u; /* pad */
    golden[cur + 11u] = 0u; /* pad */

    /* Rec 3 (UART) */
    cur += 12u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_UART );
    golden[cur + 1u] = 1u;
    PutU16Le( golden, cur + 2u, 3u );
    std::copy( uart.begin(), uart.end(), golden.begin() + static_cast<std::ptrdiff_t>( cur + 4u ) );
    golden[cur + 7u] = 0u; /* pad */

    /* Rec 4 (CAN) */
    cur += 8u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_CAN );
    golden[cur + 1u] = 1u;
    PutU16Le( golden, cur + 2u, 24u );
    std::copy( can.begin(), can.end(), golden.begin() + static_cast<std::ptrdiff_t>( cur + 4u ) );

    EXPECT_EQ( encoded, golden );

    ExpectRoundTrip( context, res );
}

TEST( ApplicationVariableResultCodec, ZeroRecordsRequiresZeroDecodeStorage )
{
    const auto context = MakeContext();

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number  = 0u;
    res.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.record_count = 0u;

    const auto wire = EncodeVariableTestResult( context, res );

    std::size_t required = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required, 0u );
}

TEST( ApplicationVariableResultValidation, ConfiguredVariableDataSizeBoundsUartPayload )
{
    const auto context = MakeContext( HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE,
                                      HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT, 3u );

    const std::array<std::uint8_t, 3u> accepted_payload = { 'a', 'b', 'c' };
    HIL_Application_Captured_Record_T  accepted_record{};
    accepted_record.peripheral_type = HIL_APPLICATION_PERIPHERAL_UART;
    accepted_record.channel         = 0u;
    accepted_record.data            = { accepted_payload.data(),
                                        static_cast<std::uint8_t>( accepted_payload.size() ) };

    HIL_Application_Variable_Test_Result_T accepted{};
    accepted.flags        = HIL_APPLICATION_RESULT_FLAG_COMPLETE_TICK;
    accepted.record_count = 1u;
    accepted.records      = &accepted_record;

    const auto encoded = EncodeVariableTestResult( context, accepted );
    EXPECT_FALSE( encoded.empty() );
    ExpectRoundTrip( context, accepted );

    const std::array<std::uint8_t, 4u> rejected_payload = { 'a', 'b', 'c', 'd' };
    HIL_Application_Captured_Record_T  rejected_record{};
    rejected_record.peripheral_type = HIL_APPLICATION_PERIPHERAL_UART;
    rejected_record.channel         = 0u;
    rejected_record.data            = { rejected_payload.data(),
                                        static_cast<std::uint8_t>( rejected_payload.size() ) };

    HIL_Application_Variable_Test_Result_T rejected{};
    rejected.flags        = HIL_APPLICATION_RESULT_FLAG_COMPLETE_TICK;
    rejected.record_count = 1u;
    rejected.records      = &rejected_record;
    ExpectValidationFailure( context, rejected );
}

TEST( ApplicationVariableResultCodec, DecodeStorageExactCalculationMatchesScan )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u> digital = { 0x01u, 0x00u };
    const std::array<std::uint8_t, 4u> analog  = { 0x00u, 0x00u, 0x10u, 0x00u };

    std::array<HIL_Application_Captured_Record_T, 2u> recs{};
    recs[0].peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT;
    recs[0].channel         = 0u;
    recs[0].data            = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    recs[1].peripheral_type = HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT;
    recs[1].channel         = 1u;
    recs[1].data            = { analog.data(), static_cast<std::uint8_t>( analog.size() ) };

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number  = 10u;
    res.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.record_count = 2u;
    res.records      = recs.data();

    const auto wire = EncodeVariableTestResult( context, res );

    std::size_t required = 0u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required, CalculateExpectedStorage( 2u, 2u + 4u ) );
}

TEST( ApplicationVariableResultValidation, ConditionOkRequiresZeroProblemDetail )
{
    const auto context = MakeContext();

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number    = 1u;
    res.condition      = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.problem_detail = 1u; /* invalid when condition is OK */
    res.record_count   = 0u;
    res.records        = nullptr;

    ExpectValidationFailure( context, res );
}

TEST( ApplicationVariableResultValidation, ExecutionProblemAllowsNonZeroProblemDetail )
{
    const auto context = MakeContext();

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number    = 1u;
    res.condition      = HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM;
    res.problem_detail = 0x12345678u;
    res.record_count   = 0u;
    res.records        = nullptr;

    const auto message = WrapVariableTestResult( res );
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );
}

TEST( ApplicationVariableResultValidation, PartialConditionAllowsNonZeroProblemDetail )
{
    const auto context = MakeContext();

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number    = 1u;
    res.condition      = HIL_APPLICATION_RESULT_CONDITION_PARTIAL;
    res.problem_detail = 0x12345678u;
    res.record_count   = 0u;
    res.records        = nullptr;

    const auto message = WrapVariableTestResult( res );
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );
}

TEST( ApplicationVariableResultValidation, CaptureOverflowUsesStablePartialProblemDetail )
{
    static_assert( HIL_APPLICATION_RESULT_PROBLEM_DETAIL_CAPTURE_OVERFLOW == 1u );
    const auto                         context = MakeContext();
    const std::array<std::uint8_t, 1u> retained_prefix{ 0xa5u };
    HIL_Application_Captured_Record_T  record{};
    record.peripheral_type = HIL_APPLICATION_PERIPHERAL_UART;
    record.channel         = 0u;
    record.data = { retained_prefix.data(), static_cast<std::uint8_t>( retained_prefix.size() ) };

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number    = 3u;
    res.condition      = HIL_APPLICATION_RESULT_CONDITION_PARTIAL;
    res.flags          = HIL_APPLICATION_RESULT_FLAG_COMPLETE_TICK;
    res.problem_detail = HIL_APPLICATION_RESULT_PROBLEM_DETAIL_CAPTURE_OVERFLOW;
    res.record_count   = 1u;
    res.records        = &record;

    const auto message = WrapVariableTestResult( res );
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );
    ExpectRoundTrip( context, res );
}

TEST( ApplicationVariableResultValidation, InvalidConditionRejected )
{
    const auto context = MakeContext();

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number  = 1u;
    res.condition    = static_cast<HIL_Application_Result_Condition_T>( 3u );
    res.record_count = 0u;

    ExpectValidationFailure( context, res );
}

TEST( ApplicationVariableResultValidation, DuplicatePeripheralChannelPairRejected )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u> digital1 = { 0x01u, 0x00u };
    const std::array<std::uint8_t, 2u> digital2 = { 0x02u, 0x00u };

    std::array<HIL_Application_Captured_Record_T, 2u> recs{};
    recs[0].peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT;
    recs[0].channel         = 0u;
    recs[0].data            = { digital1.data(), static_cast<std::uint8_t>( digital1.size() ) };

    recs[1].peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT;
    recs[1].channel         = 0u; /* duplicate */
    recs[1].data            = { digital2.data(), static_cast<std::uint8_t>( digital2.size() ) };

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number  = 1u;
    res.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.record_count = 2u;
    res.records      = recs.data();

    ExpectValidationFailure( context, res );
}

TEST( ApplicationVariableResultValidation, OutputPeripheralsRejectedInResult )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u> digital = { 0x01u, 0x00u };
    HIL_Application_Captured_Record_T  rec{};
    rec.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT; /* output not permitted */
    rec.channel         = 0u;
    rec.data            = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number  = 1u;
    res.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.record_count = 1u;
    res.records      = &rec;

    ExpectValidationFailure( context, res );
}

TEST( ApplicationVariableResultValidation, EnvelopeRequiresTestIdAndSubtypeNone )
{
    const auto context = MakeContext();

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number  = 1u;
    res.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.record_count = 0u;

    /* Missing test_id */
    auto message        = WrapVariableTestResult( res );
    message.has_test_id = 0u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID );

    /* Subtype not NONE */
    message         = WrapVariableTestResult( res );
    message.subtype = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INVALID_SUBTYPE );
}

TEST( ApplicationVariableResultDecode, NonZeroReservedByteRejected )
{
    const auto context = MakeContext();

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number  = 1u;
    res.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.record_count = 0u;

    auto wire = EncodeVariableTestResult( context, res );

    /* Offset 23 + 7 is the reserved byte */
    wire[kPayloadOffset + 7u] = 0x01u;

    ExpectDecodeFailure( context, wire, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
}

TEST( ApplicationVariableResultDecode, TrailingBytesWhenRecordCountZeroRejected )
{
    const auto context = MakeContext();

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number  = 1u;
    res.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.record_count = 0u;

    auto wire = EncodeVariableTestResult( context, res );

    /* Append unexpected byte to payload */
    wire.push_back( 0x00u );
    PutU16Le( wire, kPayloadLengthOffset,
              static_cast<std::uint16_t>( wire.size() - kPayloadOffset ) );

    ExpectDecodeFailure( context, wire, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
}

TEST( ApplicationVariableResultDecode, NonZeroPaddingBytesRejected )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u> digital = { 0x01u, 0x00u };
    HIL_Application_Captured_Record_T  rec{};
    rec.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT;
    rec.channel         = 0u;
    rec.data            = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number  = 1u;
    res.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.record_count = 1u;
    res.records      = &rec;

    auto wire = EncodeVariableTestResult( context, res );

    /* Record 0 has 2 pad bytes at offsets 23 + 12 + 6 and 23 + 12 + 7 */
    wire[kPayloadOffset + kResultHeaderSize + 6u] = 0xeeu;

    ExpectDecodeFailure( context, wire, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
}

TEST( ApplicationVariableResultDecode, StorageBufferTooSmallRejected )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u> digital = { 0x01u, 0x00u };
    HIL_Application_Captured_Record_T  rec{};
    rec.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT;
    rec.channel         = 0u;
    rec.data            = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    HIL_Application_Variable_Test_Result_T res{};
    res.tick_number  = 1u;
    res.condition    = HIL_APPLICATION_RESULT_CONDITION_OK;
    res.record_count = 1u;
    res.records      = &rec;

    const auto wire = EncodeVariableTestResult( context, res );

    AlignedDecodeStorage      storage{};
    HIL_Application_Message_T decoded{};
    std::size_t               used = 0u;

    std::size_t req = 0u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &req ),
               HIL_APPLICATION_STATUS_OK );

    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               storage.bytes.data(), req - 1u, &used ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
}
