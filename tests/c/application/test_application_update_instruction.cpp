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
constexpr std::size_t kUpdateHeaderSize    = 8u;

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
        id.bytes[i] = static_cast<std::uint8_t>( 0x30u + i );
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
WrapUpdateInstruction( const HIL_Application_Update_Instruction_T& update_instruction )
{
    HIL_Application_Message_T message{};
    message.type                    = HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION;
    message.subtype                 = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id             = 1u;
    message.test_id                 = TestId();
    message.body.update_instruction = update_instruction;
    return message;
}

std::vector<std::uint8_t>
EncodeUpdateInstruction( const HIL_Application_Context_T&            context,
                         const HIL_Application_Update_Instruction_T& update_instruction )
{
    const auto message = WrapUpdateInstruction( update_instruction );
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

std::size_t CalculateExpectedStorage( std::size_t operation_count, std::size_t total_payload_bytes )
{
    const std::size_t struct_bytes =
        operation_count * sizeof( HIL_Application_Logical_Operation_T );
    const std::size_t align_mask           = HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT - 1u;
    const std::size_t aligned_struct_bytes = ( struct_bytes + align_mask ) & ~align_mask;
    return aligned_struct_bytes + total_payload_bytes;
}

void ExpectUpdateInstructionEqual( const HIL_Application_Update_Instruction_T& expected,
                                   const HIL_Application_Update_Instruction_T& actual )
{
    EXPECT_EQ( actual.tick_number, expected.tick_number );
    EXPECT_EQ( actual.flags, expected.flags );
    ASSERT_EQ( actual.operation_count, expected.operation_count );
    if ( expected.operation_count > 0u )
    {
        ASSERT_NE( actual.operations, nullptr );
        ASSERT_NE( expected.operations, nullptr );
        for ( std::size_t i = 0u; i < expected.operation_count; ++i )
        {
            EXPECT_EQ( actual.operations[i].peripheral_type,
                       expected.operations[i].peripheral_type );
            EXPECT_EQ( actual.operations[i].channel, expected.operations[i].channel );
            ASSERT_EQ( actual.operations[i].payload.size, expected.operations[i].payload.size );
            for ( std::size_t j = 0u; j < expected.operations[i].payload.size; ++j )
            {
                EXPECT_EQ( actual.operations[i].payload.data[j],
                           expected.operations[i].payload.data[j] );
            }
        }
    }
}

void ExpectRoundTrip( const HIL_Application_Context_T&            context,
                      const HIL_Application_Update_Instruction_T& inst )
{
    const auto                first = EncodeUpdateInstruction( context, inst );
    AlignedDecodeStorage      storage{};
    HIL_Application_Message_T decoded{};
    std::size_t               required = 99u;
    ASSERT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, first.data(), first.size(), &required ),
        HIL_APPLICATION_STATUS_OK );
    std::size_t used = 99u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, first.data(), first.size(), &decoded,
                                               required == 0u ? nullptr : storage.bytes.data(),
                                               storage.bytes.size(), &used ),
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
    ASSERT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION );
    ExpectUpdateInstructionEqual( inst, decoded.body.update_instruction );
    const auto second = EncodeUpdateInstruction( context, decoded.body.update_instruction );
    EXPECT_EQ( second, first );
}

