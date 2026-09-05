#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "hil_rig_protocol/application/application.h"

namespace {
constexpr std::size_t kInstructionCompleteSize = 73u;
constexpr std::size_t kResultCompleteSize      = 62u;
constexpr std::size_t kPayloadLengthOffset     = 21u;
constexpr std::size_t kPayloadOffset           = 23u;

HIL_Application_Test_Id_T FixedIoTestId()
{
    HIL_Application_Test_Id_T test_id{};
    for ( std::size_t i = 0u; i < HIL_APPLICATION_TEST_ID_SIZE; ++i )
    {
        test_id.bytes[i] = static_cast<std::uint8_t>( 0x10u + i );
    }
    return test_id;
}

HIL_Application_Context_T
MakeFixedIoContext( std::size_t   max_message_size = HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE,
                    std::uint32_t max_tick_count   = HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT )
{
    HIL_Application_Config_T  config{};
    HIL_Application_Context_T context{};
    EXPECT_EQ( HIL_APPLICATION_Default_Config( &config ), HIL_APPLICATION_STATUS_OK );
    config.max_encoded_message_size = max_message_size;
    config.max_expected_tick_count  = max_tick_count;
    EXPECT_EQ( HIL_APPLICATION_Init( &context, &config ), HIL_APPLICATION_STATUS_OK );
    return context;
}

HIL_Application_Message_T MinimalInstruction()
{
    HIL_Application_Message_T message{};
    message.type        = HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION;
    message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id = 1u;
    message.test_id     = FixedIoTestId();
    return message;
}

HIL_Application_Message_T GoldenInstructionMessage()
{
    auto message                              = MinimalInstruction();
    message.body.test_instruction.tick_number = 0x000abcdeu;

    constexpr std::array<std::uint8_t, 10u> digital = { 0u, 1u, 1u, 0u, 1u, 0u, 0u, 1u, 1u, 0u };
    for ( std::size_t i = 0u; i < digital.size(); ++i )
    {
        message.body.test_instruction.digital_outputs[i].high = digital[i];
    }

    constexpr std::array<std::uint32_t, 6u> analog = {
        0x00000000u, 0x00000001u, 0x12345678u, 0x89abcdefu, 0x01020304u, 0xffffffffu,
    };
    for ( std::size_t i = 0u; i < analog.size(); ++i )
    {
        message.body.test_instruction.analog_outputs[i].microvolts = analog[i];
    }

    message.body.test_instruction.pwm_outputs[0].period_nanoseconds   = 0x11223344u;
    message.body.test_instruction.pwm_outputs[0].duty_cycle_permyriad = 10000u;
    message.body.test_instruction.pwm_outputs[1].period_nanoseconds   = 0u;
    message.body.test_instruction.pwm_outputs[1].duty_cycle_permyriad = 0u;
    return message;
}

HIL_Application_Message_T MinimalResult()
{
    HIL_Application_Message_T message{};
    message.type                       = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    message.subtype                    = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
    message.has_test_id                = 1u;
    message.test_id                    = FixedIoTestId();
    message.body.test_result.condition = HIL_APPLICATION_RESULT_CONDITION_OK;
    return message;
}

HIL_Application_Message_T GoldenResultMessage()
{
    auto message                         = MinimalResult();
    message.body.test_result.tick_number = 0x00054321u;

    constexpr std::array<std::uint8_t, 10u> digital = { 1u, 0u, 1u, 0u, 1u, 1u, 0u, 0u, 1u, 0u };
    for ( std::size_t i = 0u; i < digital.size(); ++i )
    {
        message.body.test_result.digital_inputs[i].high = digital[i];
    }

    message.body.test_result.analog_inputs[0].microvolts        = 0u;
    message.body.test_result.analog_inputs[1].microvolts        = 0xffffffffu;
    message.body.test_result.pwm_inputs[0].period_nanoseconds   = 0xa1b2c3d4u;
    message.body.test_result.pwm_inputs[0].duty_cycle_permyriad = 9999u;
    message.body.test_result.pwm_inputs[1].period_nanoseconds   = 0u;
    message.body.test_result.pwm_inputs[1].duty_cycle_permyriad = 0u;
    message.body.test_result.condition      = HIL_APPLICATION_RESULT_CONDITION_PARTIAL;
    message.body.test_result.problem_detail = 0x89abcdefu;
    return message;
}

std::array<std::uint8_t, 73u> InstructionGolden()
{
    /* Literal complete message. Payload offsets: tick 0, digital 4, analogue 14, PWM 38. */
    return {
        0x00u, 0x01u, 0x01u, 0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u, 0x19u,
        0x1au, 0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu, 0x11u, 0x00u, 0x32u, 0x00u, 0xdeu, 0xbcu, 0x0au,
        0x00u, 0x00u, 0x01u, 0x01u, 0x00u, 0x01u, 0x00u, 0x00u, 0x01u, 0x01u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x78u, 0x56u, 0x34u, 0x12u, 0xefu, 0xcdu, 0xabu,
        0x89u, 0x04u, 0x03u, 0x02u, 0x01u, 0xffu, 0xffu, 0xffu, 0xffu, 0x44u, 0x33u, 0x22u, 0x11u,
        0x10u, 0x27u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    };
}

std::array<std::uint8_t, 62u> ResultGolden()
{
    /* Literal complete message. Payload offsets: tick 0, digital 4, analogue 14, PWM 22,
     * condition 34, problem_detail 35. */
    return {
        0x00u, 0x01u, 0x01u, 0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u, 0x19u,
        0x1au, 0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu, 0x20u, 0x00u, 0x27u, 0x00u, 0x21u, 0x43u, 0x05u,
        0x00u, 0x01u, 0x00u, 0x01u, 0x00u, 0x01u, 0x01u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu, 0xd4u, 0xc3u, 0xb2u, 0xa1u, 0x0fu, 0x27u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0xefu, 0xcdu, 0xabu, 0x89u,
    };
}

void ExpectTestIdEqual( const HIL_Application_Test_Id_T& expected,
                        const HIL_Application_Test_Id_T& actual )
{
    EXPECT_TRUE( std::equal( std::begin( expected.bytes ), std::end( expected.bytes ),
                             std::begin( actual.bytes ) ) );
}

void ExpectInstructionEqual( const HIL_Application_Message_T& expected,
                             const HIL_Application_Message_T& actual )
{
    EXPECT_EQ( actual.type, HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION );
    EXPECT_EQ( actual.subtype, HIL_APPLICATION_MESSAGE_SUBTYPE_NONE );
    EXPECT_EQ( actual.has_test_id, 1u );
    ExpectTestIdEqual( expected.test_id, actual.test_id );
    EXPECT_EQ( actual.body.test_instruction.tick_number,
               expected.body.test_instruction.tick_number );
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.body.test_instruction.digital_outputs[i].high,
                   expected.body.test_instruction.digital_outputs[i].high );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.body.test_instruction.analog_outputs[i].microvolts,
                   expected.body.test_instruction.analog_outputs[i].microvolts );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.body.test_instruction.pwm_outputs[i].period_nanoseconds,
                   expected.body.test_instruction.pwm_outputs[i].period_nanoseconds );
        EXPECT_EQ( actual.body.test_instruction.pwm_outputs[i].duty_cycle_permyriad,
                   expected.body.test_instruction.pwm_outputs[i].duty_cycle_permyriad );
    }
}

