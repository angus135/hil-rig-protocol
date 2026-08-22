#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

extern "C"
{
#include "transport/internal/mvp/transport_events_mvp.h"
#include "transport/internal/mvp/transport_frame_codec_mvp.h"
#include "transport/internal/mvp/transport_handshake_mvp.h"
#include "transport/internal/mvp/transport_output_mvp.h"
#include "transport/internal/mvp/transport_reliability_mvp.h"
#include "transport/internal/mvp/transport_receive_mvp.h"
#include "transport/internal/mvp/transport_session_mvp.h"
}

namespace {

constexpr std::size_t ApplicationCapacity = 32u;
constexpr std::size_t EncodedCapacity     = 96u;

struct ReceiveHarness
{
    HIL_Transport_Mvp_Root_T                                                       root{};
    std::array<std::uint8_t, ApplicationCapacity>                                  submitted{};
    std::array<std::uint8_t, EncodedCapacity>                                      encoded{};
    std::array<std::uint8_t, EncodedCapacity - 1u>                                 parser{};
    std::array<std::uint8_t, ApplicationCapacity + HIL_TRANSPORT_MVP_RAW_OVERHEAD> codec{};
    std::array<std::uint8_t, ApplicationCapacity>                                  received{};

    void Initialize( HIL_Transport_Role_T role, std::uint64_t seed, std::uint16_t initial_sequence )
    {
        root                                          = {};
        root.base.config.max_application_message_size = ApplicationCapacity;
        root.base.config.max_encoded_frame_size       = EncodedCapacity;
        root.base.config.session_seed                 = seed;
        root.base.config.initial_reliable_sequence    = initial_sequence;
        root.base.config.retransmit_timeout_ms        = 10u;
        root.base.config.max_retries                  = 2u;
        root.base.role                                = role;
        root.base.link_state                          = HIL_TRANSPORT_LINK_STATE_CONNECTED;
        root.base.session_state                       = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Init( &root.session, role, seed, initial_sequence ),
                   HIL_TRANSPORT_STATUS_OK );
        root.session.link_state          = HIL_TRANSPORT_LINK_STATE_CONNECTED;
        root.session.link_state_observed = 1u;
        root.submitted_message           = submitted.data();
        root.encoded_output              = encoded.data();
        root.encoded_output_capacity     = encoded.size();
        ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &root.parser, parser.data(), parser.size() ),
                   HIL_TRANSPORT_STATUS_OK );
        root.codec_scratch      = codec.data();
        root.codec_scratch_size = codec.size();
        root.received_message   = received.data();
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Reset( &root ), HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Begin_Establishment( &root ),
                   HIL_TRANSPORT_STATUS_OK );
    }

    HIL_Transport_Context_T Context()
    {
        return HIL_Transport_Context_T{ &root, sizeof( root ),
                                        HIL_TRANSPORT_INTERNAL_INITIALIZATION_COOKIE };
    }

    void SetEstablished( std::uint64_t session_identifier = 77u )
    {
        root.base.session_state               = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
        root.session.state                    = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
        root.session.handshake_phase          = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED;
        root.session.session_identifier       = session_identifier;
        root.session.session_identifier_valid = 1u;
    }

    void SetRecentlyAbandoned( std::uint64_t session_identifier )
    {
        root.recently_abandoned_session_identifier       = session_identifier;
        root.recently_abandoned_session_identifier_valid = 1u;
    }
};

HIL_Transport_Mvp_Frame_T EmptyFrame( HIL_Transport_Mvp_Frame_Type_T type,
                                      std::uint64_t session_identifier, std::uint16_t sequence,
                                      std::uint16_t acknowledgement_sequence )
{
    return HIL_Transport_Mvp_Frame_T{
        type, session_identifier, sequence, acknowledgement_sequence, nullptr, 0u };
}

std::vector<std::uint8_t> Encode( const HIL_Transport_Mvp_Frame_T& frame )
{
    std::array<std::uint8_t, ApplicationCapacity + HIL_TRANSPORT_MVP_RAW_OVERHEAD> raw{};
    std::array<std::uint8_t, EncodedCapacity>                                      output{};
    std::size_t                                                                    size = 0u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &frame, ApplicationCapacity, raw.data(), raw.size(),
                                               output.data(), output.size(), &size ),
               HIL_TRANSPORT_STATUS_OK );
    return std::vector<std::uint8_t>( output.begin(), output.begin() + size );
}

