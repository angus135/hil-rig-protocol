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

HIL_Application_Context_T
MakeCodecContext( std::size_t max_message_size = HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};
    EXPECT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_encoded_message_size = max_message_size;
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    return context;
}

HIL_Application_Message_T BasicSystemInfoRequest()
{
    HIL_Application_Message_T message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    message.has_test_id = 0u;
    message.body.system_info_request.request_firmware_git_hash = 1u;
    message.body.system_info_request.query = HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC;
    return message;
}

std::array<std::uint8_t, 25u> BasicSystemInfoGolden()
{
    return { 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
             0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
             0x00u, 0x01u, 0x01u, 0x02u, 0x00u, 0x01u, 0x01u };
}

std::array<std::uint8_t, 28u> ExecutionControlGoldenWithTestId()
{
    return { 0x00u, 0x01u, 0x01u, 0x80u, 0x81u, 0x82u, 0x83u, 0x84u, 0x85u, 0x86u,
             0x87u, 0x88u, 0x89u, 0x8au, 0x8bu, 0x8cu, 0x8du, 0x8eu, 0x8fu, 0x13u,
             0x00u, 0x05u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u };
}

std::array<std::uint8_t, 38u> SystemInfoResponseGoldenWithDiagnostic()
{
    return { 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
             0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x02u,
             0x01u, 0x0fu, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x02u,
             0x00u, 0x03u, 0x00u, 0x04u, 0x00u, 0x01u, 0xaau, 0x00u };
}

std::vector<std::uint8_t> FixedBodyEnvelope( HIL_Application_Message_Type_T type,
                                             std::size_t                    payload_size )
{
    std::vector<std::uint8_t> bytes( kHeaderSize + payload_size, 0u );
    bytes[0]  = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MAJOR );
    bytes[1]  = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MINOR );
    bytes[19] = static_cast<std::uint8_t>( type );
    bytes[20] = static_cast<std::uint8_t>( type == HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST
                                               ? HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC
                                               : HIL_APPLICATION_MESSAGE_SUBTYPE_NONE );
    bytes[21] = static_cast<std::uint8_t>( payload_size & 0xffu );
    bytes[22] = static_cast<std::uint8_t>( ( payload_size >> 8u ) & 0xffu );
    return bytes;
}

struct FixedBodyWidthCase
{
    HIL_Application_Message_Type_T type;
    std::size_t                    payload_size;
};

constexpr std::array<FixedBodyWidthCase, 6u> kFixedBodyWidths = {
    FixedBodyWidthCase{ HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST, 2u },
    FixedBodyWidthCase{ HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION, 50u },
    FixedBodyWidthCase{ HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL, 5u },
    FixedBodyWidthCase{ HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL, 5u },
    FixedBodyWidthCase{ HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT, 39u },
    FixedBodyWidthCase{ HIL_APPLICATION_MESSAGE_TYPE_RESPONSE, 13u },
};

void ExpectDecodeFailurePublishesNothing(
    const HIL_Application_Context_T& context, const std::uint8_t* bytes, std::size_t size,
    HIL_Application_Status_T expected    = HIL_APPLICATION_STATUS_INTERNAL_ERROR,
    bool                     check_exact = false )
{
    HIL_Application_Message_T decoded{};
    decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    std::size_t used = 123u;
    const auto  status =
        HIL_APPLICATION_Decode_Message( &context, bytes, size, &decoded, nullptr, 0u, &used );
    if ( check_exact )
    {
        EXPECT_EQ( status, expected );
    }
    else
    {
        EXPECT_NE( status, HIL_APPLICATION_STATUS_OK );
    }
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );
}
}  // namespace

static_assert( HIL_APPLICATION_HEADER_SIZE_BYTES == 23u );
static_assert( HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE == 23u + UINT16_MAX );
static_assert( HIL_APPLICATION_MIN_COMPLETE_MESSAGE_SIZE == 25u );
static_assert( HIL_RIG_PROTOCOL_VERSION_MAJOR == 0u );
static_assert( HIL_RIG_PROTOCOL_VERSION_MINOR == 1u );
static_assert( HIL_RIG_PROTOCOL_VERSION_PATCH == 0u );
static_assert( HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST == 1 );
static_assert( HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL == 19 );
static_assert( HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC == 1 );
static_assert( HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC == 1 );

