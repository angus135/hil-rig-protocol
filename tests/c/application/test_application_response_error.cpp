#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"
#include "hil_rig_protocol/version.h"

namespace {

constexpr std::size_t kPayloadOffset = HIL_APPLICATION_HEADER_SIZE_BYTES;

HIL_Application_Context_T MakeContext( std::size_t   max_variable_size = 255u,
                                       std::uint32_t max_tick_count    = 100u )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};
    EXPECT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_variable_data_size  = max_variable_size;
    config.max_expected_tick_count = max_tick_count;
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    return context;
}

HIL_Application_Test_Id_T TestId()
{
    HIL_Application_Test_Id_T               test_id{};
    constexpr std::array<std::uint8_t, 16u> bytes{
        0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u,
        0x18u, 0x19u, 0x1au, 0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu,
    };
    std::copy( bytes.begin(), bytes.end(), test_id.bytes );
    return test_id;
}

HIL_Application_Message_T Response( HIL_Application_Response_Scope_T scope )
{
    HIL_Application_Message_T message{};
    message.type                = HIL_APPLICATION_MESSAGE_TYPE_RESPONSE;
    message.subtype             = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id         = scope == HIL_APPLICATION_RESPONSE_SCOPE_GLOBAL_CONTROL ? 0u : 1u;
    message.test_id             = TestId();
    message.body.response.scope = scope;
    message.body.response.outcome                = HIL_APPLICATION_RESPONSE_OUTCOME_FAILED;
    message.body.response.reason                 = HIL_APPLICATION_RESPONSE_REASON_INTERNAL_FAILURE;
    message.body.response.tick_number            = 0x12345678u;
    message.body.response.control_command        = HIL_APPLICATION_CONTROL_ABORT;
    message.body.response.global_control_command = HIL_APPLICATION_GLOBAL_CONTROL_INVALID;
    message.body.response.detail                 = 0x89abcdefu;
    return message;
}

HIL_Application_Message_T Error( bool with_test_id, bool with_tick, const std::uint8_t* diagnostic,
                                 std::uint8_t diagnostic_size )
{
    HIL_Application_Message_T message{};
    message.type                       = HIL_APPLICATION_MESSAGE_TYPE_ERROR;
    message.subtype                    = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id                = with_test_id ? 1u : 0u;
    message.test_id                    = TestId();
    message.body.error.category        = HIL_APPLICATION_ERROR_CATEGORY_PROTOCOL;
    message.body.error.recoverable     = 1u;
    message.body.error.has_tick_number = with_tick ? 1u : 0u;
    message.body.error.tick_number     = with_tick ? 7u : 0u;
    message.body.error.detail          = 0x01020304u;
    message.body.error.diagnostic_data = { diagnostic, diagnostic_size };
    return message;
}

std::vector<std::uint8_t> Encode( const HIL_Application_Context_T& context,
                                  const HIL_Application_Message_T& message )
{
    std::size_t size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &size ),
               HIL_APPLICATION_STATUS_OK );
    std::vector<std::uint8_t> wire( size );
    std::size_t               used = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, wire.data(), wire.size(), &used ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, size );
    return wire;
}

HIL_Application_Message_T Decode( const HIL_Application_Context_T& context,
                                  const std::vector<std::uint8_t>& wire,
                                  std::vector<std::uint8_t>&       storage )
{
    std::size_t required = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required, storage.size() );
    HIL_Application_Message_T decoded{};
    std::size_t               used = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               storage.empty() ? nullptr : storage.data(),
                                               storage.size(), &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, storage.size() );
    return decoded;
}

void ExpectEncodedFailureClearsOutputs( const HIL_Application_Context_T& context,
                                        const std::vector<std::uint8_t>& wire,
                                        HIL_Application_Status_T         expected )
{
    std::size_t storage_size = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &storage_size ),
        expected );
    EXPECT_EQ( storage_size, 0u );
    storage_size = 99u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(),
                                                         &storage_size ),
               expected );
    EXPECT_EQ( storage_size, 0u );
    std::array<std::uint8_t, 255u> storage{};
    HIL_Application_Message_T      decoded{};
    decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    std::size_t used = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               storage.data(), storage.size(), &used ),
               expected );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );
}

}  // namespace

