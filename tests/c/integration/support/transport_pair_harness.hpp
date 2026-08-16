/**
 * @file transport_pair_harness.hpp
 * @brief Two-endpoint orchestration layer for Transport integration scenarios.
 *
 * @details TransportPairHarness composes two real public Transport endpoints, a
 * deterministic duplex byte link, and independent caller-controlled endpoint clocks.
 * It is intentionally
 * a service environment rather than a protocol model: production Transport still
 * decides when to establish, acknowledge, retry, recover, and expose Application data.
 */
#ifndef HIL_RIG_PROTOCOL_TESTS_TRANSPORT_PAIR_HARNESS_HPP
#define HIL_RIG_PROTOCOL_TESTS_TRANSPORT_PAIR_HARNESS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>

#include "support/transport_test_link.hpp"

namespace hil_rig_protocol::test {

/** @brief Result of initializing one host/rig pair. */
struct TransportPairInitializationResult
{
    TransportTestHarnessStatus            harness_status = TransportTestHarnessStatus::Ok;
    std::optional<HIL_Transport_Status_T> host_status{};
    std::optional<HIL_Transport_Status_T> rig_status{};
};

/** @brief Result of servicing both endpoints using their independent deterministic clocks. */
struct TransportPairProcessResult
{
    HIL_Transport_Status_T host_status = HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    HIL_Transport_Status_T rig_status  = HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
};

/**
 * @brief Result of one higher-level harness operation.
 *
 * @details harness_status reports only support-layer failures such as a bounded
 * service loop being exhausted. transport_status, when present, is an actual
 * value returned by production Transport and is never synthesized by the harness.
 */
struct TransportPairOperationResult
{
    TransportTestHarnessStatus            harness_status = TransportTestHarnessStatus::Ok;
    std::optional<HIL_Transport_Status_T> transport_status{};
};

/** @brief Outcome of accepting and immediately delivering one complete output item. */
struct TransportPairTransferResult
{
    TransportTestHarnessStatus  harness_status = TransportTestHarnessStatus::Ok;
    TransportLinkAcceptResult   accept{};
    TransportLinkDeliveryResult delivery{};
};

/**
 * @brief Owns a host endpoint, rig endpoint, simulated link, and two deterministic clocks.
 *
 * @details Normal integration tests can use EstablishCleanSession() to reach a
 * baseline session without encoding handshake knowledge into each scenario.
 * Tests specifically validating handshake/recovery ordering should instead use
 * ProcessHost(), ProcessRig(), Link(), and endpoint wrappers directly.
 *
 * This class never drains Application messages or events automatically because
 * unread public resources are part of the MVP backpressure contract.
 */
class TransportPairHarness
{
public:
    /**
     * @brief Initialize both endpoints but leave their physical links disconnected.
     * @details The pair contract requires host_config.role == HOST and
     * rig_config.role == RIG. A role mismatch is a harness configuration error and
     * no production initialization call is made.
     */
    TransportPairInitializationResult Initialize( const TransportTestEndpointConfig& host_config,
                                                  const TransportTestEndpointConfig& rig_config );

    /**
     * @brief Initialize both endpoints and report both links connected at the same caller time.
     * @details This convenience overload is appropriate for ordinary scenarios that do
     * not care about the endpoints having independent clock epochs.
     */
    TransportPairInitializationResult
    InitializeConnected( const TransportTestEndpointConfig& host_config,
                         const TransportTestEndpointConfig& rig_config, std::uint32_t now_ms = 0u );

    /**
     * @brief Initialize and connect both endpoints using independent caller clock values.
     * @details The host and rig clocks need not share an epoch. This overload supports
     * wrap and timing tests that must prove protocol behaviour does not depend on
     * synchronized clocks.
     */
    TransportPairInitializationResult
    InitializeConnected( const TransportTestEndpointConfig& host_config,
                         const TransportTestEndpointConfig& rig_config, std::uint32_t host_now_ms,
                         std::uint32_t rig_now_ms );