void ExpectResultEqual( const HIL_Application_Message_T& expected,
                        const HIL_Application_Message_T& actual )
{
    EXPECT_EQ( actual.type, HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT );
    EXPECT_EQ( actual.subtype, HIL_APPLICATION_MESSAGE_SUBTYPE_NONE );
    EXPECT_EQ( actual.has_test_id, 1u );
    ExpectTestIdEqual( expected.test_id, actual.test_id );
    EXPECT_EQ( actual.body.test_result.tick_number, expected.body.test_result.tick_number );
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.body.test_result.digital_inputs[i].high,
                   expected.body.test_result.digital_inputs[i].high );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.body.test_result.analog_inputs[i].microvolts,
                   expected.body.test_result.analog_inputs[i].microvolts );
    }
    for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
    {
        EXPECT_EQ( actual.body.test_result.pwm_inputs[i].period_nanoseconds,
                   expected.body.test_result.pwm_inputs[i].period_nanoseconds );
        EXPECT_EQ( actual.body.test_result.pwm_inputs[i].duty_cycle_permyriad,
                   expected.body.test_result.pwm_inputs[i].duty_cycle_permyriad );
    }
    EXPECT_EQ( actual.body.test_result.condition, expected.body.test_result.condition );
    EXPECT_EQ( actual.body.test_result.problem_detail, expected.body.test_result.problem_detail );
}

