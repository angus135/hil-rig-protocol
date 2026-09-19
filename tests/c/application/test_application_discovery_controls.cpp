#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"
#include "hil_rig_protocol/version.h"

namespace {
HIL_Application_Context_T MakeContext( std::size_t maximum = 1024u )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};
    EXPECT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_encoded_message_size = maximum;
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    return context;
}

HIL_Application_Message_T MakeRequest()
{
    HIL_Application_Message_T message{};
    message.type    = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST;
    message.subtype = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    message.body.system_info_request.request_firmware_git_hash = 1u;
    message.body.system_info_request.query = HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC;
    message.body.system_info_request.application_protocol_major = HIL_RIG_PROTOCOL_VERSION_MAJOR;
    message.body.system_info_request.application_protocol_minor = HIL_RIG_PROTOCOL_VERSION_MINOR;
    message.body.system_info_request.application_protocol_patch = HIL_RIG_PROTOCOL_VERSION_PATCH;
    return message;
}

HIL_Application_Test_Id_T MakeTestId()
{
    HIL_Application_Test_Id_T id{};
    for ( std::size_t index = 0u; index < HIL_APPLICATION_TEST_ID_SIZE; ++index )
    {
        id.bytes[index] = static_cast<std::uint8_t>( index + 1u );
    }
    return id;
}

std::vector<std::uint8_t> MakeResponseWire( std::size_t payload_size )
{
    std::vector<std::uint8_t> wire( HIL_APPLICATION_HEADER_SIZE_BYTES + payload_size, 0u );
    wire[0]  = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MAJOR );
    wire[1]  = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MINOR );
    wire[19] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE );
    wire[20] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC );
    wire[21] = static_cast<std::uint8_t>( payload_size & 0xffu );
    wire[22] = static_cast<std::uint8_t>( ( payload_size >> 8u ) & 0xffu );
    if ( payload_size >= 4u )
    {
        wire[23] = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MAJOR );
        wire[25] = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MINOR );
    }
    return wire;
}

void ExpectResponseFailure( const HIL_Application_Context_T& context,
                            const std::vector<std::uint8_t>& wire,
                            HIL_Application_Status_T         expected_status )
{
    std::size_t storage_size = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &storage_size ),
        expected_status );
    EXPECT_EQ( storage_size, 0u );

    storage_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(),
                                                         &storage_size ),
               expected_status );
    EXPECT_EQ( storage_size, 0u );

    HIL_Application_Message_T decoded{};
    decoded.type             = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    std::size_t used_storage = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               nullptr, 0u, &used_storage ),
               expected_status );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used_storage, 0u );
}

void ExpectMalformedResponse( const HIL_Application_Context_T& context,
                              const std::vector<std::uint8_t>& wire )
{
    ExpectResponseFailure( context, wire, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
}
}  // namespace

static_assert( HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL == 20 );