std::vector<std::uint8_t> PeekAndCommit( ReceiveHarness& endpoint, std::uint32_t now_ms )
{
    std::array<std::uint8_t, EncodedCapacity> output{};
    std::size_t                               size = 0u;
    EXPECT_EQ(
        HIL_TRANSPORT_MVP_Output_Peek_Output( &endpoint.root, output.data(), output.size(), &size ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Commit_Output( &endpoint.root, now_ms ),
               HIL_TRANSPORT_STATUS_OK );
    return std::vector<std::uint8_t>( output.begin(), output.begin() + size );
}

HIL_Transport_Mvp_Frame_T DecodeTransmission( const std::vector<std::uint8_t>& transmission )
{
    std::array<std::uint8_t, ApplicationCapacity + HIL_TRANSPORT_MVP_RAW_OVERHEAD> raw{};
    HIL_Transport_Mvp_Frame_T                                                      frame{};
    HIL_Transport_Mvp_Decode_Result_T result = HIL_TRANSPORT_MVP_DECODE_MALFORMED;
    EXPECT_GE( transmission.size(), 2u );
    EXPECT_EQ( transmission.back(), 0u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Decode_Frame_View( transmission.data(), transmission.size() - 1u,
                                                    raw.data(), raw.size(), ApplicationCapacity,
                                                    &frame, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_DECODE_VALID );
    frame.payload = nullptr;
    return frame;
}

void FillEvents( ReceiveHarness& endpoint )
{
    const HIL_Transport_Event_T event{ HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,
                                       HIL_TRANSPORT_STATUS_NOT_READY,
                                       HIL_TRANSPORT_FAILURE_PROTOCOL, 0u };
    while ( endpoint.root.event_count < HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &endpoint.root, &event ),
                   HIL_TRANSPORT_STATUS_OK );
    }
}

HIL_Transport_Event_T ReadEvent( ReceiveHarness& endpoint )
{
    HIL_Transport_Event_T event{};
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &endpoint.root, &event ), HIL_TRANSPORT_STATUS_OK );
    return event;
}

void ReceiveWhole( ReceiveHarness& endpoint, const std::vector<std::uint8_t>& bytes )
{
    auto        context  = endpoint.Context();
    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, bytes.data(), bytes.size(), &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( consumed, bytes.size() );
}

void AdvanceHostToWaitingForConfirmAck( ReceiveHarness& host )
{
    auto context = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 2u );
    ReceiveWhole( host, Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 77u, 500u, 10u ) ) );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 4u );
    ASSERT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK );
}

std::vector<std::uint8_t> CompleteHostHandshake( ReceiveHarness& host )
{
    AdvanceHostToWaitingForConfirmAck( host );
    const auto final_ack =
        Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, host.root.session.session_identifier, 0u,
                            host.root.session.retained_transmit_sequence ) );
    ReceiveWhole( host, final_ack );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED );
    return final_ack;
}

TEST( TransportReceive, PublicApiCompletesHandshakeAcrossArbitraryChunkBoundaries )
{
    ReceiveHarness host;
    ReceiveHarness rig;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    auto host_context = host.Context();
    auto rig_context  = rig.Context();

    ASSERT_EQ( HIL_TRANSPORT_Process( &host_context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    auto initiate = PeekAndCommit( host, 2u );
    for ( std::size_t index = 0u; index < initiate.size(); ++index )
    {
        std::size_t consumed = 0u;
        ASSERT_EQ(
            HIL_TRANSPORT_Receive_Bytes( &rig_context, initiate.data() + index, 1u, &consumed ),
            HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( consumed, 1u );
    }

    ASSERT_EQ( HIL_TRANSPORT_Process( &rig_context, 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    auto        response = PeekAndCommit( rig, 4u );
    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &host_context, response.data(), 7u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( consumed, 7u );
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &host_context, response.data() + consumed,
                                            response.size() - consumed, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( consumed, response.size() - 7u );

    ASSERT_EQ( HIL_TRANSPORT_Process( &host_context, 5u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    auto confirm = PeekAndCommit( host, 6u );
    ReceiveWhole( rig, confirm );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );

    auto acknowledgement = PeekAndCommit( rig, 7u );
    ReceiveWhole( host, acknowledgement );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( rig.root.parser.body_ready, 0u );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );
    EXPECT_EQ( ReadEvent( rig ).type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );
}

TEST( TransportReceive, RetainsMalformedBodyUntilProtocolEventCapacityReturns )
{
    ReceiveHarness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 5u );
    FillEvents( rig );
    auto                                  context = rig.Context();
    constexpr std::array<std::uint8_t, 4> Malformed{ 0x02u, 0xFFu, 0xAAu, 0u };

    std::size_t consumed = 0u;
    EXPECT_EQ(
        HIL_TRANSPORT_Receive_Bytes( &context, Malformed.data(), Malformed.size(), &consumed ),
        HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, Malformed.size() );
    EXPECT_EQ( rig.root.parser.body_ready, 1u );

    constexpr std::array<std::uint8_t, 1> Suffix{ 0u };
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, Suffix.data(), Suffix.size(), &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, 0u );
    EXPECT_EQ( rig.root.parser.body_ready, 1u );

    ( void )ReadEvent( rig );
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, 0u );
    EXPECT_EQ( rig.root.parser.body_ready, 0u );
    EXPECT_EQ( rig.root.event_count, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY );
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig.root.event_count, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY );
}

TEST( TransportReceive, OversizeDelimiterRetainsPendingErrorAndExactSuffix )
{
    ReceiveHarness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 5u );
    FillEvents( rig );
    auto context  = rig.Context();
    auto initiate = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 77u, 10u, 0u ) );
    std::vector<std::uint8_t> stream( EncodedCapacity, 0x7Fu );
    stream.push_back( 0u );
    stream.insert( stream.end(), initiate.begin(), initiate.end() );

    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, stream.data(), stream.size(), &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    ASSERT_EQ( consumed, EncodedCapacity + 1u );
    EXPECT_EQ( rig.root.receive_protocol_error_pending, 1u );
    EXPECT_EQ( rig.root.parser.body_ready, 0u );

    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, initiate.data(), initiate.size(), &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, 0u );
    EXPECT_EQ( rig.root.receive_protocol_error_pending, 1u );

    ( void )ReadEvent( rig );
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig.root.receive_protocol_error_pending, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, stream.data() + EncodedCapacity + 1u,
                                            stream.size() - EncodedCapacity - 1u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, initiate.size() );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING );
}