void ExpectMalformedFixedBodyPublishesNothing( const HIL_Application_Context_T& context,
                                               const std::vector<std::uint8_t>& bytes )
{
    std::size_t storage = 999u;
    EXPECT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, bytes.data(), bytes.size(), &storage ),
        HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( storage, 0u );

    HIL_Application_Message_T decoded{};
    decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    std::size_t used = 999u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, bytes.data(), bytes.size(), &decoded,
                                               nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );

    storage = 999u;
    EXPECT_EQ(
        HIL_APPLICATION_Validate_Encoded_Message( &context, bytes.data(), bytes.size(), &storage ),
        HIL_APPLICATION_STATUS_MALFORMED_MESSAGE );
    EXPECT_EQ( storage, 0u );
}
}  // namespace

static_assert( kInstructionCompleteSize == 73u );
static_assert( kResultCompleteSize == 62u );

TEST( ApplicationFixedIoGolden, InstructionFullFacadeSequenceMatchesLiteralVector )
{
    const auto context  = MakeFixedIoContext();
    const auto message  = GoldenInstructionMessage();
    const auto expected = InstructionGolden();

    ASSERT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );

    std::size_t encoded_size = 999u;
    ASSERT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_size, 73u );

    std::array<std::uint8_t, 73u> encoded{};
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, 73u );
    EXPECT_EQ( encoded, expected );

    std::size_t storage = 999u;
    ASSERT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, expected.data(), expected.size(), &storage ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( storage, 0u );

    HIL_Application_Message_T decoded{};
    std::size_t               used = 999u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, expected.data(), expected.size(), &decoded,
                                               nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, 0u );
    ExpectInstructionEqual( message, decoded );

    storage = 999u;
    ASSERT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, expected.data(), expected.size(),
                                                         &storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( storage, 0u );

    std::array<std::uint8_t, 73u> reencoded{};
    encoded_size = 999u;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &decoded, reencoded.data(),
                                               reencoded.size(), &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, 73u );
    EXPECT_EQ( reencoded, expected );
}