void ExpectValidationFailure( const HIL_Application_Context_T&            context,
                              const HIL_Application_Update_Instruction_T& inst )
{
    const auto message = WrapUpdateInstruction( inst );
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

TEST( ApplicationUpdateInstructionGolden, SingleOperationDigitalMatchesLiteralWireLayout )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u>  digital_payload = { 0x05u, 0x00u }; /* bits 0 and 2 set */
    HIL_Application_Logical_Operation_T op{};
    op.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    op.channel         = 0u;
    op.payload.data    = digital_payload.data();
    op.payload.size    = static_cast<std::uint8_t>( digital_payload.size() );

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 42u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 1u;
    inst.operations      = &op;

    const auto encoded = EncodeUpdateInstruction( context, inst );

    /* Sizing check: envelope 23 + update header 8 + record (4 header + 2 payload + 2 pad = 8) = 39
     * bytes */
    ASSERT_EQ( encoded.size(), kHeaderSize + kUpdateHeaderSize + 8u );

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
               static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION ) );
    EXPECT_EQ( encoded[20], static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_SUBTYPE_NONE ) );
    EXPECT_EQ( encoded[21], 16u ); /* 8 header + 8 record */
    EXPECT_EQ( encoded[22], 0u );

    /* Payload header fields */
    const std::size_t p = kPayloadOffset;
    EXPECT_EQ( encoded[p + 0u], 42u ); /* tick_number LE u32 */
    EXPECT_EQ( encoded[p + 1u], 0u );
    EXPECT_EQ( encoded[p + 2u], 0u );
    EXPECT_EQ( encoded[p + 3u], 0u );
    EXPECT_EQ( encoded[p + 4u], 1u ); /* operation_count */
    EXPECT_EQ( encoded[p + 5u], 0u ); /* flags */
    EXPECT_EQ( encoded[p + 6u], 0u ); /* reserved u16 */
    EXPECT_EQ( encoded[p + 7u], 0u );

    /* TLV record fields */
    const std::size_t r = p + kUpdateHeaderSize;
    EXPECT_EQ( encoded[r + 0u],
               static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT ) );
    EXPECT_EQ( encoded[r + 1u], 0u ); /* channel */
    EXPECT_EQ( encoded[r + 2u], 2u ); /* payload_length LE u16 */
    EXPECT_EQ( encoded[r + 3u], 0u );
    EXPECT_EQ( encoded[r + 4u], 0x05u ); /* payload data */
    EXPECT_EQ( encoded[r + 5u], 0x00u );
    EXPECT_EQ( encoded[r + 6u], 0x00u ); /* padding */
    EXPECT_EQ( encoded[r + 7u], 0x00u );

    ExpectRoundTrip( context, inst );
}

