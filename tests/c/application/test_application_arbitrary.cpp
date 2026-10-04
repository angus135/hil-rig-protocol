#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"
#include "hil_rig_protocol/version.h"

namespace {

HIL_Application_Context_T Context( std::size_t maximum = HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};
    EXPECT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_encoded_message_size = maximum;
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    return context;
}

std::vector<std::uint8_t> Encode( const HIL_Application_Context_T& context,
                                  const HIL_Application_Message_T& message )
{
    std::size_t size = 0u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &size ),
               HIL_APPLICATION_STATUS_OK );
    std::vector<std::uint8_t> wire( size );
    std::size_t               used = 0u;
    EXPECT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &message, wire.data(), wire.size(), &used ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, size );
    return wire;
}

HIL_Application_Message_T Control( std::uint32_t id, std::uint32_t value )
{
    HIL_Application_Message_T message{};
    message.type                   = HIL_APPLICATION_MESSAGE_TYPE_ARBITRARY_CONTROL;
    message.body.arbitrary_control = { id, value };
    return message;
}

HIL_Application_Message_T Data( std::uint32_t id, const std::vector<std::uint8_t>& bytes )
{
    HIL_Application_Message_T message{};
    message.type                = HIL_APPLICATION_MESSAGE_TYPE_ARBITRARY_DATA;
    message.body.arbitrary_data = {
        id,
        { bytes.empty() ? nullptr : bytes.data(), static_cast<std::uint16_t>( bytes.size() ) } };
    return message;
}

TEST( ApplicationArbitrary, ControlHasExactWireFieldsAndOptionalTestId )
{
    const auto context = Context();
    auto       message = Control( 0x01020304u, 0xa1b2c3d4u );
    const auto wire    = Encode( context, message );
    ASSERT_EQ( wire.size(), 31u );
    EXPECT_EQ( wire[1], HIL_RIG_PROTOCOL_VERSION_MINOR );
    EXPECT_EQ( wire[2], 0u );
    EXPECT_EQ( wire[19], 64u );
    EXPECT_EQ( wire[21], 8u );
    EXPECT_EQ( wire[23], 4u );
    EXPECT_EQ( wire[24], 3u );
    EXPECT_EQ( wire[25], 2u );
    EXPECT_EQ( wire[26], 1u );
    EXPECT_EQ( wire[27], 0xd4u );
    EXPECT_EQ( wire[28], 0xc3u );
    EXPECT_EQ( wire[29], 0xb2u );
    EXPECT_EQ( wire[30], 0xa1u );
    HIL_Application_Message_T decoded{};
    std::size_t               used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, 0u );
    EXPECT_EQ( decoded.body.arbitrary_control.control_id, 0x01020304u );
    EXPECT_EQ( decoded.body.arbitrary_control.value, 0xa1b2c3d4u );

    message                  = Control( 0u, UINT32_MAX );
    message.has_test_id      = 1u;
    message.test_id.bytes[0] = 0xabu;
    const auto contextual    = Encode( context, message );
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, contextual.data(), contextual.size(),
                                               &decoded, nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.has_test_id, 1u );
    EXPECT_EQ( decoded.test_id.bytes[0], 0xabu );
    EXPECT_EQ( decoded.body.arbitrary_control.value, UINT32_MAX );
}

TEST( ApplicationArbitrary, DataLengthStorageAndOpaqueOwnership )
{
    const auto                context = Context();
    std::vector<std::uint8_t> bytes( 256u, 0xffu );
    bytes[0]                  = 0u;
    auto message              = Data( 1234u, bytes );
    message.has_test_id       = 1u;
    message.test_id.bytes[15] = 0x5au;
    auto wire                 = Encode( context, message );
    ASSERT_EQ( wire.size(), 285u );
    EXPECT_EQ( wire[19], 65u );
    EXPECT_EQ( wire[27], 0u );
    EXPECT_EQ( wire[28], 1u );
    std::size_t required = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required, bytes.size() );
    required = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(), &required ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required, bytes.size() );
    std::vector<std::uint8_t> storage( 256u );
    HIL_Application_Message_T decoded{};
    std::size_t               used = 99u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               storage.data(), storage.size() - 1u, &used ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, wire.data(), wire.size(), &decoded,
                                               storage.data(), storage.size(), &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, bytes.size() );
    EXPECT_EQ( decoded.body.arbitrary_data.data_id, 1234u );
    EXPECT_EQ( decoded.body.arbitrary_data.payload.data, storage.data() );
    EXPECT_EQ( storage, bytes );
    wire[29] = 0xadu;
    EXPECT_EQ( storage[0], 0u );

    const auto empty_wire = Encode( context, Data( UINT32_MAX, {} ) );
    EXPECT_EQ( empty_wire.size(), 29u );
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, empty_wire.data(), empty_wire.size(),
                                               &decoded, nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( decoded.body.arbitrary_data.payload.data, nullptr );
    EXPECT_EQ( decoded.body.arbitrary_data.payload.size, 0u );
}

TEST( ApplicationArbitrary, RejectsContradictoryInternalLengthAndCompleteMessageOverflow )
{
    const auto                      context = Context();
    const std::vector<std::uint8_t> bytes{ 0u, 1u, 0xffu };
    auto                            wire = Encode( context, Data( 0x01020304u, bytes ) );
    ASSERT_EQ( wire.size(), 32u );
    EXPECT_EQ( wire[23], 4u );
    EXPECT_EQ( wire[24], 3u );
    EXPECT_EQ( wire[25], 2u );
    EXPECT_EQ( wire[26], 1u );
    EXPECT_EQ( wire[27], 3u );
    wire[27]             = 4u;
    std::size_t required = 99u;
    EXPECT_EQ(
        HIL_APPLICATION_Validate_Encoded_Message( &context, wire.data(), wire.size(), &required ),
        HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( required, 0u );

    const auto maximum_context = Context( HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE );
    std::vector<std::uint8_t> maximum_bytes( 65506u, 0x5au );
    const auto                maximum_message = Data( 0u, maximum_bytes );
    required                                  = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &maximum_context, &maximum_message, &required ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required, 65535u );
    maximum_bytes.push_back( 0u );
    const auto too_large = Data( 0u, maximum_bytes );
    required             = 99u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &maximum_context, &too_large, &required ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( required, 0u );
}

}  // namespace