TEST( TransportReceive, PinnedInitiateRetryDefersResponseWithoutCallerResend )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto context = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 2u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 12u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );

    std::array<std::uint8_t, EncodedCapacity> retry{};
    std::size_t                               retry_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Peek_Output( &context, retry.data(), retry.size(), &retry_size ),
               HIL_TRANSPORT_STATUS_OK );
    auto        response = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 77u, 500u, 10u ) );
    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, response.data(), response.size(), &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, response.size() );
    EXPECT_EQ( host.root.parser.body_ready, 1u );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE );

    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &context, 13u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_CONFIRM_PENDING );
}

TEST( TransportReceive, FinalAckWaitsTransactionallyForEventCapacity )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    host.root.base.config.max_retries = 0u;
    auto context                      = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 2u );
    ReceiveWhole( host, Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 77u, 500u, 10u ) ) );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 4u );
    FillEvents( host );
    auto ack = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 77u, 0u, 11u ) );

    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, ack.data(), ack.size(), &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, ack.size() );
    EXPECT_EQ( host.root.parser.body_ready, 1u );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    const auto retained_parser = host.parser;

    EXPECT_EQ( HIL_TRANSPORT_Process( &context, 14u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( host.root.parser.body_ready, 1u );
    EXPECT_EQ( host.parser, retained_parser );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( host.root.session.retransmissions_committed, 0u );
    EXPECT_EQ( host.root.session.reliable_last_committed_ms, 4u );

    ( void )ReadEvent( host );
    EXPECT_EQ( HIL_TRANSPORT_Process( &context, 15u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, ack.size() );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
}

TEST( TransportReceive, ExactDuplicateFinalHostAckIsRepeatedlyConsumedWithoutEventsOrMutation )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    const auto final_ack = CompleteHostHandshake( host );
    ASSERT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );
    ASSERT_EQ( host.root.session.completed_confirm_sequence_valid, 1u );
    ASSERT_EQ( host.root.session.completed_confirm_sequence, 11u );

    const auto session_before            = host.root.session;
    const auto encoded_before            = host.encoded;
    const auto submitted_message_size    = host.root.submitted_message_size;
    const auto submitted_message_pending = host.root.submitted_message_pending;
    const auto received_message_size     = host.root.received_message_size;
    const auto received_message_pending  = host.root.received_message_pending;
    const auto control_output_size       = host.root.control_output_size;
    const auto control_output_state      = host.root.control_output_state;
    const auto output_selection          = host.root.output_selection;

    ReceiveWhole( host, final_ack );
    ReceiveWhole( host, final_ack );

    EXPECT_EQ( 0, std::memcmp( &host.root.session, &session_before, sizeof( session_before ) ) );
    EXPECT_EQ( host.encoded, encoded_before );
    EXPECT_EQ( host.root.submitted_message_size, submitted_message_size );
    EXPECT_EQ( host.root.submitted_message_pending, submitted_message_pending );
    EXPECT_EQ( host.root.received_message_size, received_message_size );
    EXPECT_EQ( host.root.received_message_pending, received_message_pending );
    EXPECT_EQ( host.root.control_output_size, control_output_size );
    EXPECT_EQ( host.root.control_output_state, control_output_state );
    EXPECT_EQ( host.root.output_selection, output_selection );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    HIL_Transport_Event_T event{};
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &host.root, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportReceive, WrongFinalHostAckSequenceStillPublishesProtocolError )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    ( void )CompleteHostHandshake( host );
    ASSERT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );

    ReceiveWhole( host, Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 77u, 0u, 12u ) ) );

    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host.root.session.completed_confirm_sequence_valid, 1u );
    EXPECT_EQ( host.root.session.completed_confirm_sequence, 11u );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    HIL_Transport_Event_T event{};
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &host.root, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportReceive, DuplicateFinalHostAckIsConsumedWhileEventQueueIsFull )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    const auto final_ack = CompleteHostHandshake( host );
    ASSERT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );
    FillEvents( host );
    const auto  session_before = host.root.session;
    auto        context        = host.Context();
    std::size_t consumed       = 0u;

    EXPECT_EQ(
        HIL_TRANSPORT_Receive_Bytes( &context, final_ack.data(), final_ack.size(), &consumed ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, final_ack.size() );
    EXPECT_EQ( host.root.event_count, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY );
    EXPECT_EQ( 0, std::memcmp( &host.root.session, &session_before, sizeof( session_before ) ) );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
}