    /**
     * @brief Drive a healthy public service loop from the current harness time.
     * @param max_service_steps Bound preventing a test-harness loop from hanging.
     * @return A harness error only for support-layer failures. Any non-OK
     * production status is returned separately in transport_status.
     * @details The helper never rewinds or resets time. Tests set an initial time
     * explicitly with InitializeConnected(), SetBothTimes(), or the per-endpoint
     * clock setters.
     */
    TransportPairOperationResult EstablishCleanSession( std::size_t max_service_steps = 32u );

    /** @brief Process only the host using the host caller clock. */
    HIL_Transport_Status_T
    ProcessHost( HIL_Transport_Operating_Mode_T mode = HIL_TRANSPORT_OPERATING_MODE_NORMAL );

    /** @brief Process only the rig using the rig caller clock. */
    HIL_Transport_Status_T
    ProcessRig( HIL_Transport_Operating_Mode_T mode = HIL_TRANSPORT_OPERATING_MODE_NORMAL );

    /**
     * @brief Process both endpoints using each endpoint's deterministic current time.
     * @details Both production calls are always made. One side returning a non-OK
     * status must not prevent the independently serviced peer from progressing.
     */
    TransportPairProcessResult
    ProcessBoth( HIL_Transport_Operating_Mode_T mode = HIL_TRANSPORT_OPERATING_MODE_NORMAL );

    /**
     * @brief Accept and immediately deliver the exact newly accepted output item.
     * @details The accepted item's stable handle is used when moving it to the
     * ready byte stream. Older accepted traffic in the same direction therefore
     * cannot be delivered accidentally by this convenience operation.
     */
    TransportPairTransferResult TransferOneOutput( TransportTestDirection direction );

    /** @brief Set only the host caller clock. */
    void SetHostTime( std::uint32_t now_ms );

    /** @brief Set only the rig caller clock. */
    void SetRigTime( std::uint32_t now_ms );

    /** @brief Set both caller clocks to one value for ordinary synchronized test steps. */
    void SetBothTimes( std::uint32_t now_ms );

    /** @brief Advance only the host clock with natural uint32_t wrapping semantics. */
    void AdvanceHostTime( std::uint32_t delta_ms = 1u );

    /** @brief Advance only the rig clock with natural uint32_t wrapping semantics. */
    void AdvanceRigTime( std::uint32_t delta_ms = 1u );

    /** @brief Advance both independent clocks by the same delta. */
    void AdvanceBothTimes( std::uint32_t delta_ms = 1u );

    /** @brief Current time supplied to host-side Process/Commit operations. */
    std::uint32_t HostNow() const;

    /** @brief Current time supplied to rig-side Process/Commit operations. */
    std::uint32_t RigNow() const;

    /** @brief Mutable host endpoint access for scenario-specific public operations. */
    TransportTestEndpoint& Host();

    /** @brief Mutable rig endpoint access for scenario-specific public operations. */
    TransportTestEndpoint& Rig();

    /** @brief Mutable simulated external-link access for deterministic fault injection. */
    TransportTestLink& Link();

    /** @brief Const host endpoint access. */
    const TransportTestEndpoint& Host() const;

    /** @brief Const rig endpoint access. */
    const TransportTestEndpoint& Rig() const;

    /** @brief Const simulated-link access. */
    const TransportTestLink& Link() const;

private:
    TransportPairOperationResult PumpHealthyOutputs( std::size_t max_transfers );
    TransportPairOperationResult TransferPendingDirection( TransportTestDirection direction,
                                                           bool&                  transferred );

    TransportTestEndpoint host_{};
    TransportTestEndpoint rig_{};
    TransportTestLink     link_{};
    std::uint32_t         host_now_ms_ = 0u;
    std::uint32_t         rig_now_ms_  = 0u;
};

}  // namespace hil_rig_protocol::test

#endif /* HIL_RIG_PROTOCOL_TESTS_TRANSPORT_PAIR_HARNESS_HPP */