TEST( ApplicationCodecEnvelope, LiteralVectorWithoutTestIdUsesOverallVersionAndZeroIdBytes )
{
    const auto                    context  = MakeCodecContext();
    const auto                    message  = BasicSystemInfoRequest();
    const auto                    expected = BasicSystemInfoGolden();
    std::array<std::uint8_t, 64u> encoded{};
    std::size_t                   encoded_size = 999u;

    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_size, expected.size() );
    EXPECT_TRUE( std::equal( expected.begin(), expected.end(), encoded.begin() ) );
}

TEST( ApplicationCodecEnvelope, LiteralVectorWithPresentOpaqueTestIdPreservesAllBytes )
{
    const auto                context = MakeCodecContext();
    HIL_Application_Message_T message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id = 1u;
    for ( std::size_t i = 0u; i < HIL_APPLICATION_TEST_ID_SIZE; ++i )
    {
        message.test_id.bytes[i] = static_cast<std::uint8_t>( 0x80u + i );
    }
    message.body.execution_control.command = HIL_APPLICATION_CONTROL_START;
    message.body.execution_control.flags   = 0u;

    const auto expected = ExecutionControlGoldenWithTestId();

    std::array<std::uint8_t, 64u> encoded{};
    std::size_t                   encoded_size = 999u;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_size, expected.size() );
    EXPECT_TRUE( std::equal( expected.begin(), expected.end(), encoded.begin() ) );
}

TEST( ApplicationCodecEnvelope, IndependentlyWrittenLiteralVectorsDecodeSuccessfully )
{
    const auto context = MakeCodecContext();

    const auto                request_bytes = BasicSystemInfoGolden();
    HIL_Application_Message_T request{};
    std::size_t               used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, request_bytes.data(), request_bytes.size(),
                                               &request, nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, 0u );
    EXPECT_EQ( request.type, HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST );
    EXPECT_EQ( request.subtype, HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC );
    EXPECT_EQ( request.has_test_id, 0u );
    EXPECT_EQ( request.body.system_info_request.request_firmware_git_hash, 1u );
    EXPECT_EQ( request.body.system_info_request.query, HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC );

    const auto                control_bytes = ExecutionControlGoldenWithTestId();
    HIL_Application_Message_T control{};
    used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, control_bytes.data(), control_bytes.size(),
                                               &control, nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, 0u );
    EXPECT_EQ( control.type, HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL );
    EXPECT_EQ( control.body.execution_control.command, HIL_APPLICATION_CONTROL_START );
    EXPECT_EQ( control.body.execution_control.flags, 0u );
    EXPECT_EQ( control.has_test_id, 1u );
    for ( std::size_t i = 0u; i < HIL_APPLICATION_TEST_ID_SIZE; ++i )
    {
        EXPECT_EQ( control.test_id.bytes[i], static_cast<std::uint8_t>( 0x80u + i ) );
    }
}

TEST( ApplicationCodecEnvelope, LiteralVectorWithPresentAllZeroTestIdIsValid )
{
    const auto                context = MakeCodecContext();
    HIL_Application_Message_T message{};
    message.type                           = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    message.subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id                    = 1u;
    message.body.execution_control.command = HIL_APPLICATION_CONTROL_START;
    message.body.execution_control.flags   = 0u;

    std::array<std::uint8_t, 28u> expected{};
    expected[0]  = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MAJOR );
    expected[1]  = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MINOR );
    expected[2]  = 1u;
    expected[19] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL );
    expected[20] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_SUBTYPE_NONE );
    expected[21] = 5u;
    expected[22] = 0u;
    expected[23] = static_cast<std::uint8_t>( HIL_APPLICATION_CONTROL_START );

    std::array<std::uint8_t, 64u> encoded{};
    std::size_t                   encoded_size = 0u;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_size, expected.size() );
    EXPECT_TRUE( std::equal( expected.begin(), expected.end(), encoded.begin() ) );

    HIL_Application_Message_T decoded{};
    std::size_t               used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded,
                                               nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, 0u );
    EXPECT_EQ( decoded.has_test_id, 1u );
    for ( const auto byte : decoded.test_id.bytes )
    {
        EXPECT_EQ( byte, 0u );
    }
}