TEST( TransportReceive, ResetAndReplacementEstablishmentClearCompletedConfirmMarker )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    ( void )CompleteHostHandshake( host );
    ASSERT_EQ( host.root.session.completed_confirm_sequence_valid, 1u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Explicit_Reset( &host.root ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.session.completed_confirm_sequence, 0u );
    EXPECT_EQ( host.root.session.completed_confirm_sequence_valid, 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Begin_Establishment( &host.root ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.session.session_identifier, 78u );
    EXPECT_EQ( host.root.session.completed_confirm_sequence, 0u );
    EXPECT_EQ( host.root.session.completed_confirm_sequence_valid, 0u );
}

TEST( TransportReceive, ActiveApplicationAckTakesPriorityOverCompletedConfirmMarker )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    ( void )CompleteHostHandshake( host );
    ASSERT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );
    const std::array<std::uint8_t, 2u> payload{ 0x31u, 0x32u };
    auto                               context = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Submit_Application_Data( &context, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 20u );
    ASSERT_EQ( host.root.session.retained_reliable_frame_type,
               HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    const auto application_sequence = host.root.session.retained_transmit_sequence;

    ReceiveWhole(
        host, Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 77u, 0u, application_sequence ) ) );

    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( host.root.submitted_message_pending, 0u );
    EXPECT_EQ( host.root.session.completed_confirm_sequence, 11u );
    EXPECT_EQ( host.root.session.completed_confirm_sequence_valid, 1u );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
    HIL_Transport_Event_T event{};
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &host.root, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportReceive, UnrelatedFinalHandshakeAckTriggersRecoveryForCurrentSession )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    AdvanceHostToWaitingForConfirmAck( host );
    host.SetRecentlyAbandoned( 42u );
    const auto unrelated_ack = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 99u, 0u, 11u ) );

    auto        context  = host.Context();
    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, unrelated_ack.data(), unrelated_ack.size(),
                                            &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, unrelated_ack.size() );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.session.state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_RESET );

    const auto reset = DecodeTransmission( PeekAndCommit( host, 5u ) );
    EXPECT_EQ( reset.type, HIL_TRANSPORT_MVP_FRAME_RESET );
    EXPECT_EQ( reset.session_identifier, 77u );
}

TEST( TransportReceive, RecordedAbandonedFinalHandshakeAckPreservesCurrentAttempt )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    AdvanceHostToWaitingForConfirmAck( host );
    host.SetRecentlyAbandoned( 42u );
    const auto session_before  = host.root.session;
    const auto reliable_before = host.encoded;
    const auto stale_ack       = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 42u, 0u, 11u ) );

    ReceiveWhole( host, stale_ack );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK );
    EXPECT_EQ( host.root.session.reliable_state, session_before.reliable_state );
    EXPECT_EQ( host.root.session.retained_transmit_sequence,
               session_before.retained_transmit_sequence );
    EXPECT_EQ( host.encoded, reliable_before );
    EXPECT_EQ( host.root.recovery_reset_pending, 0u );
    EXPECT_EQ( host.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    HIL_Transport_Event_T event{};
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &host.root, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportReceive, ConfirmWaitsTransactionallyForOccupiedControlOutput )
{
    ReceiveHarness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    rig.root.base.config.max_retries = 0u;
    auto context                     = rig.Context();
    ReceiveWhole( rig, Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 77u, 10u, 0u ) ) );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( rig, 2u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Publish_Reset( &rig.root, 77u ),
               HIL_TRANSPORT_STATUS_OK );
    auto confirm = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_CONFIRM, 77u, 11u, 500u ) );

    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, confirm.data(), confirm.size(), &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, confirm.size() );
    EXPECT_EQ( rig.root.parser.body_ready, 1u );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    const auto retained_parser = rig.parser;

    EXPECT_EQ( HIL_TRANSPORT_Process( &context, 12u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( rig.root.parser.body_ready, 1u );
    EXPECT_EQ( rig.parser, retained_parser );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( rig.root.session.retransmissions_committed, 0u );
    EXPECT_EQ( rig.root.session.reliable_last_committed_ms, 2u );

    ( void )PeekAndCommit( rig, 13u );
    EXPECT_EQ( HIL_TRANSPORT_Process( &context, 14u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, confirm.size() );
    EXPECT_EQ( rig.root.parser.body_ready, 0u );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
}

TEST( TransportReceive, PendingProtocolErrorSuspendsReliableTimeoutWhileEventCapacityIsFull )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    host.root.base.config.max_retries = 0u;
    auto context                      = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 2u );
    FillEvents( host );
    std::vector<std::uint8_t> oversized( EncodedCapacity, 0x7Fu );
    oversized.push_back( 0u );

    std::size_t consumed = 0u;
    ASSERT_EQ(
        HIL_TRANSPORT_Receive_Bytes( &context, oversized.data(), oversized.size(), &consumed ),
        HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    ASSERT_EQ( consumed, oversized.size() );
    ASSERT_EQ( host.root.receive_protocol_error_pending, 1u );
    const auto retained_reliable = host.encoded;

    EXPECT_EQ( HIL_TRANSPORT_Process( &context, 12u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( host.root.receive_protocol_error_pending, 1u );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( host.root.session.retransmissions_committed, 0u );
    EXPECT_EQ( host.root.session.reliable_last_committed_ms, 2u );
    EXPECT_EQ( host.encoded, retained_reliable );
}

TEST( TransportReceive, IncompatibilityAbandonsBodyPublishesResetAndWaitsForItsCommit )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto context = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 2u );
    auto incompatible = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 77u, 500u, 999u ) );

    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, incompatible.data(), incompatible.size(),
                                            &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_RESET );

    auto reset = PeekAndCommit( host, 3u );
    auto frame = DecodeTransmission( reset );
    EXPECT_EQ( frame.type, HIL_TRANSPORT_MVP_FRAME_RESET );
    EXPECT_EQ( frame.session_identifier, 77u );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 4u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE );
    EXPECT_EQ( host.root.session.session_identifier, 78u );
}

