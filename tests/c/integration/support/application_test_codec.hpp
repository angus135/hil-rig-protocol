/**
 * @file application_test_codec.hpp
 * @brief Public-API-only Application codec support for integration tests.
 *
 * @details This helper owns one real HIL_Application_Context_T and caller-owned
 * aligned decode storage. It deliberately contains no endpoint role, lifecycle,
 * transaction, Test-ID acceptance, tick-ordering, or Transport policy. Public
 * statuses are returned to tests without hidden GoogleTest assertions.
 */
#ifndef HIL_RIG_PROTOCOL_TESTS_APPLICATION_TEST_CODEC_HPP
#define HIL_RIG_PROTOCOL_TESTS_APPLICATION_TEST_CODEC_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "hil_rig_protocol/application/application.h"

namespace hil_rig_protocol::test {

constexpr std::size_t kApplicationConfigurationBaseCompleteSize = 220u;
constexpr std::size_t kApplicationInstructionCompleteSize       = 73u;
constexpr std::size_t kApplicationResultCompleteSize            = 62u;
constexpr std::size_t kApplicationExecutionControlCompleteSize  = 28u;
constexpr std::size_t kApplicationPayloadLengthOffset           = 21u;
constexpr std::size_t kApplicationPayloadOffset                 = HIL_APPLICATION_HEADER_SIZE_BYTES;

/** @brief Public statuses and output from the normal sized encode path. */
struct ApplicationEncodeResult
{
    HIL_Application_Status_T  validation_status = HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    HIL_Application_Status_T  sizing_status     = HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    HIL_Application_Status_T  encoding_status   = HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    std::size_t               encoded_size      = 0u;
    std::size_t               output_size       = 0u;
    std::vector<std::uint8_t> bytes{};
};

/** @brief Public statuses and output when encoding without typed size calculation. */
struct ApplicationDirectEncodeResult
{
    HIL_Application_Status_T  validation_status = HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    HIL_Application_Status_T  encoding_status   = HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    std::size_t               output_size       = 0u;
    std::vector<std::uint8_t> bytes{};
};

/** @brief Public statuses and storage publication from one encoded decode attempt. */
struct ApplicationDecodeResult
{
    HIL_Application_Status_T storage_status            = HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    HIL_Application_Status_T encoded_validation_status = HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    HIL_Application_Status_T decode_status             = HIL_APPLICATION_STATUS_INVALID_ARGUMENT;
    std::size_t              required_storage_size     = 0u;
    std::size_t              validation_storage_size   = 0u;
    std::size_t              used_storage_size         = 0u;
    std::size_t              supplied_storage_size     = 0u;
};

/**
 * @brief Stateless public Application codec wrapper for black-box integration tests.
 *
 * @details Decode storage is aligned to HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT
 * and has capacity for the absolute 255-byte variable span. The last decoded
 * message and its backing storage remain owned together by this object, keeping
 * decoded Test Configuration extension pointers valid until the next decode.
 */
class ApplicationTestCodec
{
public:
    /** @brief Initialize from an explicit public Application configuration. */
    HIL_Application_Status_T Initialize( const HIL_Application_Config_T& config );

    /** @brief Initialize from defaults with selected public limits overridden. */
    HIL_Application_Status_T Initialize(
        std::size_t   max_encoded_message_size = HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE,
        std::size_t   max_variable_data_size   = HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE,
        std::uint32_t max_expected_tick_count  = HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT );

    /**
     * @brief Validate, size, allocate exactly, and encode one supported typed message.
     * @details All three public statuses remain separately observable by the test.
     */
    ApplicationEncodeResult EncodeSupportedMessage( const HIL_Application_Message_T& message );

    /**
     * @brief Validate and encode using caller-selected capacity without size calculation.
     * @details This supports intentionally unsized families such as current Execution Control.
     */
    ApplicationDirectEncodeResult
    EncodeMessageWithCapacity( const HIL_Application_Message_T& message,
                               std::size_t                      buffer_capacity );

    /**
     * @brief Query storage, validate encoded bytes, then decode into owned aligned storage.
     * @param encoded_message Complete Application bytes.
     * @param storage_capacity Optional caller-selected decode capacity. When omitted,
     *        exactly the successfully reported required storage size is supplied.
     */
    ApplicationDecodeResult
    DecodeMessage( const std::vector<std::uint8_t>& encoded_message,
                   std::optional<std::size_t>       storage_capacity = std::nullopt );

    /** @brief Last decoded typed message; valid on successful DecodeMessage(). */
    const HIL_Application_Message_T& DecodedMessage() const;

    /** @brief Public configuration copied during the most recent successful initialization. */
    const HIL_Application_Config_T& Config() const;

private:
    HIL_Application_Context_T context_{};
    HIL_Application_Config_T  config_{};
    alignas( HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT )
        std::array<std::uint8_t, HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE> decode_storage_{};
    HIL_Application_Message_T decoded_message_{};
};

/** @brief Distinctive 16-byte Test ID shared by the fixed-subset fixtures. */
HIL_Application_Test_Id_T ApplicationFixtureTestId();

/** @brief Alternate structurally valid Test ID for stateless semantic-boundary tests. */
HIL_Application_Test_Id_T ApplicationAlternateTestId();

/** @brief Representative Test Configuration with every fixed configuration family exercised. */
HIL_Application_Message_T MakeApplicationConfigurationMessage( const std::uint8_t* extension_data,
                                                               std::uint8_t        extension_size );

/** @brief Distinctive valid fixed Test Instruction for tick 0, 1, or 2. */
HIL_Application_Message_T
MakeApplicationInstructionMessage( std::uint32_t             tick_number,
                                   HIL_Application_Test_Id_T test_id = ApplicationFixtureTestId() );

/** @brief Distinctive valid fixed Test Result for tick 0, 1, or 2. */
HIL_Application_Message_T
MakeApplicationResultMessage( std::uint32_t             tick_number,
                              HIL_Application_Test_Id_T test_id = ApplicationFixtureTestId() );

/** @brief Execution Control START fixture using the shared Test ID. */
HIL_Application_Message_T MakeApplicationStartMessage();

/** @brief Deterministic maximum-length Configuration extension. */
std::array<std::uint8_t, HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE>
MakeMaximumApplicationConfigurationExtension();

/** @brief Public-field equality helpers with no test-framework assertions. */
bool ApplicationTestIdsEqual( const HIL_Application_Test_Id_T& expected,
                              const HIL_Application_Test_Id_T& actual );
bool ApplicationConfigurationsEqual( const HIL_Application_Message_T& expected,
                                     const HIL_Application_Message_T& actual );
bool ApplicationInstructionsEqual( const HIL_Application_Message_T& expected,
                                   const HIL_Application_Message_T& actual );
bool ApplicationResultsEqual( const HIL_Application_Message_T& expected,
                              const HIL_Application_Message_T& actual );

}  // namespace hil_rig_protocol::test

#endif /* HIL_RIG_PROTOCOL_TESTS_APPLICATION_TEST_CODEC_HPP */