TEST( ApplicationCodecEnvelope, AbsentTestIdAlwaysEncodesZeroBytes )
{
    const auto context        = MakeCodecContext();
    auto       message        = BasicSystemInfoRequest();
    message.test_id.bytes[0]  = 0xa5u;
    message.test_id.bytes[15] = 0x5au;
    std::array<std::uint8_t, 64u> encoded{};
    std::size_t                   encoded_size = 0u;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    for ( std::size_t i = 3u; i < 19u; ++i )
    {
        EXPECT_EQ( encoded[i], 0u );
    }
}

TEST( ApplicationCodecEnvelope, RejectsMalformedLiteralEnvelopeFields )
{
    const auto context = MakeCodecContext();
    const auto golden  = BasicSystemInfoGolden();

    auto bytes = golden;
    bytes[2]   = 2u;
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_MALFORMED_MESSAGE, true );

    bytes    = golden;
    bytes[3] = 1u;
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID, true );

    bytes     = golden;
    bytes[19] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE, true );

    bytes     = golden;
    bytes[19] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_TYPE_RESERVED );
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE, true );

    bytes     = golden;
    bytes[20] = 2u;
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_INVALID_SUBTYPE, true );

    bytes     = golden;
    bytes[20] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_SUBTYPE_RESERVED );
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_INVALID_SUBTYPE, true );

    bytes = golden;
    bytes[0] ^= 1u;
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE, true );

    bytes = golden;
    bytes[1] ^= 1u;
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE, true );

    bytes     = golden;
    bytes[20] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_SUBTYPE_NONE );
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_INVALID_SUBTYPE, true );
}

TEST( ApplicationCodecEnvelope, PayloadLengthIsLiteralLittleEndianUint16BeyondOneByte )
{
    const auto                     context = MakeCodecContext( 1024u );
    std::array<std::uint8_t, 250u> diagnostic{};
    std::array<std::uint8_t, 10u>  git_hash{};
    HIL_Application_Message_T      message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    message.has_test_id = 0u;
    message.body.system_info_response.application_protocol_major = HIL_RIG_PROTOCOL_VERSION_MAJOR;
    message.body.system_info_response.application_protocol_minor = HIL_RIG_PROTOCOL_VERSION_MINOR;
    message.body.system_info_response.application_protocol_patch = HIL_RIG_PROTOCOL_VERSION_PATCH;
    message.body.system_info_response.firmware_version_major     = 7u;
    message.body.system_info_response.firmware_version_minor     = 8u;
    message.body.system_info_response.firmware_version_patch     = 9u;
    message.body.system_info_response.diagnostic_data            = HIL_Application_Byte_Span_T{
        diagnostic.data(), static_cast<std::uint8_t>( diagnostic.size() ) };
    message.body.system_info_response.firmware_git_hash = HIL_Application_Byte_Span_T{
        git_hash.data(), static_cast<std::uint8_t>( git_hash.size() ) };

    std::array<std::uint8_t, 512u> encoded{};
    std::size_t                    encoded_size = 0u;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );

    constexpr std::size_t expected_payload_size = 274u;
    EXPECT_EQ( encoded_size, kHeaderSize + expected_payload_size );
    EXPECT_EQ( encoded[kPayloadLengthOffset], 0x12u );
    EXPECT_EQ( encoded[kPayloadLengthOffset + 1u], 0x01u );
}

TEST( ApplicationCodecDecode, EveryTruncatedLengthFailsWithoutPublishingOutput )
{
    const auto context = MakeCodecContext();
    const auto golden  = BasicSystemInfoGolden();
    for ( std::size_t size = 0u; size < golden.size(); ++size )
    {
        ExpectDecodeFailurePublishesNothing( context, golden.data(), size );
    }
}