TEST( TransportReceive, UnrelatedResetWhileConnectingTriggersRecoveryForCurrentSession )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto context = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    host.SetRecentlyAbandoned( 42u );
    const auto unrelated_reset = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 99u, 0u, 0u ) );

    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, unrelated_reset.data(),
                                            unrelated_reset.size(), &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, unrelated_reset.size() );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.session.state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( host.root.parser.accumulated_size, 0u );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_RESET );

    const auto reset = DecodeTransmission( PeekAndCommit( host, 2u ) );
    EXPECT_EQ( reset.type, HIL_TRANSPORT_MVP_FRAME_RESET );
    EXPECT_EQ( reset.session_identifier, 77u );
}

TEST( TransportReceive, UnrelatedResetWhileEstablishedClearsApplicationOwnershipForRecovery )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    host.SetEstablished();
    host.SetRecentlyAbandoned( 42u );
    auto                               context = host.Context();
    const std::array<std::uint8_t, 2u> received_payload{ 1u, 2u };
    const HIL_Transport_Mvp_Frame_T    received_application{
        HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE,
        77u,
        10u,
        0u,
        received_payload.data(),
        received_payload.size(),
    };
    ReceiveWhole( host, Encode( received_application ) );
    const std::array<std::uint8_t, 2u> submitted_payload{ 3u, 4u };
    ASSERT_EQ( HIL_TRANSPORT_Submit_Application_Data( &context, submitted_payload.data(),
                                                      submitted_payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( host.root.received_message_pending, 1u );
    ASSERT_EQ( host.root.submitted_message_pending, 1u );

    const auto unrelated_reset = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 99u, 0u, 0u ) );
    std::size_t consumed       = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, unrelated_reset.data(),
                                            unrelated_reset.size(), &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, unrelated_reset.size() );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.received_message_pending, 0u );
    EXPECT_EQ( host.root.received_message_size, 0u );
    EXPECT_EQ( host.root.submitted_message_pending, 0u );
    EXPECT_EQ( host.root.submitted_message_size, 0u );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_RESET );

    const auto reset = DecodeTransmission( PeekAndCommit( host, 1u ) );
    EXPECT_EQ( reset.type, HIL_TRANSPORT_MVP_FRAME_RESET );
    EXPECT_EQ( reset.session_identifier, 77u );
}

TEST( TransportReceive, FullEventsCannotPreventMandatoryIncompatibilityRecovery )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto context = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 2u );
    FillEvents( host );
    auto incompatible = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 99u, 0u, 0u ) );

    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, incompatible.data(), incompatible.size(),
                                            &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, incompatible.size() );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( host.root.parser.accumulated_size, 0u );
    EXPECT_EQ( host.root.event_count, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY );
    EXPECT_EQ( host.root.recovery_reset_pending, 1u );
    EXPECT_EQ( host.root.recovery_reset_session_identifier, 77u );
    const auto reset = DecodeTransmission( PeekAndCommit( host, 3u ) );
    EXPECT_EQ( reset.type, HIL_TRANSPORT_MVP_FRAME_RESET );
    EXPECT_EQ( reset.session_identifier, 77u );
}

TEST( TransportReceive, StaleResetDoesNotAbandonNewerSession )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto context = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    host.SetRecentlyAbandoned( 42u );
    const auto session_before  = host.root.session;
    const auto reliable_before = host.encoded;
    const auto stale_reset     = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 42u, 0u, 0u ) );

    std::size_t consumed = 0u;
    EXPECT_EQ(
        HIL_TRANSPORT_Receive_Bytes( &context, stale_reset.data(), stale_reset.size(), &consumed ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, stale_reset.size() );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( host.root.session.session_identifier, 77u );
    EXPECT_EQ( host.root.session.handshake_phase, session_before.handshake_phase );
    EXPECT_EQ( host.root.session.reliable_state, session_before.reliable_state );
    EXPECT_EQ( host.encoded, reliable_before );
    EXPECT_EQ( host.root.recovery_reset_pending, 0u );
    EXPECT_EQ( host.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    HIL_Transport_Event_T event{};
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &host.root, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportReceive, RecordedAbandonedResponseDoesNotResetNewAttempt )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto context = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    host.root.recently_abandoned_session_identifier       = 42u;
    host.root.recently_abandoned_session_identifier_valid = 1u;
    const auto delayed_response =
        Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 42u, 500u, 10u ) );

    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, delayed_response.data(),
                                            delayed_response.size(), &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, delayed_response.size() );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( host.root.session.session_identifier, 77u );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
}

