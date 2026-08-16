/**
 * @file transport_test_endpoint.hpp
 * @brief Public-API endpoint wrapper used by Transport integration tests.
 *
 * @details The integration suite deliberately drives the production Transport
 * facade in the same way as a firmware or host caller. This support type owns
 * one real HIL_Transport_Context_T plus the caller-provided workspace required
 * by that context. It does not include private Transport headers and does not
 * inspect implementation state.
 */
#ifndef HIL_RIG_PROTOCOL_TESTS_TRANSPORT_TEST_ENDPOINT_HPP
#define HIL_RIG_PROTOCOL_TESTS_TRANSPORT_TEST_ENDPOINT_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "hil_rig_protocol/transport/transport.h"

namespace hil_rig_protocol::test {

/**
 * @brief Integration-test configuration for one public Transport endpoint.
 *
 * @details Defaults intentionally describe an ordinary MVP endpoint. Tests may
 * override only the caller-visible policy they need. The wrapper translates
 * this structure into HIL_Transport_Config_T and obtains workspace capacity via
 * HIL_TRANSPORT_Required_Storage_Size(), so integration tests do not depend on
 * the private MVP workspace layout.
 */
struct TransportTestEndpointConfig
{
    HIL_Transport_Role_T role                  = HIL_TRANSPORT_ROLE_HOST;
    std::size_t   max_application_message_size = HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE;
    std::size_t   max_encoded_frame_size       = HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE;
    std::uint64_t session_seed                 = UINT64_C( 0x1234 );
    std::uint16_t initial_reliable_sequence    = 0u;
    std::uint32_t connection_timeout_ms        = 0u;
    std::uint32_t retransmit_timeout_ms        = 10u;
    std::uint8_t  max_retries                  = 2u;

    /** @brief Construct normal host settings with the supplied session seed. */
    static TransportTestEndpointConfig Host( std::uint64_t seed, std::uint16_t initial_sequence,
                                             std::uint32_t retransmit_timeout_ms = 10u,
                                             std::uint8_t  max_retries           = 2u );

    /** @brief Construct normal rig settings, which always use the invalid local seed. */
    static TransportTestEndpointConfig Rig( std::uint16_t initial_sequence,
                                            std::uint32_t retransmit_timeout_ms = 10u,
                                            std::uint8_t  max_retries           = 2u );
};

/** @brief Result of one public Receive_Bytes() call. */
struct TransportReceiveResult
{
    HIL_Transport_Status_T status         = HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    std::size_t            bytes_offered  = 0u;
    std::size_t            bytes_consumed = 0u;
};

/** @brief Result of one public Peek_Output() call. */
struct TransportPeekResult
{
    HIL_Transport_Status_T    status          = HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    std::size_t               buffer_capacity = 0u;
    std::size_t               required_size   = 0u;
    std::vector<std::uint8_t> bytes{};
};

/** @brief Result of one public Read_Application_Data() call. */
struct TransportApplicationReadResult
{
    HIL_Transport_Status_T    status          = HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    std::size_t               buffer_capacity = 0u;
    std::size_t               required_size   = 0u;
    std::vector<std::uint8_t> bytes{};
};

/** @brief Result of one public Read_Event() call. */
struct TransportEventReadResult
{
    HIL_Transport_Status_T status = HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    HIL_Transport_Event_T  event{};
};

/** @brief Result of draining all immediately readable public Transport events. */
struct TransportEventDrainResult
{
    std::vector<HIL_Transport_Event_T> events{};
    HIL_Transport_Status_T             terminal_status = HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
};

/** @brief Result of one public Get_Status() call. */
struct TransportStatusResult
{
    HIL_Transport_Status_T          status = HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    HIL_Transport_Status_Snapshot_T snapshot{};
};

/**
 * @brief Owns one production Transport context for black-box integration tests.
 *
 * @details Every method is a thin wrapper around the public Transport API. The
 * class exists to centralize caller-owned storage and buffer management, not to
 * make protocol decisions on behalf of Transport. Tests that need a non-OK
 * result receive the real status rather than having assertions hidden inside
 * this support layer.
 *
 * Workspace backing is stored as max_align_t elements. This guarantees at least
 * HIL_TRANSPORT_WORKSPACE_ALIGNMENT while still allowing the production library
 * to receive a byte-addressed caller-owned region of exactly the required size.
 */
class TransportTestEndpoint
{
public:
    TransportTestEndpoint()                                          = default;
    TransportTestEndpoint( const TransportTestEndpoint& )            = delete;
    TransportTestEndpoint& operator=( const TransportTestEndpoint& ) = delete;
    TransportTestEndpoint( TransportTestEndpoint&& )                 = delete;
    TransportTestEndpoint& operator=( TransportTestEndpoint&& )      = delete;

    /**
     * @brief Initialize the endpoint without changing the physical-link state.
     * @param config Caller-visible endpoint settings.
     * @return Status from workspace sizing or HIL_TRANSPORT_Init().
     */
    HIL_Transport_Status_T Initialize( const TransportTestEndpointConfig& config );