TEST( ApplicationDiscovery, RequestUsesApprovedEightByteLayoutAndExactVersionGate )
{
    const auto                    context = MakeContext();
    const auto                    request = MakeRequest();
    std::array<std::uint8_t, 31u> encoded{};
    std::size_t                   output_size = 0u;

    ASSERT_EQ( HIL_APPLICATION_Encoded_Size( &context, &request, &output_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( output_size, encoded.size() );
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &request, encoded.data(), encoded.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded[21], 8u );
    EXPECT_EQ( encoded[22], 0u );
    EXPECT_EQ( encoded[23], 1u );
    EXPECT_EQ( encoded[24], HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC );
    EXPECT_EQ( encoded[25], HIL_RIG_PROTOCOL_VERSION_MAJOR );
    EXPECT_EQ( encoded[27], HIL_RIG_PROTOCOL_VERSION_MINOR );
    EXPECT_EQ( encoded[29], HIL_RIG_PROTOCOL_VERSION_PATCH );
    EXPECT_EQ( HIL_APPLICATION_Check_Protocol_Version( HIL_RIG_PROTOCOL_VERSION_MAJOR,
                                                       HIL_RIG_PROTOCOL_VERSION_MINOR,
                                                       HIL_RIG_PROTOCOL_VERSION_PATCH ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( HIL_APPLICATION_Check_Protocol_Version( 1u, HIL_RIG_PROTOCOL_VERSION_MINOR,
                                                       HIL_RIG_PROTOCOL_VERSION_PATCH ),
               HIL_APPLICATION_STATUS_VERSION_MISMATCH );
    EXPECT_EQ( HIL_APPLICATION_Check_Protocol_Version( HIL_RIG_PROTOCOL_VERSION_MAJOR, 4u,
                                                       HIL_RIG_PROTOCOL_VERSION_PATCH ),
               HIL_APPLICATION_STATUS_VERSION_MISMATCH );
    EXPECT_EQ( HIL_APPLICATION_Check_Protocol_Version( HIL_RIG_PROTOCOL_VERSION_MAJOR,
                                                       HIL_RIG_PROTOCOL_VERSION_MINOR, 1u ),
               HIL_APPLICATION_STATUS_VERSION_MISMATCH );

    auto inconsistent                                                = request;
    inconsistent.body.system_info_request.application_protocol_patch = 1u;
    output_size                                                      = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &inconsistent, &output_size ),
               HIL_APPLICATION_STATUS_VERSION_MISMATCH );
    EXPECT_EQ( output_size, 0u );
    output_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &inconsistent, encoded.data(),
                                               encoded.size(), &output_size ),
               HIL_APPLICATION_STATUS_VERSION_MISMATCH );
    EXPECT_EQ( output_size, 0u );
}

TEST( ApplicationDiscovery, ForeignDiscoveryDecodesButBodyMustAgreeWithEnvelope )
{
    const auto                    context = MakeContext();
    const auto                    request = MakeRequest();
    std::array<std::uint8_t, 31u> encoded{};
    HIL_Application_Message_T     decoded{};
    std::size_t                   output_size  = 0u;
    std::size_t                   used_storage = 99u;

    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &request, encoded.data(), encoded.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_OK );
    encoded[1]  = 4u;
    encoded[27] = 4u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), output_size, &decoded,
                                               nullptr, 0u, &used_storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.body.system_info_request.application_protocol_minor, 4u );
    EXPECT_EQ( HIL_APPLICATION_Check_Protocol_Version(
                   decoded.body.system_info_request.application_protocol_major,
                   decoded.body.system_info_request.application_protocol_minor,
                   decoded.body.system_info_request.application_protocol_patch ),
               HIL_APPLICATION_STATUS_VERSION_MISMATCH );

    encoded[27]  = static_cast<std::uint8_t>( HIL_RIG_PROTOCOL_VERSION_MINOR );
    used_storage = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), output_size, &decoded,
                                               nullptr, 0u, &used_storage ),
               HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used_storage, 0u );
}

TEST( ApplicationDiscovery, ResponseScansBothMaximumSpansWithoutAllocation )
{
    std::array<std::uint8_t, 255u> diagnostic{};
    std::array<std::uint8_t, 255u> git_hash{};
    std::array<std::uint8_t, 547u> encoded{};
    std::array<std::uint8_t, 510u> storage{};
    HIL_Application_Message_T      message{};
    HIL_Application_Message_T      decoded{};
    std::size_t                    encoded_size     = 0u;
    std::size_t                    required_storage = 0u;
    std::size_t                    used_storage     = 0u;

    diagnostic.fill( 0xa5u );
    git_hash.fill( 0x5au );
    message.type    = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE;
    message.subtype = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    message.body.system_info_response.application_protocol_major = HIL_RIG_PROTOCOL_VERSION_MAJOR;
    message.body.system_info_response.application_protocol_minor = HIL_RIG_PROTOCOL_VERSION_MINOR;
    message.body.system_info_response.application_protocol_patch = HIL_RIG_PROTOCOL_VERSION_PATCH;
    message.body.system_info_response.diagnostic_data            = { diagnostic.data(), 255u };
    message.body.system_info_response.firmware_git_hash          = { git_hash.data(), 255u };

    const auto default_context = MakeContext( HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE );
    encoded_size               = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &default_context, &message, &encoded_size ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( encoded_size, 0u );

    const auto context = MakeContext( 547u );

    ASSERT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, encoded.size() );
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, encoded.data(), encoded_size,
                                                    &required_storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required_storage, storage.size() );
    std::size_t validation_storage = 99u;
    ASSERT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, encoded.data(), encoded_size,
                                                         &validation_storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( validation_storage, storage.size() );
    used_storage = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded,
                                               storage.data(), storage.size() - 1u, &used_storage ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used_storage, 0u );
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded,
                                               storage.data(), storage.size(), &used_storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used_storage, storage.size() );
    EXPECT_EQ( decoded.body.system_info_response.diagnostic_data.data, storage.data() );
    EXPECT_EQ( decoded.body.system_info_response.firmware_git_hash.data, storage.data() + 255u );
}