TEST( TransportReceive, DelayedOlderHandshakeTransmissionDoesNotResetEstablishedSession )
{
    ReceiveHarness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    auto context  = rig.Context();
    auto initiate = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 77u, 10u, 0u ) );
    ReceiveWhole( rig, initiate );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( rig, 2u );
    ReceiveWhole( rig, Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_CONFIRM, 77u, 11u, 500u ) ) );
    ( void )PeekAndCommit( rig, 3u );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( ReadEvent( rig ).type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );

    ReceiveWhole( rig, initiate );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig.root.session.session_identifier, 77u );
    EXPECT_EQ( ReadEvent( rig ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
}

TEST( TransportReceive, EstablishedExpectedApplicationIsRetainedAndAcknowledged )
{
    ReceiveHarness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 5u );
    rig.root.base.session_state               = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    rig.root.session.state                    = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    rig.root.session.handshake_phase          = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED;
    rig.root.session.session_identifier       = 77u;
    rig.root.session.session_identifier_valid = 1u;
    const std::array<std::uint8_t, 3> payload{ 1u, 2u, 3u };
    const HIL_Transport_Mvp_Frame_T   frame{
        HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, 77u, 10u, 0u, payload.data(), payload.size() };

    ReceiveWhole( rig, Encode( frame ) );
    EXPECT_EQ( rig.root.received_message_pending, 1u );
    EXPECT_EQ( rig.root.received_message_size, payload.size() );
    EXPECT_TRUE( std::equal( payload.begin(), payload.end(), rig.received.begin() ) );
    EXPECT_EQ( rig.root.parser.body_ready, 0u );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );
    EXPECT_EQ( rig.root.session.last_accepted_receive_sequence, 10u );
    EXPECT_EQ( rig.root.session.expected_receive_sequence, 11u );
}

TEST( TransportReceive, CorruptDelimitedFrameReportsLossWithoutAbandoningEstablishedSession )
{
    ReceiveHarness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 5u );
    rig.root.base.session_state               = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    rig.root.session.state                    = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    rig.root.session.handshake_phase          = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED;
    rig.root.session.session_identifier       = 77u;
    rig.root.session.session_identifier_valid = 1u;
    auto corrupted = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 77u, 0u, 0u ) );
    corrupted[corrupted.size() - 2u] ^= 0x01u;

    ReceiveWhole( rig, corrupted );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig.root.session.session_identifier, 77u );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( ReadEvent( rig ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
}

TEST( TransportReceive, AcceptedPeerResetRecoversWithoutReplyingWithAnotherReset )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto context = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 2u );

    ReceiveWhole( host, Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 77u, 0u, 0u ) ) );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INITIATE_PENDING );
    EXPECT_EQ( host.root.session.session_identifier, 78u );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( host.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_RESET );
}

TEST( TransportReceive, PeerResetAndReplacementInitiateInOneChunkAreAcceptedWithoutProcessGap )
{
    ReceiveHarness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    rig.root.base.session_state               = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    rig.root.session.state                    = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    rig.root.session.handshake_phase          = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED;
    rig.root.session.session_identifier       = 77u;
    rig.root.session.session_identifier_valid = 1u;

    auto bytes    = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 77u, 0u, 0u ) );
    auto initiate = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 78u, 10u, 0u ) );
    bytes.insert( bytes.end(), initiate.begin(), initiate.end() );

    auto        context  = rig.Context();
    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, bytes.data(), bytes.size(), &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, bytes.size() );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig.root.session.session_identifier, 78u );
    EXPECT_EQ( rig.root.session.session_identifier_valid, 1u );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING );
    EXPECT_EQ( rig.root.session.last_accepted_receive_sequence, 10u );
    EXPECT_EQ( rig.root.session.expected_receive_sequence, 11u );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( ReadEvent( rig ).type, HIL_TRANSPORT_EVENT_SESSION_RESET );

    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    const auto response = DecodeTransmission( PeekAndCommit( rig, 2u ) );
    EXPECT_EQ( response.type, HIL_TRANSPORT_MVP_FRAME_RESPONSE );
    EXPECT_EQ( response.session_identifier, 78u );
    EXPECT_EQ( response.sequence, 500u );
    EXPECT_EQ( response.acknowledgement_sequence, 10u );
}

