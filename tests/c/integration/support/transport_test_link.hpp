/**
 * @file transport_test_link.hpp
 * @brief Deterministic in-memory byte link for Transport integration tests.
 *
 * @details The link models the boundary intentionally left outside the Transport
 * library. A sender first exposes an opaque complete output item through
 * Peek_Output(); the simulated external I/O accepts that complete item and only
 * then Commit_Output() is called. After commit the item belongs to this link and
 * may be delayed, dropped, duplicated, corrupted, joined with other output, or
 * split into arbitrary Receive_Bytes() chunks before the peer observes it.
 *
 * The link never decodes Transport frames and therefore cannot make decisions
 * based on session identifiers, sequence numbers, frame types, COBS, or CRC.
 */
#ifndef HIL_RIG_PROTOCOL_TESTS_TRANSPORT_TEST_LINK_HPP
#define HIL_RIG_PROTOCOL_TESTS_TRANSPORT_TEST_LINK_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <vector>

#include "support/transport_test_endpoint.hpp"

namespace hil_rig_protocol::test {

/** @brief Direction of opaque bytes through the simulated duplex link. */
enum class TransportTestDirection
{
    HostToRig,
    RigToHost
};

/**
 * @brief Stable identity for one complete output item owned by the simulated link.
 *
 * @details The direction is part of the handle so exact fault operations do not
 * need a second independently supplied routing argument. The ordinal is unique
 * for the lifetime of one TransportTestLink object, including across Clear() calls.
 */
struct TransportTestOutputHandle
{
    TransportTestDirection direction = TransportTestDirection::HostToRig;
    std::uint64_t          ordinal   = 0u;
};

/**
 * @brief Errors produced by the test harness itself rather than production Transport.
 *
 * @details Integration support must not manufacture HIL_TRANSPORT_STATUS_* values
 * for its own bookkeeping failures because doing so can make a test appear to
 * have observed a production Transport failure when no production call returned it.
 */
enum class TransportTestHarnessStatus
{
    Ok,
    InvalidEndpointRole,
    InvalidPairRoles,
    AcceptedItemNotFound,
    ReceiveContractViolation,
    ServiceStepLimit
};

/** @brief One complete output item accepted from a Transport endpoint. */
struct TransportTestOutputItem
{
    TransportTestOutputHandle handle{};
    std::vector<std::uint8_t> bytes{};
};

/** @brief Result of accepting and committing one endpoint output item. */
struct TransportLinkAcceptResult
{
    TransportTestHarnessStatus               harness_status = TransportTestHarnessStatus::Ok;
    std::optional<HIL_Transport_Status_T>    transport_status{};
    std::optional<TransportTestOutputHandle> handle{};
    std::size_t                              size = 0u;
};

/** @brief Result of one link-to-endpoint Receive_Bytes() delivery operation. */
struct TransportLinkDeliveryResult
{
    TransportTestHarnessStatus            harness_status = TransportTestHarnessStatus::Ok;
    std::optional<HIL_Transport_Status_T> transport_status{};
    std::size_t                           bytes_offered  = 0u;
    std::size_t                           bytes_consumed = 0u;
};

/**
 * @brief Caller-side deterministic model of a full-duplex byte-stream link.
 *
 * @details Each direction retains complete accepted output items until the test
 * chooses what the external medium does with them. Items moved to the ready byte
 * stream lose their frame boundary, allowing several complete outputs to be
 * joined before Receive_Bytes(). DeliverReady() removes only the exact accepted
 * prefix reported by Transport and automatically retains any caller-owned suffix.
 */
class TransportTestLink
{
public:
    /**
     * @brief Accept one complete output item from an endpoint and commit it.
     * @details Direction is derived from sender.Role(), so a test cannot silently
     * queue host output into the rig-to-host path or vice versa. Successful
     * Peek_Output() bytes are copied before Commit_Output() and a failed production
     * call does not create a queued item.
     */
    TransportLinkAcceptResult AcceptOutput( TransportTestEndpoint& sender, std::uint32_t now_ms );

    /** @brief Remove and return the oldest accepted item without delivering it. */
    bool TakeNextAccepted( TransportTestDirection direction, TransportTestOutputItem& item );

    /**
     * @brief Remove a specific accepted item identified by its stable link handle.
     * @details This is used by helpers that accepted one particular output and must
     * not accidentally manipulate an older item already waiting in the same direction.
     */
    bool TakeAccepted( TransportTestOutputHandle handle, TransportTestOutputItem& item );

    /** @brief Drop one exact accepted item by stable handle. */
    bool DropAccepted( TransportTestOutputHandle handle );

    /**
     * @brief Duplicate one exact accepted item and return the duplicate's new handle.
     * @details The original remains accepted and byte-identical. The duplicate is
     * inserted immediately after it but receives a distinct stable identity.
     */
    std::optional<TransportTestOutputHandle> DuplicateAccepted( TransportTestOutputHandle handle );

    /** @brief Move one exact accepted item into the held/delayed collection. */
    bool HoldAccepted( TransportTestOutputHandle handle );