TEST( ApplicationCodecDecode, DeclaredLengthAndTrailingByteRulesAreExact )
{
    const auto context = MakeCodecContext();
    auto       bytes   = BasicSystemInfoGolden();

    bytes[kPayloadLengthOffset] = 3u;
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE, true );
    std::size_t required_storage = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, bytes.data(), bytes.size(),
                                                    &required_storage ),
               HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE );
    EXPECT_EQ( required_storage, 0u );

    bytes                       = BasicSystemInfoGolden();
    bytes[kPayloadLengthOffset] = 1u;
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_MALFORMED_MESSAGE, true );

    std::array<std::uint8_t, 26u> trailing{};
    std::copy( bytes.begin(), bytes.end(), trailing.begin() );
    trailing[kPayloadLengthOffset] = 2u;
    trailing[25]                   = 0x55u;
    ExpectDecodeFailurePublishesNothing( context, trailing.data(), trailing.size(),
                                         HIL_APPLICATION_STATUS_MALFORMED_MESSAGE, true );
    required_storage = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, trailing.data(), trailing.size(),
                                                    &required_storage ),
               HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( required_storage, 0u );

    std::array<std::uint8_t, 24u> short_fixed_body{};
    const auto                    golden = BasicSystemInfoGolden();
    std::copy_n( golden.begin(), short_fixed_body.size(), short_fixed_body.begin() );
    short_fixed_body[kPayloadLengthOffset] = 1u;
    ExpectDecodeFailurePublishesNothing( context, short_fixed_body.data(), short_fixed_body.size(),
                                         HIL_APPLICATION_STATUS_MALFORMED_MESSAGE, true );

    required_storage = 99u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message(
                   &context, short_fixed_body.data(), short_fixed_body.size(), &required_storage ),
               HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( required_storage, 0u );
}

TEST( ApplicationCodecDecode, RejectsInputAboveConfiguredMaximum )
{
    const auto context = MakeCodecContext( HIL_APPLICATION_MIN_COMPLETE_MESSAGE_SIZE );
    const auto golden  = BasicSystemInfoGolden();
    std::array<std::uint8_t, 26u> oversized{};
    std::copy( golden.begin(), golden.end(), oversized.begin() );
    oversized.back() = 0x55u;
    ExpectDecodeFailurePublishesNothing( context, oversized.data(), oversized.size(),
                                         HIL_APPLICATION_STATUS_INVALID_LENGTH, true );
}

TEST( ApplicationCodecDecode, ByteSpanMalformedInputAndCallerStorageHaveDistinctStatuses )
{
    const auto context = MakeCodecContext();
    const auto valid   = SystemInfoResponseGoldenWithDiagnostic();

    std::array<std::uint8_t, 37u> missing_span_data{};
    std::copy_n( valid.begin(), missing_span_data.size(), missing_span_data.begin() );
    missing_span_data[kPayloadLengthOffset] = 14u;
    missing_span_data[35]                   = 2u;
    ExpectDecodeFailurePublishesNothing( context, missing_span_data.data(),
                                         missing_span_data.size(),
                                         HIL_APPLICATION_STATUS_MALFORMED_MESSAGE, true );

    HIL_Application_Message_T decoded{};
    decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    std::size_t used = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, valid.data(), valid.size(), &decoded,
                                               nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );

    std::array<std::uint8_t, 1u> storage{};
    used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, valid.data(), valid.size(), &decoded,
                                               storage.data(), storage.size(), &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, 1u );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE );
    EXPECT_EQ( decoded.body.system_info_response.application_protocol_major, 0u );
    EXPECT_EQ( decoded.body.system_info_response.application_protocol_minor, 1u );
    EXPECT_EQ( decoded.body.system_info_response.application_protocol_patch, 0u );
    EXPECT_EQ( decoded.body.system_info_response.firmware_version_major, 2u );
    EXPECT_EQ( decoded.body.system_info_response.firmware_version_minor, 3u );
    EXPECT_EQ( decoded.body.system_info_response.firmware_version_patch, 4u );
    ASSERT_EQ( decoded.body.system_info_response.diagnostic_data.size, 1u );
    ASSERT_NE( decoded.body.system_info_response.diagnostic_data.data, nullptr );
    EXPECT_EQ( decoded.body.system_info_response.diagnostic_data.data[0], 0xaau );
    EXPECT_EQ( decoded.body.system_info_response.firmware_git_hash.size, 0u );
    EXPECT_EQ( decoded.body.system_info_response.firmware_git_hash.data, nullptr );
}

TEST( ApplicationCodecDecode, Uint16PayloadLimitDoesNotOverflowLengthArithmetic )
{
    auto context = MakeCodecContext( HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE );
    std::vector<std::uint8_t> bytes( HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE, 0u );
    bytes[0]  = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MAJOR );
    bytes[1]  = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MINOR );
    bytes[19] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST );
    bytes[20] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC );
    bytes[21] = 0xffu;
    bytes[22] = 0xffu;
    bytes[23] = 1u;
    bytes[24] = static_cast<std::uint8_t>( HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC );
    ExpectDecodeFailurePublishesNothing( context, bytes.data(), bytes.size(),
                                         HIL_APPLICATION_STATUS_MALFORMED_MESSAGE, true );
}