TEST( TransportReceive, FullEventQueueStillLeavesRigReadyForRetriedInitiateAfterPeerReset )
{
    ReceiveHarness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    rig.root.base.session_state               = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    rig.root.session.state                    = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    rig.root.session.handshake_phase          = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED;
    rig.root.session.session_identifier       = 77u;
    rig.root.session.session_identifier_valid = 1u;
    FillEvents( rig );

    auto reset    = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 77u, 0u, 0u ) );
    auto initiate = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 78u, 10u, 0u ) );
    auto bytes    = reset;
    bytes.insert( bytes.end(), initiate.begin(), initiate.end() );

    auto        context  = rig.Context();
    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, bytes.data(), bytes.size(), &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, reset.size() );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE );

    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &rig.root, &event ), HIL_TRANSPORT_STATUS_OK );
    consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, bytes.data() + reset.size(), initiate.size(),
                                            &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, initiate.size() );
    EXPECT_EQ( rig.root.session.session_identifier, 78u );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING );
}

TEST( TransportReceive, ApplicationCanCompleteHostHandshakeWhenFinalAckWasLost )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto context = host.Context();

    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 2u );
    ReceiveWhole( host, Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 77u, 500u, 10u ) ) );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    const auto confirm = DecodeTransmission( PeekAndCommit( host, 4u ) );
    ASSERT_EQ( confirm.type, HIL_TRANSPORT_MVP_FRAME_CONFIRM );
    ASSERT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK );

    const std::array<std::uint8_t, 3u> payload{ 0x11u, 0x22u, 0x33u };
    const HIL_Transport_Mvp_Frame_T    application{ HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE,
                                                 77u,
                                                 501u,
                                                 0u,
                                                 payload.data(),
                                                 payload.size() };
    ReceiveWhole( host, Encode( application ) );

    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( host.root.session.next_transmit_sequence, 12u );
    EXPECT_EQ( host.root.session.completed_confirm_sequence, confirm.sequence );
    EXPECT_EQ( host.root.session.completed_confirm_sequence_valid, 1u );
    EXPECT_EQ( host.root.session.expected_receive_sequence, 502u );
    ASSERT_EQ( host.root.received_message_pending, 1u );
    ASSERT_EQ( host.root.received_message_size, payload.size() );
    EXPECT_TRUE( std::equal( payload.begin(), payload.end(), host.received.begin() ) );
    EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );

    ReceiveWhole( host,
                  Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 77u, 0u, confirm.sequence ) ) );
    HIL_Transport_Event_T event{};
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &host.root, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host.root.session.completed_confirm_sequence_valid, 1u );

    const auto ack = DecodeTransmission( PeekAndCommit( host, 5u ) );
    EXPECT_EQ( ack.type, HIL_TRANSPORT_MVP_FRAME_ACK );
    EXPECT_EQ( ack.session_identifier, 77u );
    EXPECT_EQ( ack.acknowledgement_sequence, 501u );
}

TEST( TransportReceive, FullEventQueueCannotCarryOldSessionBodyPastRecoveryResetCommit )
{
    ReceiveHarness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    rig.root.base.session_state               = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    rig.root.session.state                    = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    rig.root.session.handshake_phase          = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED;
    rig.root.session.session_identifier       = 77u;
    rig.root.session.session_identifier_valid = 1u;
    rig.root.session.accepted_receive_sequence_valid  = 1u;
    rig.root.session.last_accepted_receive_sequence   = 500u;
    rig.root.session.expected_receive_sequence        = 501u;
    rig.root.session.last_accepted_receive_frame_type = HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Begin_Local_Recovery( &rig.root,
                                                                 HIL_TRANSPORT_FAILURE_DELIVERY ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( rig.root.recovery_reset_pending, 1u );
    FillEvents( rig );

    const std::array<std::uint8_t, 1u> payload{ 0x44u };
    const HIL_Transport_Mvp_Frame_T    old_application{ HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE,
                                                     77u,
                                                     501u,
                                                     0u,
                                                     payload.data(),
                                                     payload.size() };
    const auto                         encoded_application = Encode( old_application );

    auto        context  = rig.Context();
    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, encoded_application.data(),
                                            encoded_application.size(), &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, encoded_application.size() );
    EXPECT_EQ( rig.root.parser.body_ready, 0u );
    EXPECT_EQ( rig.root.receive_protocol_error_pending, 1u );
    EXPECT_EQ( rig.root.recovery_reset_pending, 1u );
    EXPECT_EQ( rig.root.recovery_reset_session_identifier, 77u );

    ( void )PeekAndCommit( rig, 5u );
    EXPECT_EQ( rig.root.recovery_reset_pending, 0u );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );

    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &rig.root, &event ), HIL_TRANSPORT_STATUS_OK );
    consumed = 99u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, 0u );
    EXPECT_EQ( rig.root.receive_protocol_error_pending, 0u );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE );

    /* Zero-length receive may cross the completed recovery boundary itself. */
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 6u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
}