    /**
     * @brief Initialize and then report a connected physical link.
     * @details This is a convenience for existing integration scenarios. Tests
     * that exercise disconnected initialization should call Initialize() and
     * NotifyLink() separately.
     */
    HIL_Transport_Status_T InitializeConnected( const TransportTestEndpointConfig& config,
                                                std::uint32_t                      now_ms = 0u );

    /**
     * @brief Compatibility convenience for concise existing integration scenarios.
     * @details Builds TransportTestEndpointConfig from the supplied public values,
     * initializes the endpoint, and reports the physical link connected at time zero.
     */
    HIL_Transport_Status_T InitializeConnected( HIL_Transport_Role_T role, std::uint64_t seed,
                                                std::uint16_t initial_sequence,
                                                std::uint32_t retransmit_timeout_ms = 10u,
                                                std::uint8_t  max_retries           = 2u );

    /** @brief Forward a physical-link observation through the public API. */
    HIL_Transport_Status_T NotifyLink( HIL_Transport_Link_State_T state, std::uint32_t now_ms );

    /** @brief Progress this endpoint using caller-supplied deterministic time. */
    HIL_Transport_Status_T
    Process( std::uint32_t                  now_ms,
             HIL_Transport_Operating_Mode_T mode = HIL_TRANSPORT_OPERATING_MODE_NORMAL );

    /** @brief Explicitly reset the production Transport context. */
    HIL_Transport_Status_T Reset();

    /** @brief Submit one opaque complete Application message. */
    HIL_Transport_Status_T SubmitApplication( const std::uint8_t* data, std::size_t size );

    /** @brief Convenience overload for vector-owned opaque Application bytes. */
    HIL_Transport_Status_T SubmitApplication( const std::vector<std::uint8_t>& bytes );

    /**
     * @brief Peek the next encoded output using the configured maximum buffer size.
     * @details A successful result copies bytes but deliberately does not commit
     * them. External acceptance is modelled separately by TransportTestLink.
     */
    TransportPeekResult PeekOutput();

    /**
     * @brief Peek output with an explicitly caller-selected destination capacity.
     * @details A zero capacity performs the public size-query form using a null
     * destination. This makes BUFFER_TOO_SMALL and pinned-output contracts
     * testable without bypassing the black-box endpoint wrapper.
     */
    TransportPeekResult PeekOutput( std::size_t buffer_capacity );

    /** @brief Perform only the public output-size query without consuming or pinning output. */
    TransportPeekResult QueryOutputSize();

    /** @brief Confirm external acceptance of the currently peeked output item. */
    HIL_Transport_Status_T CommitOutput( std::uint32_t now_ms );

    /**
     * @brief Offer arbitrary external bytes and preserve exact consumption data.
     * @details data may be nullptr only when size is zero, matching the public API.
     */
    TransportReceiveResult ReceiveBytes( const std::uint8_t* data, std::size_t size );

    /** @brief Convenience overload for contiguous vector-owned input bytes. */
    TransportReceiveResult ReceiveBytes( const std::vector<std::uint8_t>& bytes );

    /** @brief Read one pending Application message using the configured maximum capacity. */
    TransportApplicationReadResult ReadApplication();

    /**
     * @brief Read Application data with an explicitly caller-selected capacity.
     * @details A zero capacity performs the public size-query form and therefore
     * permits undersized-buffer preservation to be exercised through this wrapper.
     */
    TransportApplicationReadResult ReadApplication( std::size_t buffer_capacity );

    /** @brief Perform only the public Application-message size query. */
    TransportApplicationReadResult QueryApplicationSize();

    /** @brief Read at most one pending high-level Transport event. */
    TransportEventReadResult ReadEvent();

    /** @brief Read the current public Transport status snapshot. */
    TransportStatusResult GetStatus() const;

    /**
     * @brief Consume all currently readable events and expose why draining stopped.
     * @return Events plus the first non-OK public Read_Event() status.
     * @details Normal complete draining ends with NOT_READY. Returning that
     * terminal status explicitly prevents setup helpers from silently treating an
     * INTERNAL_ERROR or other unexpected result as an empty event queue.
     */
    TransportEventDrainResult DrainEvents();

    /** @brief Return the public configuration copied into this test endpoint. */
    const HIL_Transport_Config_T& Config() const;

    /** @brief Return the caller-selected endpoint role used at initialization. */
    HIL_Transport_Role_T Role() const;

private:
    HIL_Transport_Context_T       context_{};
    HIL_Transport_Config_T        config_{};
    HIL_Transport_Role_T          role_ = HIL_TRANSPORT_ROLE_HOST;
    std::vector<std::max_align_t> workspace_{};
    bool                          initialized_ = false;
};

}  // namespace hil_rig_protocol::test

#endif /* HIL_RIG_PROTOCOL_TESTS_TRANSPORT_TEST_ENDPOINT_HPP */