TEST( ApplicationCodecEncode, CapacityBoundariesPublishSizeOnlyOnSuccess )
{
    const auto                    context  = MakeCodecContext();
    const auto                    message  = BasicSystemInfoRequest();
    constexpr std::size_t         complete = 25u;
    std::array<std::uint8_t, 64u> buffer{};

    for ( const auto capacity :
          std::array<std::size_t, 4u>{ 0u, kHeaderSize - 1u, kHeaderSize, complete - 1u } )
    {
        std::size_t output_size = 777u;
        EXPECT_NE( HIL_APPLICATION_Encode_Message( &context, &message, buffer.data(), capacity,
                                                   &output_size ),
                   HIL_APPLICATION_STATUS_OK );
        EXPECT_EQ( output_size, 0u );
    }

    for ( const auto capacity : std::array<std::size_t, 2u>{ complete, complete + 10u } )
    {
        std::size_t output_size = 0u;
        ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, buffer.data(), capacity,
                                                   &output_size ),
                   HIL_APPLICATION_STATUS_OK );
        EXPECT_EQ( output_size, complete );
    }
}

TEST( ApplicationCodecContext, DefaultAndReducedBoundsInitializeAndInvalidBoundsDoNot )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};
    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( config.max_encoded_message_size, HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE );
    ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );

    config.max_encoded_message_size = 128u;
    ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( context.config.max_encoded_message_size, 128u );

    config.max_encoded_message_size = 24u;
    context.initialized             = 1u;
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( context.initialized, 0u );

    config.max_encoded_message_size = HIL_APPLICATION_MIN_COMPLETE_MESSAGE_SIZE;
    ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( context.config.max_encoded_message_size, 25u );

    config.max_encoded_message_size = HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE + 1u;
    context.initialized             = 1u;
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_INVALID_LENGTH );
    EXPECT_EQ( context.initialized, 0u );
}

TEST( ApplicationCodecContext, AliasedDefaultConfigurationInitializesAndPreservesFields )
{
    HIL_Application_Context_T context{};
    ASSERT_EQ( HIL_APPLICATION_Default_Config( &context.config ), HIL_APPLICATION_STATUS_OK );
    const HIL_Application_Config_T expected = context.config;

    ASSERT_EQ( HIL_APPLICATION_Init( &context, &context.config ), HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( context.initialized, 1u );
    EXPECT_EQ( context.config.max_encoded_message_size, expected.max_encoded_message_size );
    EXPECT_EQ( context.config.max_variable_data_size, expected.max_variable_data_size );
    EXPECT_EQ( context.config.max_variable_transfers_per_tick,
               expected.max_variable_transfers_per_tick );
    EXPECT_EQ( context.config.max_expected_tick_count, expected.max_expected_tick_count );
}

TEST( ApplicationCodecContext, InvalidAliasedConfigurationFailsAndClearsContext )
{
    HIL_Application_Context_T context{};
    ASSERT_EQ( HIL_APPLICATION_Default_Config( &context.config ), HIL_APPLICATION_STATUS_OK );
    context.config.max_variable_data_size = HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE + 1u;
    context.initialized                   = 1u;

    EXPECT_EQ( HIL_APPLICATION_Init( &context, &context.config ),
               HIL_APPLICATION_STATUS_INVALID_COUNT );
    EXPECT_EQ( context.initialized, 0u );
    EXPECT_EQ( context.config.max_encoded_message_size, 0u );
    EXPECT_EQ( context.config.max_variable_data_size, 0u );
    EXPECT_EQ( context.config.max_variable_transfers_per_tick, 0u );
    EXPECT_EQ( context.config.max_expected_tick_count, 0u );
}

TEST( ApplicationCodecContext, NonAliasedConfigurationIsStillCopiedOnSuccessfulInitialization )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};
    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_encoded_message_size        = 400u;
    config.max_variable_data_size          = 32u;
    config.max_variable_transfers_per_tick = 3u;
    config.max_expected_tick_count         = 123u;

    ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    config = HIL_Application_Config_T{};

    EXPECT_EQ( context.initialized, 1u );
    EXPECT_EQ( context.config.max_encoded_message_size, 400u );
    EXPECT_EQ( context.config.max_variable_data_size, 32u );
    EXPECT_EQ( context.config.max_variable_transfers_per_tick, 3u );
    EXPECT_EQ( context.config.max_expected_tick_count, 123u );
}