TEST( TransportReceive, NonExpectedApplicationSequencesCannotCompleteHostHandshake )
{
    const std::array<std::uint16_t, 3u> non_expected_sequences{ 500u, 499u, 502u };
    for ( const std::uint16_t application_sequence : non_expected_sequences )
    {
        SCOPED_TRACE( application_sequence );

        ReceiveHarness host;
        host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
        auto context = host.Context();

        ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
                   HIL_TRANSPORT_STATUS_OK );
        ( void )PeekAndCommit( host, 2u );
        ReceiveWhole( host,
                      Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 77u, 500u, 10u ) ) );
        ASSERT_EQ( HIL_TRANSPORT_Process( &context, 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
                   HIL_TRANSPORT_STATUS_OK );
        ( void )PeekAndCommit( host, 4u );
        ASSERT_EQ( host.root.session.handshake_phase,
                   HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK );

        const std::array<std::uint8_t, 1u> payload{ 0xA5u };
        const HIL_Transport_Mvp_Frame_T    application{ HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE,
                                                     77u,
                                                     application_sequence,
                                                     0u,
                                                     payload.data(),
                                                     payload.size() };
        ReceiveWhole( host, Encode( application ) );

        EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
        EXPECT_EQ( host.root.session.state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
        EXPECT_EQ( host.root.recovery_reset_pending, 1u );
        EXPECT_EQ( host.root.recovery_reset_session_identifier, 77u );
        EXPECT_EQ( host.root.received_message_pending, 0u );
        EXPECT_EQ( host.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );

        EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
        EXPECT_EQ( ReadEvent( host ).type, HIL_TRANSPORT_EVENT_SESSION_RESET );

        const auto reset = DecodeTransmission( PeekAndCommit( host, 5u ) );
        EXPECT_EQ( reset.type, HIL_TRANSPORT_MVP_FRAME_RESET );
        EXPECT_EQ( reset.session_identifier, 77u );
        EXPECT_EQ( host.root.recovery_reset_pending, 0u );
    }
}

TEST( TransportReceive, PinnedConfirmRetryDefersApplicationProofUntilCommit )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto context = host.Context();

    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 2u );
    ReceiveWhole( host, Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 77u, 500u, 10u ) ) );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 3u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ( void )PeekAndCommit( host, 4u );

    HIL_Transport_Mvp_Reliability_Outcome_T outcome{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &host.root, 14u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );
    std::array<std::uint8_t, EncodedCapacity> retry{};
    std::size_t                               retry_size = 0u;
    ASSERT_EQ(
        HIL_TRANSPORT_MVP_Output_Peek_Output( &host.root, retry.data(), retry.size(), &retry_size ),
        HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED );

    const std::array<std::uint8_t, 1u> payload{ 0xA5u };
    const HIL_Transport_Mvp_Frame_T    application{ HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE,
                                                 77u,
                                                 501u,
                                                 0u,
                                                 payload.data(),
                                                 payload.size() };
    const auto                         encoded_application = Encode( application );
    std::size_t                        consumed            = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, encoded_application.data(),
                                            encoded_application.size(), &consumed ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( consumed, encoded_application.size() );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( host.root.parser.body_ready, 1u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Output_Commit_Output( &host.root, 15u ), HIL_TRANSPORT_STATUS_OK );
    consumed = 1u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, 0u );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( host.root.received_message_pending, 1u );
}

TEST( TransportReceive, MultipleUnrelatedFramesPreserveFirstRecoveryReset )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto context = host.Context();
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    auto first  = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 76u, 0u, 0u ) );
    auto second = Encode( EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 75u, 0u, 0u ) );
    first.insert( first.end(), second.begin(), second.end() );

    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, first.data(), first.size(), &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, first.size() );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.session.state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.parser.body_ready, 0u );
    EXPECT_EQ( host.root.parser.accumulated_size, 0u );
    EXPECT_EQ( host.root.event_count, 3u );
    EXPECT_EQ( host.root.recovery_reset_pending, 1u );
    EXPECT_EQ( host.root.recovery_reset_session_identifier, 77u );

    const auto reset = DecodeTransmission( PeekAndCommit( host, 2u ) );
    EXPECT_EQ( reset.type, HIL_TRANSPORT_MVP_FRAME_RESET );
    EXPECT_EQ( reset.session_identifier, 77u );
}

TEST( TransportReceive, ValidatesArgumentsDisconnectedPolicyAndPendingFlagInvariant )
{
    ReceiveHarness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    auto        context  = host.Context();
    std::size_t consumed = 99u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, nullptr, 1u, &consumed ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( consumed, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, nullptr, 0u, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    host.root.base.link_state    = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
    host.root.session.link_state = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
    constexpr std::array<std::uint8_t, 1> Byte{ 1u };
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, Byte.data(), Byte.size(), &consumed ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( consumed, 0u );

    host.root.base.link_state                = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    host.root.session.link_state             = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    host.root.receive_protocol_error_pending = 2u;
    EXPECT_EQ( HIL_TRANSPORT_Receive_Bytes( &context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
    EXPECT_EQ( host.root.receive_protocol_error_pending, 0u );
}

TEST( TransportReceive, ExplicitResetClearsOversizePendingState )
{
    ReceiveHarness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 5u );
    rig.root.receive_protocol_error_pending = 1u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Explicit_Reset( &rig.root ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig.root.receive_protocol_error_pending, 0u );
}

}  // namespace