TEST( ApplicationDiscovery, ResponseScannerRejectsEveryMalformedSpanBoundary )
{
    const auto context = MakeContext();

    const auto truncated_numeric_fields = MakeResponseWire( 11u );
    ExpectMalformedResponse( context, truncated_numeric_fields );

    const auto missing_diagnostic_length = MakeResponseWire( 12u );
    ExpectMalformedResponse( context, missing_diagnostic_length );

    const auto missing_git_length = MakeResponseWire( 13u );
    ExpectMalformedResponse( context, missing_git_length );

    auto short_diagnostic = MakeResponseWire( 14u );
    short_diagnostic[35]  = 2u;
    short_diagnostic[36]  = 0xaau;
    ExpectMalformedResponse( context, short_diagnostic );

    auto short_git = MakeResponseWire( 15u );
    short_git[35]  = 0u;
    short_git[36]  = 2u;
    short_git[37]  = 0xbbu;
    ExpectMalformedResponse( context, short_git );

    auto trailing = MakeResponseWire( 15u );
    trailing[35]  = 0u;
    trailing[36]  = 0u;
    trailing[37]  = 0xccu;
    ExpectMalformedResponse( context, trailing );
}

TEST( ApplicationDiscovery, TruncatedSpansTakePrecedenceOverConfiguredLimits )
{
    auto limited_config                   = MakeContext().config;
    limited_config.max_variable_data_size = 2u;
    HIL_Application_Context_T context{};
    ASSERT_EQ( HIL_APPLICATION_Init( &context, &limited_config ), HIL_APPLICATION_STATUS_OK );

    auto truncated_diagnostic = MakeResponseWire( 15u );
    truncated_diagnostic[35]  = 4u;
    truncated_diagnostic[36]  = 0xaau;
    truncated_diagnostic[37]  = 0xbbu;
    ExpectResponseFailure( context, truncated_diagnostic,
                           HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );

    auto truncated_git = MakeResponseWire( 15u );
    truncated_git[35]  = 0u;
    truncated_git[36]  = 4u;
    truncated_git[37]  = 0xccu;
    ExpectResponseFailure( context, truncated_git, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
}

TEST( ApplicationDiscovery, CompleteSpansOverConfiguredLimitsFailValidation )
{
    auto limited_config                   = MakeContext().config;
    limited_config.max_variable_data_size = 2u;
    HIL_Application_Context_T context{};
    ASSERT_EQ( HIL_APPLICATION_Init( &context, &limited_config ), HIL_APPLICATION_STATUS_OK );

    auto complete_diagnostic = MakeResponseWire( 17u );
    complete_diagnostic[35]  = 3u;
    complete_diagnostic[36]  = 0xaau;
    complete_diagnostic[37]  = 0xbbu;
    complete_diagnostic[38]  = 0xccu;
    complete_diagnostic[39]  = 0u;
    ExpectResponseFailure( context, complete_diagnostic, HIL_APPLICATION_STATUS_VALIDATION_FAILED );

    auto complete_git = MakeResponseWire( 17u );
    complete_git[35]  = 0u;
    complete_git[36]  = 3u;
    complete_git[37]  = 0xddu;
    complete_git[38]  = 0xeeu;
    complete_git[39]  = 0xffu;
    ExpectResponseFailure( context, complete_git, HIL_APPLICATION_STATUS_VALIDATION_FAILED );
}

TEST( ApplicationDiscovery, EmptyAndGitOnlyResponseSpansHaveExactStorageAndConfiguredLimits )
{
    const auto                    context = MakeContext();
    std::array<std::uint8_t, 64u> encoded{};
    std::array<std::uint8_t, 3u>  storage{};
    HIL_Application_Message_T     message{};
    HIL_Application_Message_T     decoded{};
    std::size_t                   encoded_size     = 0u;
    std::size_t                   required_storage = 99u;
    std::size_t                   used_storage     = 99u;

    message.type    = HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE;
    message.subtype = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    message.body.system_info_response.application_protocol_major = HIL_RIG_PROTOCOL_VERSION_MAJOR;
    message.body.system_info_response.application_protocol_minor = HIL_RIG_PROTOCOL_VERSION_MINOR;
    message.body.system_info_response.application_protocol_patch = HIL_RIG_PROTOCOL_VERSION_PATCH;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, 37u );
    ASSERT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, encoded.data(), encoded_size,
                                                    &required_storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required_storage, 0u );
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded,
                                               nullptr, 0u, &used_storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.body.system_info_response.diagnostic_data.data, nullptr );
    EXPECT_EQ( decoded.body.system_info_response.firmware_git_hash.data, nullptr );

    const std::array<std::uint8_t, 3u> git_hash{ 0x61u, 0x62u, 0x63u };
    message.body.system_info_response.firmware_git_hash = {
        git_hash.data(), static_cast<std::uint8_t>( git_hash.size() ) };
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, 40u );
    ASSERT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, encoded.data(), encoded_size,
                                                    &required_storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required_storage, storage.size() );
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded_size, &decoded,
                                               storage.data(), storage.size(), &used_storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.body.system_info_response.diagnostic_data.data, nullptr );
    EXPECT_EQ( decoded.body.system_info_response.firmware_git_hash.data, storage.data() );

    auto limited_config                   = context.config;
    limited_config.max_variable_data_size = 2u;
    HIL_Application_Context_T limited{};
    ASSERT_EQ( HIL_APPLICATION_Init( &limited, &limited_config ), HIL_APPLICATION_STATUS_OK );
    required_storage = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &limited, encoded.data(), encoded_size,
                                                    &required_storage ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( required_storage, 0u );
}