TEST( ApplicationFixedIoGolden, ResultFullFacadeSequenceMatchesLiteralVector )
{
    const auto context  = MakeFixedIoContext();
    const auto message  = GoldenResultMessage();
    const auto expected = ResultGolden();

    ASSERT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ), HIL_APPLICATION_STATUS_OK );

    std::size_t encoded_size = 999u;
    ASSERT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    ASSERT_EQ( encoded_size, 62u );

    std::array<std::uint8_t, 62u> encoded{};
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, encoded.data(), encoded.size(),
                                               &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, 62u );
    EXPECT_EQ( encoded, expected );

    std::size_t storage = 999u;
    ASSERT_EQ(
        HIL_APPLICATION_Decode_Storage_Size( &context, expected.data(), expected.size(), &storage ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( storage, 0u );

    HIL_Application_Message_T decoded{};
    std::size_t               used = 999u;
    ASSERT_EQ( HIL_APPLICATION_Decode_Message( &context, expected.data(), expected.size(), &decoded,
                                               nullptr, 0u, &used ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( used, 0u );
    ExpectResultEqual( message, decoded );

    storage = 999u;
    ASSERT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, expected.data(), expected.size(),
                                                         &storage ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( storage, 0u );

    std::array<std::uint8_t, 62u> reencoded{};
    encoded_size = 999u;
    ASSERT_EQ( HIL_APPLICATION_Encode_Message( &context, &decoded, reencoded.data(),
                                               reencoded.size(), &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, 62u );
    EXPECT_EQ( reencoded, expected );
}

TEST( ApplicationFixedIoGolden, LiteralVectorsLockEnvelopeAndFixedFieldOffsets )
{
    const auto                              instruction         = InstructionGolden();
    constexpr std::array<std::uint8_t, 10u> instruction_digital = {
        0x00u, 0x01u, 0x01u, 0x00u, 0x01u, 0x00u, 0x00u, 0x01u, 0x01u, 0x00u,
    };
    constexpr std::array<std::uint8_t, 24u> instruction_analog = {
        0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x78u, 0x56u, 0x34u, 0x12u,
        0xefu, 0xcdu, 0xabu, 0x89u, 0x04u, 0x03u, 0x02u, 0x01u, 0xffu, 0xffu, 0xffu, 0xffu,
    };
    constexpr std::array<std::uint8_t, 12u> instruction_pwm = {
        0x44u, 0x33u, 0x22u, 0x11u, 0x10u, 0x27u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    };
    ASSERT_EQ( instruction.size(), 73u );
    EXPECT_EQ( instruction[0], 0x00u );
    EXPECT_EQ( instruction[1], 0x01u );
    EXPECT_EQ( instruction[2], 0x01u );
    for ( std::size_t i = 0u; i < 16u; ++i )
    {
        EXPECT_EQ( instruction[3u + i], static_cast<std::uint8_t>( 0x10u + i ) );
    }
    EXPECT_EQ( instruction[19], 0x11u );
    EXPECT_EQ( instruction[20], 0x00u );
    EXPECT_EQ( instruction[21], 0x32u );
    EXPECT_EQ( instruction[22], 0x00u );
    EXPECT_EQ( instruction[23], 0xdeu );
    EXPECT_EQ( instruction[24], 0xbcu );
    EXPECT_EQ( instruction[25], 0x0au );
    EXPECT_EQ( instruction[26], 0x00u );
    EXPECT_TRUE( std::equal( instruction_digital.begin(), instruction_digital.end(),
                             instruction.begin() + 27 ) );
    EXPECT_TRUE( std::equal( instruction_analog.begin(), instruction_analog.end(),
                             instruction.begin() + 37 ) );
    EXPECT_TRUE(
        std::equal( instruction_pwm.begin(), instruction_pwm.end(), instruction.begin() + 61 ) );

    const auto                              result         = ResultGolden();
    constexpr std::array<std::uint8_t, 10u> result_digital = {
        0x01u, 0x00u, 0x01u, 0x00u, 0x01u, 0x01u, 0x00u, 0x00u, 0x01u, 0x00u,
    };
    constexpr std::array<std::uint8_t, 8u> result_analog = {
        0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu,
    };
    constexpr std::array<std::uint8_t, 12u> result_pwm = {
        0xd4u, 0xc3u, 0xb2u, 0xa1u, 0x0fu, 0x27u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    };
    ASSERT_EQ( result.size(), 62u );
    EXPECT_EQ( result[0], 0x00u );
    EXPECT_EQ( result[1], 0x01u );
    EXPECT_EQ( result[2], 0x01u );
    for ( std::size_t i = 0u; i < 16u; ++i )
    {
        EXPECT_EQ( result[3u + i], static_cast<std::uint8_t>( 0x10u + i ) );
    }
    EXPECT_EQ( result[19], 0x20u );
    EXPECT_EQ( result[20], 0x00u );
    EXPECT_EQ( result[21], 0x27u );
    EXPECT_EQ( result[22], 0x00u );
    EXPECT_EQ( result[23], 0x21u );
    EXPECT_EQ( result[24], 0x43u );
    EXPECT_EQ( result[25], 0x05u );
    EXPECT_EQ( result[26], 0x00u );
    EXPECT_TRUE( std::equal( result_digital.begin(), result_digital.end(), result.begin() + 27 ) );
    EXPECT_TRUE( std::equal( result_analog.begin(), result_analog.end(), result.begin() + 37 ) );
    EXPECT_TRUE( std::equal( result_pwm.begin(), result_pwm.end(), result.begin() + 45 ) );
    EXPECT_EQ( result[57], 0x01u );
    EXPECT_EQ( result[58], 0xefu );
    EXPECT_EQ( result[59], 0xcdu );
    EXPECT_EQ( result[60], 0xabu );
    EXPECT_EQ( result[61], 0x89u );
}

TEST( ApplicationFixedIoValidation, TickUsesConfiguredStructuralCeilingForBothFamilies )
{
    const auto context = MakeFixedIoContext( HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE, 10u );

    auto instruction = MinimalInstruction();
    for ( const auto tick : { 0u, 9u } )
    {
        instruction.body.test_instruction.tick_number = tick;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &instruction ),
                   HIL_APPLICATION_STATUS_OK );
    }
    instruction.body.test_instruction.tick_number = 10u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &instruction ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    std::size_t encoded_size = 0u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &instruction, &encoded_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( encoded_size, 73u );

    auto result = MinimalResult();
    for ( const auto tick : { 0u, 9u } )
    {
        result.body.test_result.tick_number = tick;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &result ),
                   HIL_APPLICATION_STATUS_OK );
    }
    result.body.test_result.tick_number = 10u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &result ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
}