TEST( ApplicationUpdateInstructionGolden, RepresentativeMultiPeripheralMatchesLiteralWireLayout )
{
    const auto context = MakeContext();

    /* 1. DIGITAL_OUTPUT (ch 0): 2 bytes (0x03ff = 10 pins high), 2 pad bytes */
    const std::array<std::uint8_t, 2u> digital = { 0xffu, 0x03u };

    /* 2. ANALOG_OUTPUT (ch 2): 4 bytes microvolts (3300000 = 0x00325aa0), 0 pad */
    const std::array<std::uint8_t, 4u> analog = { 0xa0u, 0x5au, 0x32u, 0x00u };

    /* 3. PWM_OUTPUT (ch 1): 6 bytes (period 1000000 ns = 0x000f4240, duty 5000 = 0x1388), 2 pad */
    const std::array<std::uint8_t, 6u> pwm = { 0x40u, 0x42u, 0x0fu, 0x00u, 0x88u, 0x13u };

    /* 4. UART (ch 0): 5 bytes "HELLO", 3 pad bytes */
    const std::array<std::uint8_t, 5u> uart = { 'H', 'E', 'L', 'L', 'O' };

    /* 5. SPI (ch 1): 8 bytes packetised: 2 packets of sizes 2 and 3, 0 pad */
    const std::array<std::uint8_t, 8u> spi = { 2u, 2u, 3u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u };

    /* 6. CAN (ch 0): 12 bytes CAN frame: id 0x123, dlc 4, 8 data bytes, reserved 0, 0 pad */
    const std::array<std::uint8_t, 12u> can = { 0x23u, 0x01u, 4u,    0xdeu, 0xadu, 0xbeu,
                                                0xefu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };

    std::array<HIL_Application_Logical_Operation_T, 6u> ops{};
    ops[0].peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    ops[0].channel         = 0u;
    ops[0].payload         = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    ops[1].peripheral_type = HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT;
    ops[1].channel         = 2u;
    ops[1].payload         = { analog.data(), static_cast<std::uint8_t>( analog.size() ) };

    ops[2].peripheral_type = HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT;
    ops[2].channel         = 1u;
    ops[2].payload         = { pwm.data(), static_cast<std::uint8_t>( pwm.size() ) };

    ops[3].peripheral_type = HIL_APPLICATION_PERIPHERAL_UART;
    ops[3].channel         = 0u;
    ops[3].payload         = { uart.data(), static_cast<std::uint8_t>( uart.size() ) };

    ops[4].peripheral_type = HIL_APPLICATION_PERIPHERAL_SPI;
    ops[4].channel         = 1u;
    ops[4].payload         = { spi.data(), static_cast<std::uint8_t>( spi.size() ) };

    ops[5].peripheral_type = HIL_APPLICATION_PERIPHERAL_CAN;
    ops[5].channel         = 0u;
    ops[5].payload         = { can.data(), static_cast<std::uint8_t>( can.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 0x00020304u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_HAS_MORE_CHUNKS;
    inst.operation_count = static_cast<std::uint8_t>( ops.size() );
    inst.operations      = ops.data();

    const auto encoded = EncodeUpdateInstruction( context, inst );

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
    golden[19] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_TYPE_UPDATE_INSTRUCTION );
    golden[20] = static_cast<std::uint8_t>( HIL_APPLICATION_MESSAGE_SUBTYPE_NONE );
    PutU16Le( golden, kPayloadLengthOffset, static_cast<std::uint16_t>( kExpectedPayloadSize ) );

    /* Payload Header */
    const std::size_t p = kPayloadOffset;
    PutU32Le( golden, p + 0u, 0x00020304u );
    golden[p + 4u] = 6u; /* operation_count */
    golden[p + 5u] = HIL_APPLICATION_INSTRUCTION_FLAG_HAS_MORE_CHUNKS;
    golden[p + 6u] = 0u;
    golden[p + 7u] = 0u;

    /* Op 0 (Digital) */
    std::size_t cur  = p + 8u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT );
    golden[cur + 1u] = 0u;
    PutU16Le( golden, cur + 2u, 2u );
    golden[cur + 4u] = digital[0];
    golden[cur + 5u] = digital[1];
    golden[cur + 6u] = 0u; /* pad */
    golden[cur + 7u] = 0u; /* pad */

    /* Op 1 (Analog) */
    cur += 8u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT );
    golden[cur + 1u] = 2u;
    PutU16Le( golden, cur + 2u, 4u );
    std::copy( analog.begin(), analog.end(),
               golden.begin() + static_cast<std::ptrdiff_t>( cur + 4u ) );

    /* Op 2 (PWM) */
    cur += 8u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT );
    golden[cur + 1u] = 1u;
    PutU16Le( golden, cur + 2u, 6u );
    std::copy( pwm.begin(), pwm.end(), golden.begin() + static_cast<std::ptrdiff_t>( cur + 4u ) );
    golden[cur + 10u] = 0u; /* pad */
    golden[cur + 11u] = 0u; /* pad */

    /* Op 3 (UART) */
    cur += 12u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_UART );
    golden[cur + 1u] = 0u;
    PutU16Le( golden, cur + 2u, 5u );
    std::copy( uart.begin(), uart.end(), golden.begin() + static_cast<std::ptrdiff_t>( cur + 4u ) );
    golden[cur + 9u]  = 0u; /* pad */
    golden[cur + 10u] = 0u; /* pad */
    golden[cur + 11u] = 0u; /* pad */

    /* Op 4 (SPI) */
    cur += 12u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_SPI );
    golden[cur + 1u] = 1u;
    PutU16Le( golden, cur + 2u, 8u );
    std::copy( spi.begin(), spi.end(), golden.begin() + static_cast<std::ptrdiff_t>( cur + 4u ) );

    /* Op 5 (CAN) */
    cur += 12u;
    golden[cur + 0u] = static_cast<std::uint8_t>( HIL_APPLICATION_PERIPHERAL_CAN );
    golden[cur + 1u] = 0u;
    PutU16Le( golden, cur + 2u, 12u );
    std::copy( can.begin(), can.end(), golden.begin() + static_cast<std::ptrdiff_t>( cur + 4u ) );

    EXPECT_EQ( encoded, golden );

    ExpectRoundTrip( context, inst );
}

TEST( ApplicationUpdateInstructionCodec, DecodeStorageExactCalculationMatchesScan )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u> digital = { 0x01u, 0x00u };
    const std::array<std::uint8_t, 4u> analog  = { 0x00u, 0x00u, 0x10u, 0x00u };

    std::array<HIL_Application_Logical_Operation_T, 2u> ops{};
    ops[0].peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    ops[0].channel         = 0u;
    ops[0].payload         = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    ops[1].peripheral_type = HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT;
    ops[1].channel         = 1u;
    ops[1].payload         = { analog.data(), static_cast<std::uint8_t>( analog.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 10u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 2u;
    inst.operations      = ops.data();

    const auto wire = EncodeUpdateInstruction( context, inst );

    std::size_t required = 0u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &required ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( required, CalculateExpectedStorage( 2u, 2u + 4u ) );
}

TEST( ApplicationUpdateInstructionValidation, TickNumberWithinBounds )
{
    const auto context = MakeContext( HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE, 100u );

    const std::array<std::uint8_t, 2u>  digital = { 0x01u, 0x00u };
    HIL_Application_Logical_Operation_T op{};
    op.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    op.channel         = 0u;
    op.payload         = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 1u;
    inst.operations      = &op;

    /* Valid tick numbers */
    for ( const auto tick : { 0u, 99u } )
    {
        inst.tick_number   = tick;
        const auto message = WrapUpdateInstruction( inst );
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
                   HIL_APPLICATION_STATUS_OK );
    }

    /* Out of bounds tick number */
    inst.tick_number = 100u;
    ExpectValidationFailure( context, inst );
}