TEST( ApplicationDiscovery, InvalidQuerySubtypeAndTestIdAreRejected )
{
    const auto                    context = MakeContext();
    auto                          request = MakeRequest();
    std::array<std::uint8_t, 31u> encoded{};
    HIL_Application_Message_T     decoded{};
    std::size_t                   output_size  = 99u;
    std::size_t                   used_storage = 99u;

    request.body.system_info_request.query = HIL_APPLICATION_SYSTEM_INFO_QUERY_INVALID;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &request, &output_size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( output_size, 0u );

    request         = MakeRequest();
    request.subtype = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &request, &output_size ),
               HIL_APPLICATION_STATUS_INVALID_SUBTYPE );

    request             = MakeRequest();
    request.has_test_id = 1u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &request, &output_size ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID );

    request = MakeRequest();
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &request, encoded.data(), encoded.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_OK );
    encoded[24]  = 0u;
    used_storage = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), output_size, &decoded,
                                               nullptr, 0u, &used_storage ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used_storage, 0u );
}

TEST( ApplicationDiscoveryControls, DecodeStorageRejectsFamilyInconsistentTestIdPresence )
{
    const auto                    context = MakeContext();
    const auto                    request = MakeRequest();
    std::array<std::uint8_t, 31u> discovery{};
    std::array<std::uint8_t, 28u> control{};
    std::size_t                   output_size  = 0u;
    std::size_t                   storage_size = 99u;

    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &request, discovery.data(),
                                               discovery.size(), &output_size ),
               HIL_APPLICATION_STATUS_OK );
    discovery[2] = 1u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, discovery.data(), discovery.size(),
                                                    &storage_size ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID );
    EXPECT_EQ( storage_size, 0u );

    HIL_Application_Message_T execution{};
    execution.type                           = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    execution.subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    execution.has_test_id                    = 1u;
    execution.test_id                        = MakeTestId();
    execution.body.execution_control.command = HIL_APPLICATION_CONTROL_START;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &execution, control.data(), control.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_OK );
    control[2]   = 0u;
    storage_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, control.data(), control.size(),
                                                    &storage_size ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID );
    EXPECT_EQ( storage_size, 0u );
}

