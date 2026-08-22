#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "support/transport_pair_harness.hpp"

namespace {

using hil_rig_protocol::test::TransportPairHarness;
using hil_rig_protocol::test::TransportTestDirection;
using hil_rig_protocol::test::TransportTestEndpoint;
using hil_rig_protocol::test::TransportTestEndpointConfig;
using hil_rig_protocol::test::TransportTestHarnessStatus;
using hil_rig_protocol::test::TransportTestOutputItem;

constexpr std::uint64_t Session             = UINT64_C( 0x0807060504030201 );
constexpr std::uint16_t HostInitialSequence = 0xFFFEu;
constexpr std::uint16_t RigInitialSequence  = 0x5678u;

// version=1, INITIATE, session=0x0807060504030201, seq=0xFFFE,
// ack=0, payload=empty, CRC=0x3A544BCB.
constexpr std::array<std::uint8_t, 20u> GoldenInitiate{
    0x0Du, 0x01u, 0x01u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
    0x08u, 0xFEu, 0xFFu, 0x01u, 0x05u, 0xCBu, 0x4Bu, 0x54u, 0x3Au, 0x00u,
};

// version=1, RESPONSE, session=0x0807060504030201, seq=0x5678,
// ack=0xFFFE, payload=empty, CRC=0x3E388BB3.
constexpr std::array<std::uint8_t, 20u> GoldenResponse{
    0x13u, 0x01u, 0x02u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
    0x08u, 0x78u, 0x56u, 0xFEu, 0xFFu, 0xB3u, 0x8Bu, 0x38u, 0x3Eu, 0x00u,
};

// version=1, CONFIRM, session=0x0807060504030201, seq=0xFFFF,
// ack=0x5678, payload=empty, CRC=0x875A9EDA.
constexpr std::array<std::uint8_t, 20u> GoldenConfirm{
    0x13u, 0x01u, 0x03u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
    0x08u, 0xFFu, 0xFFu, 0x78u, 0x56u, 0xDAu, 0x9Eu, 0x5Au, 0x87u, 0x00u,
};

// version=1, ACK, session=0x0807060504030201, seq=0,
// ack=0xFFFF, payload=empty, CRC=0x9CEA66DB.
constexpr std::array<std::uint8_t, 20u> GoldenAck{
    0x0Bu, 0x01u, 0x05u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
    0x08u, 0x01u, 0x07u, 0xFFu, 0xFFu, 0xDBu, 0x66u, 0xEAu, 0x9Cu, 0x00u,
};

// version=1, APPLICATION, session=0x0807060504030201, wrapped seq=0,
// ack=0, payload={0x11,0x00,0x22}, CRC=0x7F681C57.
constexpr std::array<std::uint8_t, 23u> GoldenApplication{
    0x0Bu, 0x01u, 0x04u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x01u,
    0x01u, 0x01u, 0x02u, 0x11u, 0x06u, 0x22u, 0x57u, 0x1Cu, 0x68u, 0x7Fu, 0x00u,
};

// version=1, RESET, session=0x0807060504030201, seq=0,
// ack=0, payload=empty, CRC=0x9F0618EA.
constexpr std::array<std::uint8_t, 20u> GoldenReset{
    0x0Bu, 0x01u, 0x06u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
    0x08u, 0x01u, 0x01u, 0x01u, 0x05u, 0xEAu, 0x18u, 0x06u, 0x9Fu, 0x00u,
};

constexpr std::array<std::uint8_t, 3u> ApplicationPayload{ 0x11u, 0x00u, 0x22u };

template <std::size_t Size>
std::vector<std::uint8_t> Bytes( const std::array<std::uint8_t, Size>& literal )
{
    return { literal.begin(), literal.end() };
}

void AssertInitialized( const hil_rig_protocol::test::TransportPairInitializationResult& result )
{
    ASSERT_EQ( result.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( result.host_status.has_value() );
    ASSERT_TRUE( result.rig_status.has_value() );
    ASSERT_EQ( *result.host_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *result.rig_status, HIL_TRANSPORT_STATUS_OK );
}

void InitializeCanonicalPair( TransportPairHarness& pair )
{
    AssertInitialized(
        pair.InitializeConnected( TransportTestEndpointConfig::Host( Session, HostInitialSequence ),
                                  TransportTestEndpointConfig::Rig( RigInitialSequence ) ) );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

void TransferExpectOk( TransportPairHarness& pair, const TransportTestDirection direction )
{
    const auto transfer = pair.TransferOneOutput( direction );
    ASSERT_EQ( transfer.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( transfer.accept.transport_status.has_value() );
    ASSERT_TRUE( transfer.delivery.transport_status.has_value() );
    ASSERT_EQ( *transfer.accept.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( *transfer.delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
}

template <std::size_t Size>
void ExpectOutput( TransportTestEndpoint& endpoint, const std::array<std::uint8_t, Size>& expected )
{
    const auto output = endpoint.PeekOutput();
    ASSERT_EQ( output.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output.bytes, Bytes( expected ) );
}

TransportTestOutputItem CommitAndTake( TransportPairHarness& pair, TransportTestEndpoint& endpoint,
                                       const std::uint32_t now_ms )
{
    const auto accepted = pair.Link().AcceptOutput( endpoint, now_ms );
    EXPECT_EQ( accepted.harness_status, TransportTestHarnessStatus::Ok );
    EXPECT_TRUE( accepted.transport_status.has_value() );
    if ( accepted.harness_status != TransportTestHarnessStatus::Ok
         || !accepted.transport_status.has_value()
         || *accepted.transport_status != HIL_TRANSPORT_STATUS_OK || !accepted.handle.has_value() )
    {
        return {};
    }
    TransportTestOutputItem item{};
    EXPECT_TRUE( pair.Link().TakeAccepted( *accepted.handle, item ) );
    return item;
}

template <std::size_t Size> void DeliverLiteral( TransportPairHarness&                 pair,
                                                 TransportTestEndpoint&                receiver,
                                                 const TransportTestDirection          direction,
                                                 const std::array<std::uint8_t, Size>& literal )
{
    pair.Link().InjectReadyBytes( direction, literal.data(), literal.size() );
    const auto delivery = pair.Link().DeliverReady( receiver );
    ASSERT_EQ( delivery.harness_status, TransportTestHarnessStatus::Ok );
    ASSERT_TRUE( delivery.transport_status.has_value() );
    ASSERT_EQ( *delivery.transport_status, HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( delivery.bytes_consumed, literal.size() );
}

void EstablishUsingLiteralVectors( TransportPairHarness& pair )
{
    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    ASSERT_FALSE( CommitAndTake( pair, pair.Host(), pair.HostNow() ).bytes.empty() );
    DeliverLiteral( pair, pair.Rig(), TransportTestDirection::HostToRig, GoldenInitiate );

    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    ASSERT_FALSE( CommitAndTake( pair, pair.Rig(), pair.RigNow() ).bytes.empty() );
    DeliverLiteral( pair, pair.Host(), TransportTestDirection::RigToHost, GoldenResponse );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    ASSERT_FALSE( CommitAndTake( pair, pair.Host(), pair.HostNow() ).bytes.empty() );
    DeliverLiteral( pair, pair.Rig(), TransportTestDirection::HostToRig, GoldenConfirm );

    ASSERT_FALSE( CommitAndTake( pair, pair.Rig(), pair.RigNow() ).bytes.empty() );
    DeliverLiteral( pair, pair.Host(), TransportTestDirection::RigToHost, GoldenAck );

    ASSERT_EQ( pair.Host().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    ASSERT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
}

}  // namespace

TEST( TransportIntegrationGoldenVectors, PublicFacadeProducesCanonicalMvpFrames )
{
    TransportPairHarness pair{};
    InitializeCanonicalPair( pair );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    ExpectOutput( pair.Host(), GoldenInitiate );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );

    ASSERT_EQ( pair.ProcessRig(), HIL_TRANSPORT_STATUS_OK );
    ExpectOutput( pair.Rig(), GoldenResponse );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );

    ASSERT_EQ( pair.ProcessHost(), HIL_TRANSPORT_STATUS_OK );
    ExpectOutput( pair.Host(), GoldenConfirm );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );

    ExpectOutput( pair.Rig(), GoldenAck );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.Host().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );
    ASSERT_EQ( pair.Rig().DrainEvents().terminal_status, HIL_TRANSPORT_STATUS_NOT_READY );

    ASSERT_EQ(
        pair.Host().SubmitApplication( ApplicationPayload.data(), ApplicationPayload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    ExpectOutput( pair.Host(), GoldenApplication );
    TransferExpectOk( pair, TransportTestDirection::HostToRig );
    ASSERT_EQ( pair.Rig().ReadApplication().bytes, Bytes( ApplicationPayload ) );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    ASSERT_EQ( pair.Host().ReadEvent().event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );

    ASSERT_EQ( pair.Host().Reset(), HIL_TRANSPORT_STATUS_OK );
    ExpectOutput( pair.Host(), GoldenReset );
    ASSERT_EQ( pair.Host().CommitOutput( pair.HostNow() ), HIL_TRANSPORT_STATUS_OK );
}

TEST( TransportIntegrationGoldenVectors, LiteralVectorsDrivePublicHandshakeApplicationAndReset )
{
    TransportPairHarness pair{};
    InitializeCanonicalPair( pair );
    EstablishUsingLiteralVectors( pair );

    ASSERT_EQ(
        pair.Host().SubmitApplication( ApplicationPayload.data(), ApplicationPayload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    ASSERT_FALSE( CommitAndTake( pair, pair.Host(), pair.HostNow() ).bytes.empty() );
    DeliverLiteral( pair, pair.Rig(), TransportTestDirection::HostToRig, GoldenApplication );
    const auto received = pair.Rig().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, Bytes( ApplicationPayload ) );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    const auto confirmed = pair.Host().ReadEvent();
    ASSERT_EQ( confirmed.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( confirmed.event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );

    DeliverLiteral( pair, pair.Rig(), TransportTestDirection::HostToRig, GoldenReset );
    const auto reset_status = pair.Rig().GetStatus();
    ASSERT_EQ( reset_status.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( reset_status.snapshot.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( reset_status.snapshot.application_message_pending, 0u );
    const auto reset_event = pair.Rig().ReadEvent();
    ASSERT_EQ( reset_event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( reset_event.event.type, HIL_TRANSPORT_EVENT_SESSION_RESET );
}

TEST( TransportIntegrationGoldenVectors, IntegrityMutationCannotMasqueradeAsGoldenApplication )
{
    TransportPairHarness pair{};
    InitializeCanonicalPair( pair );
    EstablishUsingLiteralVectors( pair );

    ASSERT_EQ(
        pair.Host().SubmitApplication( ApplicationPayload.data(), ApplicationPayload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    ASSERT_FALSE( CommitAndTake( pair, pair.Host(), pair.HostNow() ).bytes.empty() );

    auto mutated = GoldenApplication;
    mutated[4] ^= 0x80u;
    DeliverLiteral( pair, pair.Rig(), TransportTestDirection::HostToRig, mutated );
    EXPECT_EQ( pair.Rig().ReadApplication().status, HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( pair.Rig().GetStatus().snapshot.session_state,
               HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    const auto error = pair.Rig().ReadEvent();
    ASSERT_EQ( error.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( error.event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );

    DeliverLiteral( pair, pair.Rig(), TransportTestDirection::HostToRig, GoldenApplication );
    const auto received = pair.Rig().ReadApplication();
    ASSERT_EQ( received.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( received.bytes, Bytes( ApplicationPayload ) );
    TransferExpectOk( pair, TransportTestDirection::RigToHost );
    EXPECT_EQ( pair.Host().ReadEvent().event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
}