TEST( ApplicationResponse, ExactGoldenVectorAndRoundTrip )
{
    const auto                          context = MakeContext();
    const auto                          message = Response( HIL_APPLICATION_RESPONSE_SCOPE_TICK );
    const std::array<std::uint8_t, 36u> expected{
        0x00u, 0x02u, 0x01u, 0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u,
        0x19u, 0x1au, 0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu, 0x30u, 0x00u, 0x0du, 0x00u, 0x02u,
        0x04u, 0x09u, 0x78u, 0x56u, 0x34u, 0x12u, 0x02u, 0x00u, 0xefu, 0xcdu, 0xabu, 0x89u,
    };
    const auto wire = Encode( context, message );
    EXPECT_TRUE( std::equal( expected.begin(), expected.end(), wire.begin() ) );
    std::vector<std::uint8_t> storage;
    const auto                decoded = Decode( context, wire, storage );
    EXPECT_EQ( decoded.body.response.scope, message.body.response.scope );
    EXPECT_EQ( decoded.body.response.outcome, message.body.response.outcome );
    EXPECT_EQ( decoded.body.response.reason, message.body.response.reason );
    EXPECT_EQ( decoded.body.response.tick_number, message.body.response.tick_number );
    EXPECT_EQ( decoded.body.response.control_command, message.body.response.control_command );
    EXPECT_EQ( decoded.body.response.global_control_command,
               message.body.response.global_control_command );
    EXPECT_EQ( decoded.body.response.detail, message.body.response.detail );
}

TEST( ApplicationResponse, AcceptsEveryDefinedValueAndUnusualCombination )
{
    const auto           context = MakeContext();
    constexpr std::array scopes{
        HIL_APPLICATION_RESPONSE_SCOPE_TEST_CONFIGURATION,
        HIL_APPLICATION_RESPONSE_SCOPE_TICK,
        HIL_APPLICATION_RESPONSE_SCOPE_COMPLETE_TEST,
        HIL_APPLICATION_RESPONSE_SCOPE_EXECUTION_CONTROL,
        HIL_APPLICATION_RESPONSE_SCOPE_GLOBAL_CONTROL,
    };
    constexpr std::array outcomes{
        HIL_APPLICATION_RESPONSE_OUTCOME_ACCEPTED,
        HIL_APPLICATION_RESPONSE_OUTCOME_REJECTED,
        HIL_APPLICATION_RESPONSE_OUTCOME_COMPLETED,
        HIL_APPLICATION_RESPONSE_OUTCOME_FAILED,
    };
    constexpr std::array reasons{
        HIL_APPLICATION_RESPONSE_REASON_NONE,
        HIL_APPLICATION_RESPONSE_REASON_UNSUPPORTED,
        HIL_APPLICATION_RESPONSE_REASON_OPERATION_NOT_ALLOWED,
        HIL_APPLICATION_RESPONSE_REASON_INCONSISTENT_TEST_ID,
        HIL_APPLICATION_RESPONSE_REASON_INVALID_TICK,
        HIL_APPLICATION_RESPONSE_REASON_LENGTH_MISMATCH,
        HIL_APPLICATION_RESPONSE_REASON_STORAGE_UNAVAILABLE,
        HIL_APPLICATION_RESPONSE_REASON_VALIDATION_FAILED,
        HIL_APPLICATION_RESPONSE_REASON_HARDWARE_NOT_READY,
        HIL_APPLICATION_RESPONSE_REASON_INTERNAL_FAILURE,
    };
    for ( const auto scope : scopes )
    {
        const auto message = Response( scope );
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
                   HIL_APPLICATION_STATUS_OK );
        std::vector<std::uint8_t> storage;
        EXPECT_EQ( Decode( context, Encode( context, message ), storage ).type,
                   HIL_APPLICATION_MESSAGE_TYPE_RESPONSE );
    }
    auto message = Response( HIL_APPLICATION_RESPONSE_SCOPE_TEST_CONFIGURATION );
    for ( const auto outcome : outcomes )
    {
        message.body.response.outcome = outcome;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
                   HIL_APPLICATION_STATUS_OK );
    }
    for ( const auto reason : reasons )
    {
        message.body.response.reason = reason;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
                   HIL_APPLICATION_STATUS_OK );
    }
    for ( const auto command : { HIL_APPLICATION_CONTROL_INVALID, HIL_APPLICATION_CONTROL_START,
                                 HIL_APPLICATION_CONTROL_ABORT } )
    {
        message.body.response.control_command = command;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
                   HIL_APPLICATION_STATUS_OK );
    }
    for ( const auto command : { HIL_APPLICATION_GLOBAL_CONTROL_INVALID,
                                 HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION } )
    {
        message.body.response.global_control_command = command;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
                   HIL_APPLICATION_STATUS_OK );
    }
}