TEST( ApplicationUpdateInstructionValidation, StreamingFlagsWithinBounds )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u>  digital = { 0x01u, 0x00u };
    HIL_Application_Logical_Operation_T op{};
    op.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    op.channel         = 0u;
    op.payload         = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 0u;
    inst.operation_count = 1u;
    inst.operations      = &op;

    /* Valid flags */
    inst.flags   = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    auto message = WrapUpdateInstruction( inst );
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );

    inst.flags = HIL_APPLICATION_INSTRUCTION_FLAG_HAS_MORE_CHUNKS;
    message    = WrapUpdateInstruction( inst );
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );

    /* Invalid flags */
    inst.flags = 0x02u;
    ExpectValidationFailure( context, inst );

    inst.flags = 0xffu;
    ExpectValidationFailure( context, inst );
}

TEST( ApplicationUpdateInstructionValidation, ZeroOperationCountRejected )
{
    const auto context = MakeContext();

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 0u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 0u;
    inst.operations      = nullptr;

    ExpectValidationFailure( context, inst );
}

TEST( ApplicationUpdateInstructionValidation, ConfiguredVariableDataSizeBoundsUartPayload )
{
    const auto context = MakeContext( HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE,
                                      HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT, 3u );

    const std::array<std::uint8_t, 3u>  accepted_payload = { 'a', 'b', 'c' };
    HIL_Application_Logical_Operation_T accepted_operation{};
    accepted_operation.peripheral_type = HIL_APPLICATION_PERIPHERAL_UART;
    accepted_operation.channel         = 0u;
    accepted_operation.payload         = { accepted_payload.data(),
                                           static_cast<std::uint8_t>( accepted_payload.size() ) };

    HIL_Application_Update_Instruction_T accepted{};
    accepted.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    accepted.operation_count = 1u;
    accepted.operations      = &accepted_operation;

    const auto encoded = EncodeUpdateInstruction( context, accepted );
    EXPECT_FALSE( encoded.empty() );
    ExpectRoundTrip( context, accepted );

    const std::array<std::uint8_t, 4u>  rejected_payload = { 'a', 'b', 'c', 'd' };
    HIL_Application_Logical_Operation_T rejected_operation{};
    rejected_operation.peripheral_type = HIL_APPLICATION_PERIPHERAL_UART;
    rejected_operation.channel         = 0u;
    rejected_operation.payload         = { rejected_payload.data(),
                                           static_cast<std::uint8_t>( rejected_payload.size() ) };

    HIL_Application_Update_Instruction_T rejected{};
    rejected.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    rejected.operation_count = 1u;
    rejected.operations      = &rejected_operation;
    ExpectValidationFailure( context, rejected );
}

