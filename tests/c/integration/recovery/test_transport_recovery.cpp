#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "support/transport_pair_harness.hpp"

namespace {

constexpr std::size_t MaxMessage = HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE;
constexpr std::size_t MaxOutput  = HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE;

using hil_rig_protocol::test::TransportPairHarness;
using hil_rig_protocol::test::TransportPairInitializationResult;
using hil_rig_protocol::test::TransportTestDirection;
using hil_rig_protocol::test::TransportTestEndpoint;
using hil_rig_protocol::test::TransportTestEndpointConfig;
using hil_rig_protocol::test::TransportTestHarnessStatus;
using hil_rig_protocol::test::TransportTestOutputItem;

/**
 * @brief Assert that the pair support layer and both real Transport initializations succeeded.
 */
void AssertPairInitialized( const TransportPairInitializationResult& initialization )
{
    ASSERT_EQ( initialization.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( initialization.host_status.has_value() );
    ASSERT_TRUE( initialization.rig_status.has_value() );
    ASSERT_EQ( *initialization.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *initialization.rig_status, HIL_TRANSPORT_STATUS_OK );
}

/**
 * @brief Copy the endpoint's public snapshot into an existing recovery-test variable.
 * @details This compatibility helper keeps the existing assertions readable while
 * routing observations through TransportTestEndpoint rather than exposing Context().
 */
HIL_Transport_Status_T GetStatus( const TransportTestEndpoint&           endpoint,
                                  HIL_Transport_Status_Snapshot_T* const snapshot )
{
    const auto result = endpoint.GetStatus();
    if ( result.status == HIL_TRANSPORT_STATUS_OK && snapshot != nullptr )
    {
        *snapshot = result.snapshot;
    }
    return result.status;
}

/**
 * @brief Offer bytes through the endpoint wrapper and expose the public consumed count.
 */
HIL_Transport_Status_T ReceiveBytes( TransportTestEndpoint&    endpoint,
                                     const std::uint8_t* const bytes, const std::size_t size,
                                     std::size_t* const bytes_consumed )
{
    const auto result = endpoint.ReceiveBytes( bytes, size );
    if ( bytes_consumed != nullptr )
    {
        *bytes_consumed = result.bytes_consumed;
    }
    return result.status;
}

/**
 * @brief Read an event through the endpoint wrapper into an existing scenario variable.
 */
HIL_Transport_Status_T ReadEvent( TransportTestEndpoint&       endpoint,
                                  HIL_Transport_Event_T* const event )
{
    const auto result = endpoint.ReadEvent();
    if ( result.status == HIL_TRANSPORT_STATUS_OK && event != nullptr )
    {
        *event = result.event;
    }
    return result.status;
}

/**
 * @brief Accept and immediately deliver one exact output item using the persistent pair link.
 * @details The pair harness moves the handle returned by AcceptOutput(), so an older
 * delayed item in the same direction cannot be substituted for the newly accepted one.
 */
void TransferOneOutput( TransportPairHarness& pair, const TransportTestDirection direction,
                        const std::uint32_t now_ms )
{
    pair.SetBothTimes( now_ms );
    const auto transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_EQ( transfer.accept.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_EQ( transfer.delivery.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( transfer.delivery.bytes_consumed, transfer.delivery.bytes_offered );
}

/**
 * @brief Accept one output into the persistent link and remove that exact item for later use.
 * @details The returned bytes represent external traffic that was accepted and committed
 * by the sender but is deliberately delayed outside the peer Transport context.
 */
std::vector<std::uint8_t> TakeOneOutput( TransportPairHarness&        pair,
                                         const TransportTestDirection direction,
                                         const std::uint32_t          now_ms )
{
    pair.SetBothTimes( now_ms );
    TransportTestEndpoint& sender =
        ( direction == TransportTestDirection::HostToRig ) ? pair.Host() : pair.Rig();

    const auto accepted = pair.Link().AcceptOutput( sender, now_ms );
    EXPECT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    EXPECT_TRUE( accepted.transport_status.has_value() );
    if ( accepted.harness_status != TransportTestHarnessStatus::Ok
         || !accepted.transport_status.has_value()
         || *accepted.transport_status != HIL_TRANSPORT_STATUS_OK )
    {
        return {};
    }

    TransportTestOutputItem item{};
    EXPECT_TRUE( accepted.handle.has_value() );
    if ( !accepted.handle.has_value() )
    {
        return {};
    }
    EXPECT_TRUE( pair.Link().TakeAccepted( *accepted.handle, item ) );
    return item.bytes;
}

/**
 * @brief Accept one output and discard it in the simulated external link.
 * @details Existing retry/exhaustion scenarios use this only when no older accepted
 * item is identified by the handle returned from AcceptOutput(), so complete loss
 * does not depend on the order of any other accepted traffic.
 */
void DropOneOutput( TransportPairHarness& pair, const TransportTestDirection direction,
                    const std::uint32_t now_ms )
{
    pair.SetBothTimes( now_ms );
    TransportTestEndpoint& sender =
        ( direction == TransportTestDirection::HostToRig ) ? pair.Host() : pair.Rig();
    const auto accepted = pair.Link().AcceptOutput( sender, now_ms );
    ASSERT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( accepted.transport_status.has_value() );
    ASSERT_EQ( *accepted.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_GT( accepted.size, 0u );
    ASSERT_TRUE( accepted.handle.has_value() );
    ASSERT_TRUE( pair.Link().DropAccepted( *accepted.handle ) );
}

/**
 * @brief Inject opaque historical bytes into the persistent pair link and deliver them.
 * @details The input direction is derived from the receiver role. Complete consumption
 * is expected by these pre-existing recovery scenarios; future backpressure scenarios
 * can call Link().DeliverReady() directly and retain the unconsumed suffix.
 */
void DeliverBytes( TransportPairHarness& pair, TransportTestEndpoint& receiver,
                   const std::uint8_t* const bytes, const std::size_t size )
{
    const auto direction =
        hil_rig_protocol::test::TransportTestLink::InputDirectionForRole( receiver.Role() );
    ASSERT_TRUE( direction.has_value() );
    pair.Link().InjectReadyBytes( *direction, bytes, size );
    const auto delivered = pair.Link().DeliverReady( receiver );
    ASSERT_EQ( delivered.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( delivered.transport_status.has_value() );
    ASSERT_EQ( *delivered.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( delivered.bytes_consumed, size );
    ASSERT_EQ( pair.Link().ReadyByteCount( *direction ), 0u );
}

/**
 * @brief Drain setup events and require the public queue to terminate normally at NOT_READY.
 */
void DrainEvents( TransportTestEndpoint& endpoint )
{
    const auto drained = endpoint.DrainEvents();
    ASSERT_EQ( drained.terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

/**
 * @brief Establish a clean pair using the original explicit caller-operation sequence.
 * @details Recovery scenarios intentionally depend on precise ordering after setup.
 * Keeping the baseline explicit preserves their existing behaviour while all byte
 * movement now passes through the pair's persistent TransportTestLink.
 */
void EstablishPublicPair( TransportPairHarness& pair )
{
    auto& host = pair.Host();
    auto& rig  = pair.Rig();

    ASSERT_EQ( host.Process( 1u ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::HostToRig, 2u );
    ASSERT_EQ( rig.Process( 3u ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, 4u );
    ASSERT_EQ( host.Process( 5u ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::HostToRig, 6u );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, 7u );

    const auto host_status = host.GetStatus();
    const auto rig_status  = rig.GetStatus();
    ASSERT_EQ( host_status.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig_status.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( host_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    ASSERT_EQ( rig_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

/**
 * @brief Deliver an already pending rig RESET and complete the replacement handshake.
 */
void DeliverRigResetAndEstablishReplacement( TransportPairHarness& pair,
                                             const std::uint32_t   first_now_ms )
{
    auto& host = pair.Host();
    auto& rig  = pair.Rig();

    TransferOneOutput( pair, TransportTestDirection::RigToHost, first_now_ms );
    ASSERT_EQ( rig.Process( first_now_ms + 1u ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( host.Process( first_now_ms + 2u ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::HostToRig, first_now_ms + 3u );
    ASSERT_EQ( rig.Process( first_now_ms + 4u ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, first_now_ms + 5u );
    ASSERT_EQ( host.Process( first_now_ms + 6u ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::HostToRig, first_now_ms + 7u );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, first_now_ms + 8u );

    const auto host_status = host.GetStatus();
    const auto rig_status  = rig.GetStatus();
    ASSERT_EQ( host_status.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig_status.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( host_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    ASSERT_EQ( rig_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

}  // namespace

TEST( TransportFacadeRecovery, RigDeliveryExhaustionResetsPeerAndReestablishes )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xA123 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u, 10u, 0u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 3u> payload{ 9u, 8u, 7u };
    ASSERT_EQ( rig.SubmitApplication( payload.data(), payload.size() ), HIL_TRANSPORT_STATUS_OK );
    DropOneOutput( pair, TransportTestDirection::RigToHost, 20u );

    ASSERT_EQ( rig.Process( 31u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_DELIVERY_FAILED );
    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.output_pending, 1u );

    TransferOneOutput( pair, TransportTestDirection::RigToHost, 32u );
    ASSERT_EQ( rig.Process( 33u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );

    HIL_Transport_Status_Snapshot_T host_status{};
    ASSERT_EQ( GetStatus( host, &host_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    ASSERT_EQ( host.Process( 34u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );

    TransferOneOutput( pair, TransportTestDirection::HostToRig, 35u );
    ASSERT_EQ( rig.Process( 36u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, 37u );
    ASSERT_EQ( host.Process( 38u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::HostToRig, 39u );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, 40u );

    ASSERT_EQ( GetStatus( host, &host_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

TEST( TransportFacadeRecovery, RigExplicitResetNotifiesHostAndReestablishes )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xD123 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.output_pending, 1u );

    TransferOneOutput( pair, TransportTestDirection::RigToHost, 20u );
    ASSERT_EQ( rig.Process( 21u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( host.Process( 22u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );

    TransferOneOutput( pair, TransportTestDirection::HostToRig, 23u );
    ASSERT_EQ( rig.Process( 24u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, 25u );
    ASSERT_EQ( host.Process( 26u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::HostToRig, 27u );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, 28u );

    HIL_Transport_Status_Snapshot_T host_status{};
    ASSERT_EQ( GetStatus( host, &host_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
}

TEST( TransportFacadeRecovery, UnrecordedOlderSessionTrafficRecoversAndThenDeliversApplication )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xD223 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 3u> old_payload{ 0x31u, 0x32u, 0x33u };
    ASSERT_EQ( host.SubmitApplication( old_payload.data(), old_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    const auto delayed_old_application =
        TakeOneOutput( pair, TransportTestDirection::HostToRig, 20u );
    ASSERT_FALSE( delayed_old_application.empty() );

    /* Replace the session twice so the delayed frame is older than the one retained marker. */
    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );
    DeliverRigResetAndEstablishReplacement( pair, 21u );
    DrainEvents( host );
    DrainEvents( rig );
    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );
    DeliverRigResetAndEstablishReplacement( pair, 40u );
    DrainEvents( host );
    DrainEvents( rig );

    DeliverBytes( pair, rig, delayed_old_application.data(), delayed_old_application.size() );

    auto rig_status = rig.GetStatus();
    ASSERT_EQ( rig_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.snapshot.output_pending, 1u );
    EXPECT_EQ( rig_status.snapshot.application_message_pending, 0u );

    DeliverRigResetAndEstablishReplacement( pair, 60u );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 4u> replacement_payload{ 0x41u, 0x42u, 0x43u, 0x44u };
    ASSERT_EQ( host.SubmitApplication( replacement_payload.data(), replacement_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::HostToRig, 80u );

    const auto received = rig.ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes.size(), replacement_payload.size() );
    EXPECT_TRUE( std::equal( replacement_payload.begin(), replacement_payload.end(),
                             received.bytes.begin() ) );

    TransferOneOutput( pair, TransportTestDirection::RigToHost, 81u );
    const auto host_status = host.GetStatus();
    rig_status             = rig.GetStatus();
    ASSERT_EQ( host_status.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host_status.snapshot.reliable_delivery_pending, 0u );
    EXPECT_EQ( rig_status.snapshot.application_message_pending, 0u );
}

TEST( TransportFacadeRecovery, InFlightPeerTrafficCannotClearPendingRecoveryReset )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xA123 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u, 10u, 0u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 3u> host_payload{ 1u, 2u, 3u };
    const std::array<std::uint8_t, 3u> rig_payload{ 4u, 5u, 6u };

    ASSERT_EQ( host.SubmitApplication( host_payload.data(), host_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    const auto delayed_host_application =
        TakeOneOutput( pair, TransportTestDirection::HostToRig, 20u );
    ASSERT_FALSE( delayed_host_application.empty() );

    ASSERT_EQ( rig.SubmitApplication( rig_payload.data(), rig_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    DropOneOutput( pair, TransportTestDirection::RigToHost, 21u );

    ASSERT_EQ( rig.Process( 31u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_DELIVERY_FAILED );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    ASSERT_EQ( rig_status.output_pending, 1u );

    /*
     * This frame was already accepted by external I/O before the rig abandoned
     * the old session. Receiving it during recovery must not clear RESET(old).
     */
    DeliverBytes( pair, rig, delayed_host_application.data(), delayed_host_application.size() );

    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.output_pending, 1u );

    const auto recovery_reset = TakeOneOutput( pair, TransportTestDirection::RigToHost, 32u );
    ASSERT_FALSE( recovery_reset.empty() );
    DeliverBytes( pair, host, recovery_reset.data(), recovery_reset.size() );

    HIL_Transport_Status_Snapshot_T host_status{};
    ASSERT_EQ( GetStatus( host, &host_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );

    ASSERT_EQ( rig.Process( 33u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
}

TEST( TransportFacadeRecovery, ExplicitResetAfterDeliveryFailureKeepsRecoveryReset )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xA1A3 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u, 10u, 0u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 2u> payload{ 0x11u, 0x22u };
    ASSERT_EQ( rig.SubmitApplication( payload.data(), payload.size() ), HIL_TRANSPORT_STATUS_OK );
    DropOneOutput( pair, TransportTestDirection::RigToHost, 20u );

    ASSERT_EQ( rig.Process( 30u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_DELIVERY_FAILED );

    /* Caller-requested recovery must not erase the RESET produced by exhaustion. */
    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.output_pending, 1u );

    const auto recovery_reset = TakeOneOutput( pair, TransportTestDirection::RigToHost, 31u );
    ASSERT_FALSE( recovery_reset.empty() );
    DeliverBytes( pair, host, recovery_reset.data(), recovery_reset.size() );

    HIL_Transport_Status_Snapshot_T host_status{};
    ASSERT_EQ( GetStatus( host, &host_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
}

TEST( TransportFacadeRecovery, RepeatedExplicitResetPreservesPeerSynchronization )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xA223 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );

    /* A second Reset while RESET(old) is still READY must preserve it. */
    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );

    const auto first_peek = rig.PeekOutput( MaxOutput );
    ASSERT_EQ( first_peek.status, HIL_TRANSPORT_STATUS_OK );
    const std::size_t first_size = first_peek.required_size;
    ASSERT_GT( first_size, 0u );

    /* Explicit Reset may invalidate a prior peek, but it must re-own RESET(old). */
    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( rig_status.output_pending, 1u );

    const auto recovery_reset = TakeOneOutput( pair, TransportTestDirection::RigToHost, 20u );
    ASSERT_EQ( recovery_reset.size(), first_size );
    EXPECT_TRUE(
        std::equal( recovery_reset.begin(), recovery_reset.end(), first_peek.bytes.begin() ) );

    DeliverBytes( pair, host, recovery_reset.data(), recovery_reset.size() );

    HIL_Transport_Status_Snapshot_T host_status{};
    ASSERT_EQ( GetStatus( host, &host_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
}

TEST( TransportFacadeRecovery, ResetAndReplacementInitiateCanShareOneReceiveChunk )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xB123 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( host.Reset(), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOneOutput( pair, TransportTestDirection::HostToRig, 20u );
    ASSERT_FALSE( reset.empty() );
    ASSERT_EQ( host.Process( 21u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( pair, TransportTestDirection::HostToRig, 22u );
    ASSERT_FALSE( initiate.empty() );

    std::vector<std::uint8_t> combined = reset;
    combined.insert( combined.end(), initiate.begin(), initiate.end() );
    DeliverBytes( pair, rig, combined.data(), combined.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.output_pending, 0u );
    ASSERT_EQ( rig.Process( 23u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 1u );
}

TEST( TransportFacadeRecovery, ApplicationCompletesHostHandshakeWhenFinalAckIsLost )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xC123 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();

    ASSERT_EQ( host.Process( 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::HostToRig, 2u );
    ASSERT_EQ( rig.Process( 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, 4u );
    ASSERT_EQ( host.Process( 5u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::HostToRig, 6u );

    HIL_Transport_Status_Snapshot_T host_status{};
    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( host, &host_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    ASSERT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    DropOneOutput( pair, TransportTestDirection::RigToHost, 7u );

    const std::array<std::uint8_t, 4u> payload{ 1u, 3u, 5u, 7u };
    ASSERT_EQ( rig.SubmitApplication( payload.data(), payload.size() ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, 8u );

    ASSERT_EQ( GetStatus( host, &host_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host_status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host_status.application_message_pending, 1u );

    const auto received = host.ReadApplication( MaxMessage );
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( received.required_size, payload.size() );
    EXPECT_TRUE( std::equal( payload.begin(), payload.end(), received.bytes.begin() ) );

    TransferOneOutput( pair, TransportTestDirection::HostToRig, 9u );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 0u );
}

TEST( TransportFacadeRecovery, RecoveryResetCommitThenReceiveInitiateDoesNotRequireProcessFirst )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xE123 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOneOutput( pair, TransportTestDirection::RigToHost, 20u );
    ASSERT_FALSE( reset.empty() );
    DeliverBytes( pair, host, reset.data(), reset.size() );

    ASSERT_EQ( host.Process( 21u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( pair, TransportTestDirection::HostToRig, 22u );
    ASSERT_FALSE( initiate.empty() );

    /* No Process() call is made on the rig between RESET commit and receive. */
    DeliverBytes( pair, rig, initiate.data(), initiate.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.output_pending, 0u );

    ASSERT_EQ( rig.Process( 23u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 1u );
}

TEST( TransportFacadeRecovery, DelayedOldApplicationBeforeReplacementInitiateDoesNotAbortHandshake )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xE223 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 3u> payload{ 0x21u, 0x22u, 0x23u };
    ASSERT_EQ( host.SubmitApplication( payload.data(), payload.size() ), HIL_TRANSPORT_STATUS_OK );
    const auto delayed_old_application =
        TakeOneOutput( pair, TransportTestDirection::HostToRig, 20u );
    ASSERT_FALSE( delayed_old_application.empty() );

    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOneOutput( pair, TransportTestDirection::RigToHost, 21u );
    ASSERT_FALSE( reset.empty() );
    DeliverBytes( pair, host, reset.data(), reset.size() );
    ASSERT_EQ( host.Process( 22u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( pair, TransportTestDirection::HostToRig, 23u );
    ASSERT_FALSE( initiate.empty() );

    std::vector<std::uint8_t> combined = delayed_old_application;
    combined.insert( combined.end(), initiate.begin(), initiate.end() );
    DeliverBytes( pair, rig, combined.data(), combined.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.application_message_pending, 0u );

    ASSERT_EQ( rig.Process( 24u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
}

TEST( TransportFacadeRecovery, PartialOldFrameThenReplacementInitiateAdvancesBetweenFrames )
{
    TransportPairHarness pair{};
    const auto           initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xE2A3 ), 10u, 0u, 0u ),
        TransportTestEndpointConfig::Rig( 500u, 0u, 0u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 3u> payload{ 0x2Au, 0x2Bu, 0x2Cu };
    ASSERT_EQ( host.SubmitApplication( payload.data(), payload.size() ), HIL_TRANSPORT_STATUS_OK );
    const auto delayed_old_application =
        TakeOneOutput( pair, TransportTestDirection::HostToRig, 20u );
    ASSERT_GT( delayed_old_application.size(), 1u );
    ASSERT_EQ( delayed_old_application.back(), 0u );

    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );

    /*
     * Leave the parser holding an incomplete frame from the abandoned session.
     * This deliberately prevents receive-entry recovery progression.
     */
    std::size_t consumed = 0u;
    ASSERT_EQ( ReceiveBytes( rig, delayed_old_application.data(),
                             delayed_old_application.size() - 1u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( consumed, delayed_old_application.size() - 1u );

    const auto reset = TakeOneOutput( pair, TransportTestDirection::RigToHost, 21u );
    ASSERT_FALSE( reset.empty() );
    DeliverBytes( pair, host, reset.data(), reset.size() );
    ASSERT_EQ( host.Process( 22u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( pair, TransportTestDirection::HostToRig, 23u );
    ASSERT_FALSE( initiate.empty() );

    /*
     * The old frame becomes complete first. Once it is rejected and consumed,
     * recovery must be reconsidered before the following INITIATE is parsed.
     */
    std::vector<std::uint8_t> combined{ delayed_old_application.back() };
    combined.insert( combined.end(), initiate.begin(), initiate.end() );
    DeliverBytes( pair, rig, combined.data(), combined.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.application_message_pending, 0u );
    EXPECT_EQ( rig_status.output_pending, 0u );

    ASSERT_EQ( rig.Process( 24u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 1u );
}

TEST( TransportFacadeRecovery, DiscardedOldBodyThenReplacementInitiateAdvancesBetweenFrames )
{
    TransportPairHarness pair{};
    const auto           initialization = pair.InitializeConnected(
        TransportTestEndpointConfig::Host( UINT64_C( 0xE2B3 ), 10u, 0u, 0u ),
        TransportTestEndpointConfig::Rig( 500u, 0u, 0u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );

    /*
     * Fill the parser beyond its retained-body capacity without a delimiter so
     * it is discarding abandoned-session input when RESET is committed.
     */
    const std::vector<std::uint8_t> oversized_old_body( MaxOutput, 0x55u );
    std::size_t                     consumed = 0u;
    ASSERT_EQ( ReceiveBytes( rig, oversized_old_body.data(), oversized_old_body.size(), &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( consumed, oversized_old_body.size() );

    const auto reset = TakeOneOutput( pair, TransportTestDirection::RigToHost, 20u );
    ASSERT_FALSE( reset.empty() );
    DeliverBytes( pair, host, reset.data(), reset.size() );
    ASSERT_EQ( host.Process( 21u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( pair, TransportTestDirection::HostToRig, 22u );
    ASSERT_FALSE( initiate.empty() );

    /*
     * The delimiter finishes the discard. After its diagnostic is retained, the
     * following replacement INITIATE in the same chunk must see fresh establishment.
     */
    std::vector<std::uint8_t> combined{ 0u };
    combined.insert( combined.end(), initiate.begin(), initiate.end() );
    DeliverBytes( pair, rig, combined.data(), combined.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.output_pending, 0u );

    ASSERT_EQ( rig.Process( 23u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
    EXPECT_EQ( rig_status.reliable_delivery_pending, 1u );
}

TEST( TransportFacadeRecovery, DeferredStaleDiagnosticAfterResetCommitDoesNotEnterFault )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xE323 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 2u> payload{ 0x31u, 0x32u };
    ASSERT_EQ( host.SubmitApplication( payload.data(), payload.size() ), HIL_TRANSPORT_STATUS_OK );
    const auto delayed_old_application =
        TakeOneOutput( pair, TransportTestDirection::HostToRig, 20u );
    ASSERT_FALSE( delayed_old_application.empty() );

    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );

    /* Fill the four-entry event FIFO with stale-session diagnostics. */
    for ( std::size_t index = 0u; index < 4u; ++index )
    {
        DeliverBytes( pair, rig, delayed_old_application.data(), delayed_old_application.size() );
    }

    std::size_t consumed = 0u;
    EXPECT_EQ( ReceiveBytes( rig, delayed_old_application.data(), delayed_old_application.size(),
                             &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, delayed_old_application.size() );

    /* Commit RESET while the protocol-error publication is still deferred. */
    const auto reset = TakeOneOutput( pair, TransportTestDirection::RigToHost, 21u );
    ASSERT_FALSE( reset.empty() );

    HIL_Transport_Event_T event{};
    ASSERT_EQ( ReadEvent( rig, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );

    EXPECT_EQ( rig.Process( 22u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_NE( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
}

TEST( TransportFacadeRecovery, UnboundRigRejectsIrrelevantApplicationWithoutStartingRecovery )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xE423 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& source_host = pair.Host();
    auto& source_rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( source_host );
    DrainEvents( source_rig );

    const std::array<std::uint8_t, 2u> payload{ 0x41u, 0x42u };
    ASSERT_EQ( source_host.SubmitApplication( payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    const auto unrelated_application =
        TakeOneOutput( pair, TransportTestDirection::HostToRig, 20u );
    ASSERT_FALSE( unrelated_application.empty() );

    TransportTestEndpoint waiting_rig;
    ASSERT_EQ( waiting_rig.InitializeConnected( HIL_TRANSPORT_ROLE_RIG,
                                                HIL_TRANSPORT_SESSION_SEED_INVALID, 700u ),
               HIL_TRANSPORT_STATUS_OK );
    DrainEvents( waiting_rig );

    DeliverBytes( pair, waiting_rig, unrelated_application.data(), unrelated_application.size() );

    HIL_Transport_Status_Snapshot_T status{};
    ASSERT_EQ( GetStatus( waiting_rig, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( status.output_pending, 0u );
    EXPECT_EQ( status.application_message_pending, 0u );
}

TEST( TransportFacadeRecovery, RecentlyAbandonedInitiateIsRejectedUntilPhysicalReconnect )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xE523 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();

    ASSERT_EQ( host.Process( 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    const auto old_initiate = TakeOneOutput( pair, TransportTestDirection::HostToRig, 2u );
    ASSERT_FALSE( old_initiate.empty() );
    DeliverBytes( pair, rig, old_initiate.data(), old_initiate.size() );
    ASSERT_EQ( rig.Process( 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, 4u );
    ASSERT_EQ( host.Process( 5u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    TransferOneOutput( pair, TransportTestDirection::HostToRig, 6u );
    TransferOneOutput( pair, TransportTestDirection::RigToHost, 7u );
    DrainEvents( host );
    DrainEvents( rig );

    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOneOutput( pair, TransportTestDirection::RigToHost, 20u );
    ASSERT_FALSE( reset.empty() );

    /* RESET is committed, so receive auto-enters WAITING_FOR_INITIATE. */
    DeliverBytes( pair, rig, old_initiate.data(), old_initiate.size() );

    HIL_Transport_Status_Snapshot_T status{};
    ASSERT_EQ( GetStatus( rig, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( status.output_pending, 0u );

    /* Physical link restart is the MVP hard-recovery boundary and clears history. */
    ASSERT_EQ( rig.NotifyLink( HIL_TRANSPORT_LINK_STATE_DISCONNECTED, 21u ),
               HIL_TRANSPORT_STATUS_OK );
    DrainEvents( rig );
    ASSERT_EQ( rig.NotifyLink( HIL_TRANSPORT_LINK_STATE_CONNECTED, 22u ), HIL_TRANSPORT_STATUS_OK );
    DrainEvents( rig );

    DeliverBytes( pair, rig, old_initiate.data(), old_initiate.size() );
    ASSERT_EQ( rig.Process( 23u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( status.output_pending, 1u );
}

TEST( TransportFacadeRecovery, DelayedOldAckBeforeReplacementInitiateDoesNotAbortHandshake )
{
    TransportPairHarness pair{};
    const auto           initialization =
        pair.InitializeConnected( TransportTestEndpointConfig::Host( UINT64_C( 0xE623 ), 10u ),
                                  TransportTestEndpointConfig::Rig( 500u ) );
    AssertPairInitialized( initialization );
    auto& host = pair.Host();
    auto& rig  = pair.Rig();
    EstablishPublicPair( pair );
    DrainEvents( host );
    DrainEvents( rig );

    const std::array<std::uint8_t, 2u> payload{ 0x61u, 0x62u };
    ASSERT_EQ( rig.SubmitApplication( payload.data(), payload.size() ), HIL_TRANSPORT_STATUS_OK );
    const auto old_application = TakeOneOutput( pair, TransportTestDirection::RigToHost, 20u );
    ASSERT_FALSE( old_application.empty() );
    DeliverBytes( pair, host, old_application.data(), old_application.size() );
    const auto delayed_old_ack = TakeOneOutput( pair, TransportTestDirection::HostToRig, 21u );
    ASSERT_FALSE( delayed_old_ack.empty() );

    ASSERT_EQ( rig.Reset(), HIL_TRANSPORT_STATUS_OK );
    const auto reset = TakeOneOutput( pair, TransportTestDirection::RigToHost, 22u );
    ASSERT_FALSE( reset.empty() );
    DeliverBytes( pair, host, reset.data(), reset.size() );
    ASSERT_EQ( host.Process( 23u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    const auto initiate = TakeOneOutput( pair, TransportTestDirection::HostToRig, 24u );
    ASSERT_FALSE( initiate.empty() );

    std::vector<std::uint8_t> combined = delayed_old_ack;
    combined.insert( combined.end(), initiate.begin(), initiate.end() );
    DeliverBytes( pair, rig, combined.data(), combined.size() );

    HIL_Transport_Status_Snapshot_T rig_status{};
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig_status.application_message_pending, 0u );

    ASSERT_EQ( rig.Process( 25u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( GetStatus( rig, &rig_status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig_status.output_pending, 1u );
}