TEST( ApplicationResponse, RejectsSentinelsUnknownValuesLengthsAndWrongTestIdPresence )
{
    const auto context  = MakeContext();
    auto       message  = Response( HIL_APPLICATION_RESPONSE_SCOPE_TICK );
    message.has_test_id = 0u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID );
    message             = Response( HIL_APPLICATION_RESPONSE_SCOPE_GLOBAL_CONTROL );
    message.has_test_id = 1u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID );

    auto wire = Encode( context, Response( HIL_APPLICATION_RESPONSE_SCOPE_TICK ) );
    for ( const auto offset : { 0u, 1u } )
    {
        auto invalid                     = wire;
        invalid[kPayloadOffset + offset] = 0u;
        std::size_t required             = 99u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, invalid.data(),
                                                             invalid.size(), &required ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        EXPECT_EQ( required, 0u );
    }
    constexpr std::array<std::size_t, 5u>  offsets{ 0u, 1u, 2u, 7u, 8u };
    constexpr std::array<std::uint8_t, 2u> invalid_values{ 0xfeu, 0xffu };
    for ( const auto offset : offsets )
    {
        for ( const auto invalid : invalid_values )
        {
            auto malformed                     = wire;
            malformed[kPayloadOffset + offset] = invalid;
            std::size_t required               = 99u;
            EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, malformed.data(),
                                                                 malformed.size(), &required ),
                       HIL_APPLICATION_STATUS_VALIDATION_FAILED );
            EXPECT_EQ( required, 0u );
        }
    }
    auto undersized = wire;
    undersized[21]  = 12u;
    undersized.pop_back();
    ExpectEncodedFailureClearsOutputs( context, undersized,
                                       HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    auto oversized = wire;
    oversized[21]  = 14u;
    oversized.push_back( 0u );
    ExpectEncodedFailureClearsOutputs( context, oversized,
                                       HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
}

TEST( ApplicationErrorMessage, ExactGoldenAndAllThreeFormsRoundTrip )
{
    constexpr std::array<std::uint8_t, 4u> diagnostic{ 0xdeu, 0xadu, 0xbeu, 0xefu };
    constexpr std::uint8_t                 diagnostic_size = 4u;
    const auto                             context         = MakeContext();
    const auto global = Error( false, false, diagnostic.data(), diagnostic_size );
    const std::array<std::uint8_t, 39u> expected{
        0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x31u, 0x00u, 0x10u, 0x00u, 0x05u, 0x01u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x04u, 0x03u, 0x02u, 0x01u, 0x04u, 0xdeu, 0xadu, 0xbeu, 0xefu,
    };
    const auto wire = Encode( context, global );
    EXPECT_TRUE( std::equal( expected.begin(), expected.end(), wire.begin() ) );

    for ( const auto& form : { Error( false, false, diagnostic.data(), diagnostic_size ),
                               Error( true, false, diagnostic.data(), diagnostic_size ),
                               Error( true, true, diagnostic.data(), diagnostic_size ) } )
    {
        auto                      form_wire = Encode( context, form );
        std::vector<std::uint8_t> storage( diagnostic.size() );
        const auto                decoded = Decode( context, form_wire, storage );
        EXPECT_EQ( decoded.has_test_id, form.has_test_id );
        EXPECT_EQ( decoded.body.error.has_tick_number, form.body.error.has_tick_number );
        EXPECT_EQ( decoded.body.error.tick_number, form.body.error.tick_number );
        EXPECT_TRUE( std::equal( diagnostic.begin(), diagnostic.end(), storage.begin() ) );
    }
}

TEST( ApplicationErrorMessage, DiagnosticsAtEmptyConfiguredAndAbsoluteLimitsHaveExactStorage )
{
    std::array<std::uint8_t, 255u> diagnostic{};
    diagnostic.fill( 0xa5u );
    constexpr std::array<std::uint8_t, 3u> sizes{ 0u, 7u, 255u };
    for ( const auto size : sizes )
    {
        const auto  context  = MakeContext( size == 255u ? 255u : 7u );
        const auto  message  = Error( true, false, diagnostic.data(), size );
        const auto  wire     = Encode( context, message );
        std::size_t required = 99u;
        EXPECT_EQ(
            HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
            HIL_APPLICATION_STATUS_OK );
        EXPECT_EQ( required, size );
        std::vector<std::uint8_t> storage( size );
        Decode( context, wire, storage );
    }
}

TEST( ApplicationErrorMessage, RejectsInvalidFieldsAndTickForms )
{
    const auto context = MakeContext( 255u, 8u );
    auto       message = Error( false, true, nullptr, 0u );
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID );
    message                        = Error( true, false, nullptr, 0u );
    message.body.error.tick_number = 1u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TICK );
    message                        = Error( true, true, nullptr, 0u );
    message.body.error.tick_number = 8u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TICK );
    message = Error( true, false, nullptr, 1u );
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );

    auto wire = Encode( context, Error( true, false, nullptr, 0u ) );
    for ( const auto offset : { 1u, 2u } )
    {
        auto invalid                     = wire;
        invalid[kPayloadOffset + offset] = 2u;
        std::size_t required             = 99u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, invalid.data(),
                                                             invalid.size(), &required ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        EXPECT_EQ( required, 0u );
    }
    constexpr std::array<std::uint8_t, 3u> invalid_categories{ 0u, 0xfeu, 0xffu };
    for ( const auto invalid : invalid_categories )
    {
        auto category            = wire;
        category[kPayloadOffset] = invalid;
        std::size_t required     = 99u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, category.data(),
                                                             category.size(), &required ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        EXPECT_EQ( required, 0u );
    }
}