TEST( ApplicationUpdateInstructionValidation, DuplicatePeripheralChannelPairRejected )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u> digital1 = { 0x01u, 0x00u };
    const std::array<std::uint8_t, 2u> digital2 = { 0x02u, 0x00u };

    std::array<HIL_Application_Logical_Operation_T, 2u> ops{};
    ops[0].peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    ops[0].channel         = 0u;
    ops[0].payload         = { digital1.data(), static_cast<std::uint8_t>( digital1.size() ) };

    ops[1].peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    ops[1].channel         = 0u; /* duplicate */
    ops[1].payload         = { digital2.data(), static_cast<std::uint8_t>( digital2.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 1u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 2u;
    inst.operations      = ops.data();

    ExpectValidationFailure( context, inst );
}

TEST( ApplicationUpdateInstructionValidation, DigitalMaskReservedBitsEnforced )
{
    const auto context = MakeContext();

    /* Reserved bit 10 set (0x0400u) */
    const std::array<std::uint8_t, 2u>  bad_digital = { 0x00u, 0x04u };
    HIL_Application_Logical_Operation_T op{};
    op.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    op.channel         = 0u;
    op.payload         = { bad_digital.data(), static_cast<std::uint8_t>( bad_digital.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 1u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 1u;
    inst.operations      = &op;

    ExpectValidationFailure( context, inst );
}

TEST( ApplicationUpdateInstructionValidation, PwmParametersValidated )
{
    const auto context = MakeContext();

    /* 1. Duty > 10000 */
    {
        const std::array<std::uint8_t, 6u>  bad_duty = { 0x40u, 0x42u, 0x0fu, 0x00u, 0x11u, 0x27u };
        HIL_Application_Logical_Operation_T op{};
        op.peripheral_type = HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT;
        op.channel         = 0u;
        op.payload         = { bad_duty.data(), static_cast<std::uint8_t>( bad_duty.size() ) };

        HIL_Application_Update_Instruction_T inst{};
        inst.tick_number     = 1u;
        inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
        inst.operation_count = 1u;
        inst.operations      = &op;

        ExpectValidationFailure( context, inst );
    }

    /* 2. Zero period with non-zero duty */
    {
        const std::array<std::uint8_t, 6u>  zero_period = { 0x00u, 0x00u, 0x00u,
                                                            0x00u, 0x88u, 0x13u };
        HIL_Application_Logical_Operation_T op{};
        op.peripheral_type = HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT;
        op.channel         = 0u;
        op.payload = { zero_period.data(), static_cast<std::uint8_t>( zero_period.size() ) };

        HIL_Application_Update_Instruction_T inst{};
        inst.tick_number     = 1u;
        inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
        inst.operation_count = 1u;
        inst.operations      = &op;

        ExpectValidationFailure( context, inst );
    }
}

TEST( ApplicationUpdateInstructionValidation, CanFrameStructureValidated )
{
    const auto context = MakeContext();

    /* 1. CAN ID > 0x7FF */
    {
        const std::array<std::uint8_t, 12u> bad_id = { 0x00u, 0x08u, 0u, 0u, 0u, 0u,
                                                       0u,    0u,    0u, 0u, 0u, 0u };
        HIL_Application_Logical_Operation_T op{};
        op.peripheral_type = HIL_APPLICATION_PERIPHERAL_CAN;
        op.channel         = 0u;
        op.payload         = { bad_id.data(), static_cast<std::uint8_t>( bad_id.size() ) };

        HIL_Application_Update_Instruction_T inst{};
        inst.tick_number     = 1u;
        inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
        inst.operation_count = 1u;
        inst.operations      = &op;

        ExpectValidationFailure( context, inst );
    }

    /* 2. CAN DLC > 8 */
    {
        const std::array<std::uint8_t, 12u> bad_dlc = { 0x00u, 0x01u, 9u, 0u, 0u, 0u,
                                                        0u,    0u,    0u, 0u, 0u, 0u };
        HIL_Application_Logical_Operation_T op{};
        op.peripheral_type = HIL_APPLICATION_PERIPHERAL_CAN;
        op.channel         = 0u;
        op.payload         = { bad_dlc.data(), static_cast<std::uint8_t>( bad_dlc.size() ) };

        HIL_Application_Update_Instruction_T inst{};
        inst.tick_number     = 1u;
        inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
        inst.operation_count = 1u;
        inst.operations      = &op;

        ExpectValidationFailure( context, inst );
    }

    /* 3. Length not multiple of 12 */
    {
        const std::array<std::uint8_t, 11u> bad_len{};
        HIL_Application_Logical_Operation_T op{};
        op.peripheral_type = HIL_APPLICATION_PERIPHERAL_CAN;
        op.channel         = 0u;
        op.payload         = { bad_len.data(), static_cast<std::uint8_t>( bad_len.size() ) };

        HIL_Application_Update_Instruction_T inst{};
        inst.tick_number     = 1u;
        inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
        inst.operation_count = 1u;
        inst.operations      = &op;

        ExpectValidationFailure( context, inst );
    }
}

TEST( ApplicationUpdateInstructionValidation, SpiPacketStructureValidated )
{
    const auto context = MakeContext();

    /* 1. Packet count zero */
    {
        const std::array<std::uint8_t, 3u>  bad_spi = { 0u, 1u, 0xaau };
        HIL_Application_Logical_Operation_T op{};
        op.peripheral_type = HIL_APPLICATION_PERIPHERAL_SPI;
        op.channel         = 0u;
        op.payload         = { bad_spi.data(), static_cast<std::uint8_t>( bad_spi.size() ) };

        HIL_Application_Update_Instruction_T inst{};
        inst.tick_number     = 1u;
        inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
        inst.operation_count = 1u;
        inst.operations      = &op;

        ExpectValidationFailure( context, inst );
    }

    /* 2. Sum of packet lengths mismatch */
    {
        const std::array<std::uint8_t, 4u>  bad_spi = { 1u, 5u, 0x11u,
                                                        0x22u }; /* advertised 5, only 2 data */
        HIL_Application_Logical_Operation_T op{};
        op.peripheral_type = HIL_APPLICATION_PERIPHERAL_SPI;
        op.channel         = 0u;
        op.payload         = { bad_spi.data(), static_cast<std::uint8_t>( bad_spi.size() ) };

        HIL_Application_Update_Instruction_T inst{};
        inst.tick_number     = 1u;
        inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
        inst.operation_count = 1u;
        inst.operations      = &op;

        ExpectValidationFailure( context, inst );
    }
}

TEST( ApplicationUpdateInstructionValidation, InputPeripheralsRejectedInInstruction )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u>  data = { 0x01u, 0x00u };
    HIL_Application_Logical_Operation_T op{};
    op.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT; /* input not permitted */
    op.channel         = 0u;
    op.payload         = { data.data(), static_cast<std::uint8_t>( data.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 1u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 1u;
    inst.operations      = &op;

    ExpectValidationFailure( context, inst );
}

TEST( ApplicationUpdateInstructionValidation, EnvelopeRequiresTestIdAndSubtypeNone )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u>  digital = { 0x01u, 0x00u };
    HIL_Application_Logical_Operation_T op{};
    op.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    op.channel         = 0u;
    op.payload         = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 1u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 1u;
    inst.operations      = &op;

    /* Missing test_id */
    auto message        = WrapUpdateInstruction( inst );
    message.has_test_id = 0u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID );

    /* Subtype not NONE */
    message         = WrapUpdateInstruction( inst );
    message.subtype = HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
               HIL_APPLICATION_STATUS_INVALID_SUBTYPE );
}

TEST( ApplicationUpdateInstructionDecode, NonZeroReservedBytesRejected )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u>  digital = { 0x01u, 0x00u };
    HIL_Application_Logical_Operation_T op{};
    op.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    op.channel         = 0u;
    op.payload         = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 1u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 1u;
    inst.operations      = &op;

    auto wire = EncodeUpdateInstruction( context, inst );

    /* Reserved field is at offset 23 + 6 */
    wire[kPayloadOffset + 6u] = 0x01u;

    ExpectDecodeFailure( context, wire, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );

    std::size_t req = 0u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, wire.data(), wire.size(), &req ),
               HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
}