TEST( ApplicationControls, StartAbortAndResetRoundTripWithExactFiveByteBodies )
{
    const auto                    context = MakeContext();
    HIL_Application_Message_T     execution{};
    HIL_Application_Message_T     decoded{};
    std::array<std::uint8_t, 28u> encoded{};
    std::size_t                   output_size  = 0u;
    std::size_t                   used_storage = 0u;

    execution.type        = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    execution.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    execution.has_test_id = 1u;
    execution.test_id     = MakeTestId();
    for ( const auto command : { HIL_APPLICATION_CONTROL_START, HIL_APPLICATION_CONTROL_ABORT } )
    {
        execution.body.execution_control.command = command;
        ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &execution, encoded.data(),
                                                   encoded.size(), &output_size ),
                   HIL_APPLICATION_STATUS_OK );
        EXPECT_EQ( output_size, encoded.size() );
        EXPECT_EQ( encoded[2], 1u );
        EXPECT_EQ( encoded[19], HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL );
        EXPECT_EQ( encoded[20], HIL_APPLICATION_MESSAGE_SUBTYPE_NONE );
        EXPECT_EQ( encoded[21], 5u );
        EXPECT_EQ( encoded[22], 0u );
        EXPECT_EQ( encoded[23], command );
        EXPECT_EQ( encoded[24], 0u );
        EXPECT_EQ( encoded[25], 0u );
        EXPECT_EQ( encoded[26], 0u );
        EXPECT_EQ( encoded[27], 0u );
        ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), output_size, &decoded,
                                                   nullptr, 0u, &used_storage ),
                   HIL_APPLICATION_STATUS_OK );
        EXPECT_EQ( decoded.body.execution_control.command, command );
    }

    execution.type                        = HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL;
    execution.has_test_id                 = 0u;
    execution.body.global_control.command = HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &execution, encoded.data(), encoded.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( output_size, encoded.size() );
    EXPECT_EQ( encoded[2], 0u );
    for ( std::size_t index = 3u; index < 19u; ++index )
    {
        EXPECT_EQ( encoded[index], 0u );
    }
    EXPECT_EQ( encoded[19], HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL );
    EXPECT_EQ( encoded[20], HIL_APPLICATION_MESSAGE_SUBTYPE_NONE );
    EXPECT_EQ( encoded[21], 5u );
    EXPECT_EQ( encoded[22], 0u );
    EXPECT_EQ( encoded[23], HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION );
    EXPECT_EQ( encoded[24], 0u );
    EXPECT_EQ( encoded[25], 0u );
    EXPECT_EQ( encoded[26], 0u );
    EXPECT_EQ( encoded[27], 0u );
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), output_size, &decoded,
                                               nullptr, 0u, &used_storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.body.global_control.command,
               HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION );
}

TEST( ApplicationControls, RejectReservedCommandsAndNonzeroFlags )
{
    const auto                    context = MakeContext();
    HIL_Application_Message_T     execution{};
    HIL_Application_Message_T     global{};
    HIL_Application_Message_T     decoded{};
    std::array<std::uint8_t, 28u> encoded{};
    std::size_t                   output_size  = 0u;
    std::size_t                   used_storage = 99u;

    execution.type                           = HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL;
    execution.subtype                        = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    execution.has_test_id                    = 1u;
    execution.test_id                        = MakeTestId();
    execution.body.execution_control.command = HIL_APPLICATION_CONTROL_START;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &execution, encoded.data(), encoded.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_OK );

    encoded[23] = 0u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), output_size, &decoded,
                                               nullptr, 0u, &used_storage ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used_storage, 0u );

    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &execution, encoded.data(), encoded.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_OK );
    encoded[24]  = 1u;
    used_storage = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), output_size, &decoded,
                                               nullptr, 0u, &used_storage ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used_storage, 0u );

    global.type                        = HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL;
    global.subtype                     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    global.body.global_control.command = HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &global, encoded.data(), encoded.size(),
                                               &output_size ),
               HIL_APPLICATION_STATUS_OK );
    encoded[23]  = 2u;
    used_storage = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), output_size, &decoded,
                                               nullptr, 0u, &used_storage ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used_storage, 0u );
}