TEST( ApplicationFixedIoValidation, EveryDigitalChannelRequiresZeroOrOne )
{
    const auto context = MakeFixedIoContext();

    auto instruction = MinimalInstruction();
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT; ++i )
    {
        instruction.body.test_instruction.digital_outputs[i].high = 2u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &instruction ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        instruction.body.test_instruction.digital_outputs[i].high = 0u;
    }

    auto result = MinimalResult();
    for ( std::size_t i = 0u; i < HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT; ++i )
    {
        result.body.test_result.digital_inputs[i].high = 0xffu;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &result ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        result.body.test_result.digital_inputs[i].high = 0u;
    }
}

TEST( ApplicationFixedIoValidation, EveryPwmChannelEnforcesDutyAndZeroPeriodRules )
{
    const auto context = MakeFixedIoContext();

    for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT; ++i )
    {
        auto instruction                                                    = MinimalInstruction();
        instruction.body.test_instruction.pwm_outputs[i].period_nanoseconds = 1u;
        instruction.body.test_instruction.pwm_outputs[i].duty_cycle_permyriad = 10000u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &instruction ),
                   HIL_APPLICATION_STATUS_OK );
        instruction.body.test_instruction.pwm_outputs[i].duty_cycle_permyriad = 10001u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &instruction ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        instruction.body.test_instruction.pwm_outputs[i].period_nanoseconds   = 0u;
        instruction.body.test_instruction.pwm_outputs[i].duty_cycle_permyriad = 0u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &instruction ),
                   HIL_APPLICATION_STATUS_OK );
        instruction.body.test_instruction.pwm_outputs[i].duty_cycle_permyriad = 1u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &instruction ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    }

    for ( std::size_t i = 0u; i < HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT; ++i )
    {
        auto result                                                = MinimalResult();
        result.body.test_result.pwm_inputs[i].period_nanoseconds   = 1u;
        result.body.test_result.pwm_inputs[i].duty_cycle_permyriad = 10000u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &result ),
                   HIL_APPLICATION_STATUS_OK );
        result.body.test_result.pwm_inputs[i].duty_cycle_permyriad = 10001u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &result ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
        result.body.test_result.pwm_inputs[i].period_nanoseconds   = 0u;
        result.body.test_result.pwm_inputs[i].duty_cycle_permyriad = 0u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &result ),
                   HIL_APPLICATION_STATUS_OK );
        result.body.test_result.pwm_inputs[i].duty_cycle_permyriad = 1u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &result ),
                   HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    }
}

TEST( ApplicationFixedIoValidation, ResultConditionAcceptsOnlyThreeDefinedValues )
{
    const auto context = MakeFixedIoContext();
    auto       result  = MinimalResult();

    for ( unsigned value = 0u; value <= 255u; ++value )
    {
        result.body.test_result.condition =
            static_cast<HIL_Application_Result_Condition_T>( value );
        const bool valid =
            value == static_cast<unsigned>( HIL_APPLICATION_RESULT_CONDITION_OK )
            || value == static_cast<unsigned>( HIL_APPLICATION_RESULT_CONDITION_PARTIAL )
            || value == static_cast<unsigned>( HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM );
        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &result ),
                   valid ? HIL_APPLICATION_STATUS_OK : HIL_APPLICATION_STATUS_VALIDATION_FAILED )
            << "condition=" << value;
    }
}

TEST( ApplicationFixedIoValidation, AnalogueUint32BoundariesAndProblemDetailRemainUnconstrained )
{
    const auto context = MakeFixedIoContext();

    auto instruction = MinimalInstruction();
    for ( std::size_t i = 0u; i < HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT; ++i )
    {
        instruction.body.test_instruction.analog_outputs[i].microvolts =
            ( i % 2u == 0u ) ? 0u : std::numeric_limits<std::uint32_t>::max();
    }
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &instruction ),
               HIL_APPLICATION_STATUS_OK );

    auto result                                         = MinimalResult();
    result.body.test_result.analog_inputs[0].microvolts = 0u;
    result.body.test_result.analog_inputs[1].microvolts = std::numeric_limits<std::uint32_t>::max();
    result.body.test_result.problem_detail              = std::numeric_limits<std::uint32_t>::max();
    EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &result ), HIL_APPLICATION_STATUS_OK );
}