TEST( ApplicationUpdateInstructionDecode, NonZeroPaddingBytesRejected )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u>  digital = { 0x01u, 0x00u };
    HIL_Application_Logical_Operation_T op{};
    op.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    op.channel         = 0u;
    op.payload         = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 1u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 1u;
    inst.operations      = &op;

    auto wire = EncodeUpdateInstruction( context, inst );

    /* Op 0 has 2 pad bytes at offsets 23 + 8 + 6 and 23 + 8 + 7 */
    wire[kPayloadOffset + kUpdateHeaderSize + 6u] = 0xffu;

    ExpectDecodeFailure( context, wire, HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
}

TEST( ApplicationUpdateInstructionDecode, StorageBufferTooSmallRejected )
{
    const auto context = MakeContext();

    const std::array<std::uint8_t, 2u>  digital = { 0x01u, 0x00u };
    HIL_Application_Logical_Operation_T op{};
    op.peripheral_type = HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT;
    op.channel         = 0u;
    op.payload         = { digital.data(), static_cast<std::uint8_t>( digital.size() ) };

    HIL_Application_Update_Instruction_T inst{};
    inst.tick_number     = 1u;
    inst.flags           = HIL_APPLICATION_INSTRUCTION_FLAG_COMPLETE_TICK;
    inst.operation_count = 1u;
    inst.operations      = &op;

    const auto wire = EncodeUpdateInstruction( context, inst );

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