TEST( ApplicationCodecContext,
      NullConfigurationLeavesExistingContextDeterministicallyUninitialized )
{
    HIL_Application_Context_T context{};
    context.initialized                     = 1u;
    context.config.max_encoded_message_size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Init( &context, nullptr ), HIL_APPLICATION_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( context.initialized, 0u );
    EXPECT_EQ( context.config.max_encoded_message_size, 0u );
}

TEST( ApplicationCodecValidation, ConfiguredExpectedTickLimitIsEnforced )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};
    ASSERT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_expected_tick_count = 1u;
    ASSERT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );

    HIL_Application_Message_T message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id = 1u;
    message.body.test_configuration.tick_duration_us.microseconds = 10000u;
    message.body.test_configuration.expected_tick_count           = 2u;

    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );

    std::array<std::uint8_t, 512u> encoded{};
    std::size_t                    encoded_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( encoded_size, 0u );
}

TEST( ApplicationCodecValidation, NonEmptyTypedByteSpanRequiresDataPointer )
{
    const auto                context = MakeCodecContext();
    HIL_Application_Message_T message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    message.has_test_id = 0u;
    message.body.system_info_response.application_protocol_major = 0u;
    message.body.system_info_response.application_protocol_minor = 1u;
    message.body.system_info_response.application_protocol_patch = 0u;
    message.body.system_info_response.diagnostic_data.size       = 1u;
    message.body.system_info_response.diagnostic_data.data       = nullptr;

    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );

    std::array<std::uint8_t, 64u> encoded{};
    std::size_t                   encoded_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( encoded_size, 0u );

    HIL_Application_Message_T configuration{};
    configuration.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION;
    configuration.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    configuration.has_test_id = 1u;
    configuration.body.test_configuration.tick_duration_us.microseconds = 10000u;
    configuration.body.test_configuration.expected_tick_count           = 1u;
    configuration.body.test_configuration.extension_data.size           = 1u;
    configuration.body.test_configuration.extension_data.data           = nullptr;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &configuration ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
}

TEST( ApplicationCodecValidation, ReservedControlFlagsMustBeZero )
{
    const auto                    context = MakeCodecContext();
    std::array<std::uint8_t, 64u> encoded{};

    HIL_Application_Message_T execution{};
    execution.type                           = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    execution.subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    execution.has_test_id                    = 1u;
    execution.body.execution_control.command = HIL_APPLICATION_CONTROL_START;
    execution.body.execution_control.flags   = 1u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &execution ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    std::size_t encoded_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &execution, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( encoded_size, 0u );

    HIL_Application_Message_T global{};
    global.type                        = HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL;
    global.subtype                     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    global.has_test_id                 = 0u;
    global.body.global_control.command = HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION;
    global.body.global_control.flags   = 1u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &global ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    encoded_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &global, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( encoded_size, 0u );

    auto encoded_execution = ExecutionControlGoldenWithTestId();
    encoded_execution[24]  = 1u;
    ExpectDecodeFailurePublishesNothing( context, encoded_execution.data(),
                                         encoded_execution.size(),
                                         HIL_APPLICATION_STATUS_VALIDATION_FAILED, true );

    std::array<std::uint8_t, 28u> encoded_global{
        0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x14u,
        0x00u, 0x05u, 0x00u, 0x01u, 0x01u, 0x00u, 0x00u, 0x00u,
    };
    ExpectDecodeFailurePublishesNothing( context, encoded_global.data(), encoded_global.size(),
                                         HIL_APPLICATION_STATUS_VALIDATION_FAILED, true );
}