TEST( ApplicationFixedIoFacade, ExactAndOneByteShortOutputCapacitiesAreDistinguished )
{
    const auto context = MakeFixedIoContext();

    const auto                    instruction = GoldenInstructionMessage();
    std::array<std::uint8_t, 73u> instruction_exact{};
    std::size_t                   output_size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &instruction, instruction_exact.data(),
                                               instruction_exact.size(), &output_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( output_size, 73u );
    std::array<std::uint8_t, 72u> instruction_short{};
    output_size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &instruction, instruction_short.data(),
                                               instruction_short.size(), &output_size ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, 0u );

    const auto                    result = GoldenResultMessage();
    std::array<std::uint8_t, 62u> result_exact{};
    output_size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &result, result_exact.data(),
                                               result_exact.size(), &output_size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( output_size, 62u );
    std::array<std::uint8_t, 61u> result_short{};
    output_size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &result, result_short.data(),
                                               result_short.size(), &output_size ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, 0u );
}

TEST( ApplicationFixedIoFacade, ConfiguredCompleteMessageLimitUsesExactFixedSizes )
{
    const auto                    instruction = GoldenInstructionMessage();
    const auto                    result      = GoldenResultMessage();
    std::array<std::uint8_t, 80u> buffer{};
    std::size_t                   size = 999u;

    auto context = MakeFixedIoContext( 73u );
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &instruction, &size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( size, 73u );
    size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &instruction, buffer.data(), buffer.size(),
                                               &size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( size, 73u );

    context = MakeFixedIoContext( 72u );
    size    = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &instruction, &size ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( size, 0u );
    size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &instruction, buffer.data(), buffer.size(),
                                               &size ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( size, 0u );

    context = MakeFixedIoContext( 62u );
    size    = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &result, &size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( size, 62u );
    size = 999u;
    EXPECT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &result, buffer.data(), buffer.size(), &size ),
        HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( size, 62u );

    context = MakeFixedIoContext( 61u );
    size    = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &result, &size ),
               HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( size, 0u );
    size = 999u;
    EXPECT_EQ(
        HIL_APPLICATION_Encode_Message( &context, &result, buffer.data(), buffer.size(), &size ),
        HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( size, 0u );
}

TEST( ApplicationFixedIoDecode, UndersizedAndOversizedInstructionBodiesAreMalformed )
{
    const auto context = MakeFixedIoContext();
    const auto golden  = InstructionGolden();

    std::vector<std::uint8_t> undersized( golden.begin(), golden.end() - 1 );
    undersized[kPayloadLengthOffset] = 49u;
    ExpectMalformedFixedBodyPublishesNothing( context, undersized );

    std::vector<std::uint8_t> oversized( golden.begin(), golden.end() );
    oversized.push_back( 0u );
    oversized[kPayloadLengthOffset] = 51u;
    ExpectMalformedFixedBodyPublishesNothing( context, oversized );
}

TEST( ApplicationFixedIoDecode, UndersizedAndOversizedResultBodiesAreMalformed )
{
    const auto context = MakeFixedIoContext();
    const auto golden  = ResultGolden();

    std::vector<std::uint8_t> undersized( golden.begin(), golden.end() - 1 );
    undersized[kPayloadLengthOffset] = 38u;
    ExpectMalformedFixedBodyPublishesNothing( context, undersized );

    std::vector<std::uint8_t> oversized( golden.begin(), golden.end() );
    oversized.push_back( 0u );
    oversized[kPayloadLengthOffset] = 40u;
    ExpectMalformedFixedBodyPublishesNothing( context, oversized );
}