    /** @brief Return one exact held item to the accepted-item queue. */
    bool ReleaseHeld( TransportTestOutputHandle handle );

    /** @brief XOR one byte in one exact accepted opaque item. */
    bool CorruptAcceptedByte( TransportTestOutputHandle handle, std::size_t byte_offset,
                              std::uint8_t xor_mask );

    /** @brief Drop the oldest accepted item to model complete-item loss. */
    bool DropNextAccepted( TransportTestDirection direction );

    /** @brief Duplicate the oldest accepted item while preserving original byte content. */
    bool DuplicateNextAccepted( TransportTestDirection direction );

    /** @brief Hold the oldest accepted item outside the deliverable stream. */
    bool HoldNextAccepted( TransportTestDirection direction );

    /** @brief Return the oldest held item to the accepted-item queue. */
    bool ReleaseOldestHeld( TransportTestDirection direction );

    /**
     * @brief XOR one byte in the oldest accepted opaque item.
     * @details This supports deterministic corruption without using the production
     * decoder in the integration harness.
     */
    bool CorruptNextAcceptedByte( TransportTestDirection direction, std::size_t byte_offset,
                                  std::uint8_t xor_mask );

    /** @brief Append the oldest accepted complete item to the direction's byte stream. */
    bool QueueNextAcceptedForDelivery( TransportTestDirection direction );

    /**
     * @brief Append one exact accepted item to the direction's ready byte stream.
     * @details Unlike QueueNextAcceptedForDelivery(), this preserves the identity of
     * the item returned by AcceptOutput() even when older accepted traffic is queued.
     * Direction comes from the handle and cannot disagree with the item's ownership.
     */
    bool QueueAcceptedForDelivery( TransportTestOutputHandle handle );

    /** @brief Append every accepted complete item to one contiguous byte stream. */
    std::size_t QueueAllAcceptedForDelivery( TransportTestDirection direction );

    /**
     * @brief Inject externally supplied raw bytes into one ready byte stream.
     * @details Direction is explicit here because injected traffic did not originate
     * from a TransportTestEndpoint and therefore has no role from which to derive it.
     */
    void InjectReadyBytes( TransportTestDirection direction, const std::uint8_t* bytes,
                           std::size_t size );

    /** @brief Convenience overload for vector-owned raw bytes. */
    void InjectReadyBytes( TransportTestDirection           direction,
                           const std::vector<std::uint8_t>& bytes );

    /**
     * @brief Offer up to max_bytes of the appropriate ready stream to one endpoint.
     *
     * @details The input direction is derived from receiver.Role(). Only
     * bytes_consumed are removed. If Receive_Bytes() retains a complete frame but
     * returns CAPACITY_EXHAUSTED, the already-consumed prefix remains Transport-owned
     * while this object preserves only the unconsumed suffix. When there are no
     * ready bytes, no Transport call is made and transport_status remains empty.
     */
    TransportLinkDeliveryResult
    DeliverReady( TransportTestEndpoint& receiver,
                  std::size_t            max_bytes = std::numeric_limits<std::size_t>::max() );

    /**
     * @brief Retry work already retained inside Transport without adding new bytes.
     * @details This maps directly to a valid zero-length Receive_Bytes() call.
     */
    TransportLinkDeliveryResult DeliverZeroLength( TransportTestEndpoint& receiver );

    /** @brief Number of complete accepted items not yet moved to the byte stream. */
    std::size_t AcceptedItemCount( TransportTestDirection direction ) const;

    /** @brief Number of delayed/held complete items in this direction. */
    std::size_t HeldItemCount( TransportTestDirection direction ) const;

    /** @brief Number of caller-owned ready bytes not yet accepted by the receiver. */
    std::size_t ReadyByteCount( TransportTestDirection direction ) const;

    /** @brief Remove all simulated traffic while preserving handle-ordinal uniqueness. */
    void Clear();

    /** @brief Derive the external output direction for a valid endpoint role. */
    static std::optional<TransportTestDirection>
    OutputDirectionForRole( HIL_Transport_Role_T role );

    /** @brief Derive the incoming-link direction for a valid endpoint role. */
    static std::optional<TransportTestDirection> InputDirectionForRole( HIL_Transport_Role_T role );

private:
    struct DirectionState
    {
        std::deque<TransportTestOutputItem> accepted{};
        std::deque<TransportTestOutputItem> held{};
        std::vector<std::uint8_t>           ready_bytes{};
        std::size_t                         ready_offset = 0u;
    };

    DirectionState&       State( TransportTestDirection direction );
    const DirectionState& State( TransportTestDirection direction ) const;
    static void           CompactReadyBytes( DirectionState& state );
    static void AppendItemToReady( DirectionState& state, TransportTestOutputItem&& item );

    DirectionState host_to_rig_{};
    DirectionState rig_to_host_{};
    std::uint64_t  next_ordinal_ = 1u;
};

}  // namespace hil_rig_protocol::test

#endif /* HIL_RIG_PROTOCOL_TESTS_TRANSPORT_TEST_LINK_HPP */