TEST( ApplicationCodecValidation, SystemInfoResponseVersionFieldsMustMatchCompiledProtocol )
{
    const auto                context = MakeCodecContext();
    HIL_Application_Message_T message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    message.has_test_id = 0u;
    message.body.system_info_response.application_protocol_major = 0u;
    message.body.system_info_response.application_protocol_minor = 1u;
    message.body.system_info_response.application_protocol_patch = 1u;

    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );

    std::array<std::uint8_t, 64u> encoded{};
    std::size_t                   encoded_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( encoded_size, 0u );

    const auto                    valid = SystemInfoResponseGoldenWithDiagnostic();
    std::array<std::uint8_t, 37u> mismatched_wire_version{};
    std::copy_n( valid.begin(), 35u, mismatched_wire_version.begin() );
    mismatched_wire_version[kPayloadLengthOffset] = 14u;
    mismatched_wire_version[27]                   = 1u;
    mismatched_wire_version[35]                   = 0u;
    mismatched_wire_version[36]                   = 0u;
    ExpectDecodeFailurePublishesNothing( context, mismatched_wire_version.data(),
                                         mismatched_wire_version.size(),
                                         HIL_APPLICATION_STATUS_VALIDATION_FAILED, true );
}

TEST( ApplicationCodecDeferredFamilies, UnfinishedVariableInstructionRemainsCleanlyNotImplemented )
{
    const auto                context = MakeCodecContext();
    HIL_Application_Message_T message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id = 1u;

    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );

    std::array<std::uint8_t, 64u> encoded{};
    std::size_t                   encoded_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
    EXPECT_EQ( encoded_size, 0u );

    encoded_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &encoded_size ),
               HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
    EXPECT_EQ( encoded_size, 0u );
}

TEST( ApplicationCodecFacade, UninitializedContextClearsAllPublishableOutputMetadata )
{
    HIL_Application_Context_T     context{};
    const auto                    message = BasicSystemInfoRequest();
    const auto                    golden  = BasicSystemInfoGolden();
    std::array<std::uint8_t, 64u> buffer{};

    std::size_t size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &size ),
               HIL_APPLICATION_STATUS_UNINITIALIZED );
    EXPECT_EQ( size, 0u );

    size = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, buffer.data(), buffer.size(), &size ),
        HIL_APPLICATION_STATUS_UNINITIALIZED );
    EXPECT_EQ( size, 0u );

    HIL_Application_Message_T decoded{};
    decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    std::size_t used = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, golden.data(), golden.size(), &decoded,
                                               nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_UNINITIALIZED );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );

    size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, golden.data(), golden.size(), &size ),
               HIL_APPLICATION_STATUS_UNINITIALIZED );
    EXPECT_EQ( size, 0u );

    size = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Validate_Encoded_Message( &context, golden.data(), golden.size(), &size ),
        HIL_APPLICATION_STATUS_UNINITIALIZED );
    EXPECT_EQ( size, 0u );
}

TEST( ApplicationCodecDecodeStorage, FixedFamiliesRequireExactPayloadWidth )
{
    const auto context = MakeCodecContext();

    for ( const auto& fixed : kFixedBodyWidths )
    {
        auto        exact   = FixedBodyEnvelope( fixed.type, fixed.payload_size );
        std::size_t storage = 99u;
        EXPECT_EQ(
            HIL_APPLICATION_Decode_Storage_Size( &context, exact.data(), exact.size(), &storage ),
            HIL_APPLICATION_STATUS_OK );
        EXPECT_EQ( storage, 0u );

        const std::array<std::size_t, 2u> malformed_sizes = { fixed.payload_size - 1u,
                                                              fixed.payload_size + 1u };
        for ( const auto payload_size : malformed_sizes )
        {
            auto malformed = FixedBodyEnvelope( fixed.type, payload_size );

            storage = 99u;
            EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, malformed.data(),
                                                            malformed.size(), &storage ),
                       HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
            EXPECT_EQ( storage, 0u );

            HIL_Application_Message_T decoded{};
            decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
            std::size_t used = 99u;
            EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, malformed.data(), malformed.size(),
                                                       &decoded, nullptr, 0u, &used ),
                       HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
            EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
            EXPECT_EQ( used, 0u );
        }
    }
}

TEST( ApplicationCodecFacade, SizingStorageAndEncodedValidationUseCommonEnvelope )
{
    const auto  context      = MakeCodecContext();
    const auto  message      = BasicSystemInfoRequest();
    std::size_t encoded_size = 999u;
    ASSERT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, 25u );

    const auto  golden  = BasicSystemInfoGolden();
    std::size_t storage = 99u;
    ASSERT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, golden.data(), golden.size(), &storage ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( storage, 0u );

    storage = 99u;
    ASSERT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, golden.data(), golden.size(),
                                                         &storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( storage, 0u );
}