TEST( ApplicationFixedIoFacade, SizingAndStorageDoNotPerformFixedValueValidation )
{
    const auto context = MakeFixedIoContext();

    auto instruction                                          = MinimalInstruction();
    instruction.body.test_instruction.digital_outputs[0].high = 2u;
    std::size_t size                                          = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &instruction, &size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( size, 73u );
    std::array<std::uint8_t, 73u> output{};
    size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &instruction, output.data(), output.size(),
                                               &size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( size, 0u );

    auto instruction_bytes                 = InstructionGolden();
    instruction_bytes[kPayloadOffset + 4u] = 2u;
    size                                   = 999u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, instruction_bytes.data(),
                                                    instruction_bytes.size(), &size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( size, 0u );
    HIL_Application_Message_T decoded{};
    decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
    std::size_t used = 999u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, instruction_bytes.data(),
                                               instruction_bytes.size(), &decoded, nullptr, 0u,
                                               &used ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
    EXPECT_EQ( used, 0u );
    size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, instruction_bytes.data(),
                                                         instruction_bytes.size(), &size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( size, 0u );

    auto result                       = MinimalResult();
    result.body.test_result.condition = HIL_APPLICATION_RESULT_CONDITION_RESERVED;
    size                              = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &result, &size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( size, 62u );
    std::array<std::uint8_t, 62u> result_output{};
    size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &result, result_output.data(),
                                               result_output.size(), &size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( size, 0u );

    auto result_bytes                  = ResultGolden();
    result_bytes[kPayloadOffset + 34u] = 3u;
    size                               = 999u;
    EXPECT_EQ( HIL_APPLICATION_Decode_Storage_Size( &context, result_bytes.data(),
                                                    result_bytes.size(), &size ),
               HIL_APPLICATION_STATUS_OK );
    EXPECT_EQ( size, 0u );
    size = 999u;
    EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, result_bytes.data(),
                                                         result_bytes.size(), &size ),
               HIL_APPLICATION_STATUS_VALIDATION_FAILED );
    EXPECT_EQ( size, 0u );
}

TEST( ApplicationFixedIoDeferred, VariableInstructionAndResultFamiliesRemainNotImplemented )
{
    const auto                    context = MakeFixedIoContext();
    std::array<std::uint8_t, 80u> buffer{};

    for ( const auto type : { HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA,
                              HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA } )
    {
        HIL_Application_Message_T message{};
        message.type        = type;
        message.subtype     = HIL_APPLICATION_MESSAGE_SUBTYPE_NONE;
        message.has_test_id = 1u;
        message.test_id     = FixedIoTestId();

        EXPECT_EQ( HIL_APPLICATION_Validate_Message( &context, &message ),
                   HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
        std::size_t size = 999u;
        EXPECT_EQ( HIL_APPLICATION_Encoded_Size( &context, &message, &size ),
                   HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
        EXPECT_EQ( size, 0u );
        size = 999u;
        EXPECT_EQ( HIL_APPLICATION_Encode_Message( &context, &message, buffer.data(), buffer.size(),
                                                   &size ),
                   HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
        EXPECT_EQ( size, 0u );

        std::array<std::uint8_t, 23u> encoded{};
        encoded[0] = 0x00u;
        encoded[1] = 0x01u;
        encoded[2] = 0x01u;
        for ( std::size_t i = 0u; i < HIL_APPLICATION_TEST_ID_SIZE; ++i )
        {
            encoded[3u + i] = static_cast<std::uint8_t>( 0x10u + i );
        }
        encoded[19] = static_cast<std::uint8_t>( type );
        encoded[20] = 0x00u;
        encoded[21] = 0x00u;
        encoded[22] = 0x00u;

        size = 999u;
        EXPECT_EQ(
            HIL_APPLICATION_Decode_Storage_Size( &context, encoded.data(), encoded.size(), &size ),
            HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
        EXPECT_EQ( size, 0u );
        HIL_Application_Message_T decoded{};
        decoded.type     = HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT;
        std::size_t used = 999u;
        EXPECT_EQ( HIL_APPLICATION_Decode_Message( &context, encoded.data(), encoded.size(),
                                                   &decoded, nullptr, 0u, &used ),
                   HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
        EXPECT_EQ( decoded.type, HIL_APPLICATION_MESSAGE_TYPE_INVALID );
        EXPECT_EQ( used, 0u );
        size = 999u;
        EXPECT_EQ( HIL_APPLICATION_Validate_Encoded_Message( &context, encoded.data(),
                                                             encoded.size(), &size ),
                   HIL_APPLICATION_STATUS_NOT_IMPLEMENTED );
        EXPECT_EQ( size, 0u );
    }
}