TEST( ApplicationErrorMessage, ScannerPrioritizesMalformedShapeBeforePolicyAndRejectsTrailingData )
{
    constexpr std::array<std::uint8_t, 4u> diagnostic{ 1u, 2u, 3u, 4u };
    constexpr std::uint8_t                 diagnostic_size = 4u;
    const auto                             permissive      = MakeContext();
    const auto                             restrictive     = MakeContext( 2u );
    const auto                             complete =
        Encode( permissive, Error( true, false, diagnostic.data(), diagnostic_size ) );

    auto truncated = complete;
    truncated.pop_back();
    --truncated[21];
    ExpectEncodedFailureClearsOutputs( restrictive, truncated,
                                       HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );

    auto trailing = complete;
    trailing.push_back( 0xa5u );
    ++trailing[21];
    ExpectEncodedFailureClearsOutputs( permissive, trailing,
                                       HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );

    std::size_t required = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &restrictive, complete.data(), complete.size(),
                                                    &required ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( required, 0u );
}

TEST( ApplicationErrorMessage, InsufficientCallerStorageClearsDecodeOutputs )
{
    constexpr std::array<std::uint8_t, 4u> diagnostic{ 1u, 2u, 3u, 4u };
    constexpr std::uint8_t                 diagnostic_size = 4u;
    const auto                             context         = MakeContext();
    const auto wire = Encode( context, Error( true, true, diagnostic.data(), diagnostic_size ) );
    std::array<std::uint8_t, 3u> storage{};
    HIL_Application_Message_T    decoded{};
    decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    std::size_t used = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               storage.data(), storage.size(), &used ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );
}
