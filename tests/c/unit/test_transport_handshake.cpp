#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "transport/internal/mvp/transport_events_mvp.h"
#include "transport/internal/mvp/transport_frame_codec_mvp.h"
#include "transport/internal/mvp/transport_handshake_mvp.h"
#include "transport/internal/mvp/transport_output_mvp.h"
#include "transport/internal/mvp/transport_reliability_mvp.h"
#include "transport/internal/mvp/transport_session_mvp.h"
}

namespace {

constexpr std::size_t ApplicationCapacity = 32u;
constexpr std::size_t EncodedCapacity     = 96u;

struct Harness
{
    HIL_Transport_Mvp_Root_T                                                       root{};
    std::array<std::uint8_t, ApplicationCapacity>                                  submitted{};
    std::array<std::uint8_t, EncodedCapacity>                                      encoded{};
    std::array<std::uint8_t, EncodedCapacity>                                      parser{};
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
};

HIL_Transport_Mvp_Frame_T EmptyFrame( HIL_Transport_Mvp_Frame_Type_T type,
                                      std::uint64_t session_identifier, std::uint16_t sequence,
                                      std::uint16_t acknowledgement_sequence )
{
    return HIL_Transport_Mvp_Frame_T{
        type, session_identifier, sequence, acknowledgement_sequence, nullptr, 0u };
}

bool PeekDecode( Harness& harness, HIL_Transport_Mvp_Frame_T* frame )
{
    std::array<std::uint8_t, EncodedCapacity>                                      output{};
    std::array<std::uint8_t, ApplicationCapacity + HIL_TRANSPORT_MVP_RAW_OVERHEAD> raw{};
    std::array<std::uint8_t, ApplicationCapacity>                                  message{};
    std::size_t                                                                    output_size = 0u;
    std::size_t                       message_size                                             = 0u;
    HIL_Transport_Mvp_Decode_Result_T decode_result = HIL_TRANSPORT_MVP_DECODE_MALFORMED;

    if ( HIL_TRANSPORT_MVP_Output_Peek_Output( &harness.root, output.data(), output.size(),
                                               &output_size )
         != HIL_TRANSPORT_STATUS_OK )
    {
        return false;
    }
    if ( ( output_size < 2u ) || ( output[output_size - 1u] != 0u ) )
    {
        return false;
    }
    if ( HIL_TRANSPORT_MVP_Decode_Frame( output.data(), output_size - 1u, raw.data(), raw.size(),
                                         frame, message.data(), message.size(), &message_size,
                                         &decode_result )
         != HIL_TRANSPORT_STATUS_OK )
    {
        return false;
    }
    return ( decode_result == HIL_TRANSPORT_MVP_DECODE_VALID ) && ( message_size == 0u );
}

bool PeekDecodeCommit( Harness& harness, std::uint32_t now_ms, HIL_Transport_Mvp_Frame_T* frame )
{
    return PeekDecode( harness, frame )
           && ( HIL_TRANSPORT_MVP_Output_Commit_Output( &harness.root, now_ms )
                == HIL_TRANSPORT_STATUS_OK );
}

HIL_Transport_Context_T Context( Harness& harness )
{
    return HIL_Transport_Context_T{ &harness.root, sizeof( harness.root ),
                                    HIL_TRANSPORT_INTERNAL_INITIALIZATION_COOKIE };
}

void ExpectOneEstablishedEvent( Harness& harness )
{
    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &harness.root, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED );
    EXPECT_EQ( event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.failure, HIL_TRANSPORT_FAILURE_NONE );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &harness.root, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportHandshake, CompletesHostHandshakeWithIndependentPeerSequence )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INITIATE_PENDING );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 1u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 100u, &initiate ) );
    EXPECT_EQ( initiate.type, HIL_TRANSPORT_MVP_FRAME_INITIATE );
    EXPECT_EQ( initiate.session_identifier, 77u );
    EXPECT_EQ( initiate.sequence, 10u );
    EXPECT_EQ( initiate.acknowledgement_sequence, 0u );

    const auto response = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 77u, 500u, 10u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_CONFIRM_PENDING );
    EXPECT_EQ( host.root.session.next_transmit_sequence, 11u );
    EXPECT_EQ( host.root.session.expected_receive_sequence, 501u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 2u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK );
    HIL_Transport_Mvp_Frame_T confirm{};
    ASSERT_TRUE( PeekDecodeCommit( host, 200u, &confirm ) );
    EXPECT_EQ( confirm.type, HIL_TRANSPORT_MVP_FRAME_CONFIRM );
    EXPECT_EQ( confirm.session_identifier, 77u );
    EXPECT_EQ( confirm.sequence, 11u );
    EXPECT_EQ( confirm.acknowledgement_sequence, 500u );

    const auto ack = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 77u, 0u, 11u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &ack, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host.root.session.state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED );
    EXPECT_EQ( host.root.base.last_failure, HIL_TRANSPORT_FAILURE_NONE );
    EXPECT_EQ( host.root.session.next_transmit_sequence, 12u );
    ExpectOneEstablishedEvent( host );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &ack, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE );
    EXPECT_EQ( host.root.event_count, 0u );
}

TEST( TransportHandshake, CompletesRigHandshakeAndPublishesFinalAckBeforeEstablishment )
{
    Harness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE );

    const auto initiate = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 77u, 10u, 0u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED );
    EXPECT_EQ( rig.root.session.session_identifier, 77u );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &rig.root, 1u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T response{};
    ASSERT_TRUE( PeekDecodeCommit( rig, 100u, &response ) );
    EXPECT_EQ( response.type, HIL_TRANSPORT_MVP_FRAME_RESPONSE );
    EXPECT_EQ( response.session_identifier, 77u );
    EXPECT_EQ( response.sequence, 500u );
    EXPECT_EQ( response.acknowledgement_sequence, 10u );

    const auto confirm = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_CONFIRM, 77u, 11u, 500u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &confirm, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig.root.session.state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED );
    EXPECT_EQ( rig.root.session.next_transmit_sequence, 501u );
    EXPECT_EQ( rig.root.session.expected_receive_sequence, 12u );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );
    ExpectOneEstablishedEvent( rig );

    HIL_Transport_Mvp_Frame_T ack{};
    ASSERT_TRUE( PeekDecodeCommit( rig, 200u, &ack ) );
    EXPECT_EQ( ack.type, HIL_TRANSPORT_MVP_FRAME_ACK );
    EXPECT_EQ( ack.session_identifier, 77u );
    EXPECT_EQ( ack.sequence, 0u );
    EXPECT_EQ( ack.acknowledgement_sequence, 11u );
}

TEST( TransportHandshake, DefersResponseWhileInitiateRetryIsPinned )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 100u, &initiate ) );

    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &host.root, 110u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );
    HIL_Transport_Mvp_Frame_T retry{};
    ASSERT_TRUE( PeekDecode( host, &retry ) );
    const auto pinned_bytes = host.encoded;
    const auto response     = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 77u, 500u, 10u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( host.root.output_selection, HIL_TRANSPORT_MVP_OUTPUT_RELIABLE );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED );
    EXPECT_EQ( host.encoded, pinned_bytes );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE );
    EXPECT_EQ( host.root.session.accepted_receive_sequence_valid, 0u );
    EXPECT_EQ( host.root.session.last_accepted_receive_frame_type,
               HIL_TRANSPORT_MVP_FRAME_INVALID );
    EXPECT_EQ( host.root.session.next_transmit_sequence, 10u );
    EXPECT_EQ( host.root.event_count, 0u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Output_Commit_Output( &host.root, 111u ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.session.retransmissions_committed, 1u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_CONFIRM_PENDING );
    EXPECT_EQ( host.root.session.expected_receive_sequence, 501u );
    EXPECT_EQ( host.root.session.last_accepted_receive_frame_type,
               HIL_TRANSPORT_MVP_FRAME_RESPONSE );
    EXPECT_EQ( host.root.session.next_transmit_sequence, 11u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE );
    EXPECT_EQ( host.root.session.expected_receive_sequence, 501u );
}

TEST( TransportHandshake, DefersConfirmWhileResponseRetryIsPinned )
{
    Harness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_SESSION_SEED_INVALID, 500u );
    const auto initiate = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 77u, 10u, 0u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &rig.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T response{};
    ASSERT_TRUE( PeekDecodeCommit( rig, 100u, &response ) );

    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &rig.root, 110u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T retry{};
    ASSERT_TRUE( PeekDecode( rig, &retry ) );
    const auto pinned_bytes = rig.encoded;
    const auto confirm      = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_CONFIRM, 77u, 11u, 500u );

    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &confirm, &result ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( rig.root.output_selection, HIL_TRANSPORT_MVP_OUTPUT_RELIABLE );
    EXPECT_EQ( rig.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED );
    EXPECT_EQ( rig.encoded, pinned_bytes );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM );
    EXPECT_EQ( rig.root.session.expected_receive_sequence, 11u );
    EXPECT_EQ( rig.root.session.last_accepted_receive_sequence, 10u );
    EXPECT_EQ( rig.root.session.last_accepted_receive_frame_type,
               HIL_TRANSPORT_MVP_FRAME_INITIATE );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( rig.root.event_count, 0u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Output_Commit_Output( &rig.root, 111u ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &confirm, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED );
    EXPECT_EQ( rig.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( rig.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED );
    EXPECT_EQ( rig.root.session.expected_receive_sequence, 12u );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );
    EXPECT_EQ( rig.root.event_count, 1u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &confirm, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE );
    EXPECT_EQ( rig.root.event_count, 1u );
}

TEST( TransportHandshake, DefersFinalAckWhileConfirmRetryIsPinned )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 77u, 10u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 100u, &initiate ) );
    const auto response = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 77u, 500u, 10u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T confirm{};
    ASSERT_TRUE( PeekDecodeCommit( host, 200u, &confirm ) );

    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &host.root, 210u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T retry{};
    ASSERT_TRUE( PeekDecode( host, &retry ) );
    const auto pinned_bytes = host.encoded;
    const auto ack          = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 77u, 0u, 11u );

    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &ack, &result ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( host.root.output_selection, HIL_TRANSPORT_MVP_OUTPUT_RELIABLE );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED );
    EXPECT_EQ( host.encoded, pinned_bytes );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( host.root.session.expected_receive_sequence, 501u );
    EXPECT_EQ( host.root.session.last_accepted_receive_frame_type,
               HIL_TRANSPORT_MVP_FRAME_RESPONSE );
    EXPECT_EQ( host.root.event_count, 0u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Output_Commit_Output( &host.root, 211u ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &ack, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( host.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED );
    EXPECT_EQ( host.root.event_count, 1u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &ack, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE );
    EXPECT_EQ( host.root.event_count, 1u );
}

TEST( TransportHandshake, DuplicateInitiatePreservesOrReplaysTheSameResponse )
{
    Harness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, 0u, 20u );
    const auto initiate = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 90u, 7u, 0u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &rig.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    const auto retained = rig.encoded;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_READY );
    EXPECT_EQ( rig.encoded, retained );
    HIL_Transport_Mvp_Frame_T response{};
    ASSERT_TRUE( PeekDecodeCommit( rig, 100u, &response ) );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
    EXPECT_EQ( rig.root.session.retransmissions_committed, 0u );
    EXPECT_EQ( rig.encoded, retained );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
}

TEST( TransportHandshake, DuplicateResponsePreservesOrReplaysTheSameConfirmWithPinnedControl )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 44u, 30u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 100u, &initiate ) );
    const auto response = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 44u, 70u, 30u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T confirm{};
    ASSERT_TRUE( PeekDecodeCommit( host, 200u, &confirm ) );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Publish_Reset( &host.root, 44u ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T reset{};
    ASSERT_TRUE( PeekDecode( host, &reset ) );
    EXPECT_EQ( host.root.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE );
    EXPECT_EQ( host.root.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
    EXPECT_EQ( host.root.session.retransmissions_committed, 0u );
}

TEST( TransportHandshake, DuplicateResponseLeavesPinnedRetryStableUntilCommitConsumesIt )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 45u, 30u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 100u, &initiate ) );
    const auto response = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 45u, 70u, 30u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T confirm{};
    ASSERT_TRUE( PeekDecodeCommit( host, 200u, &confirm ) );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
    HIL_Transport_Mvp_Frame_T retry{};
    ASSERT_TRUE( PeekDecode( host, &retry ) );
    EXPECT_EQ( host.root.output_selection, HIL_TRANSPORT_MVP_OUTPUT_RELIABLE );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE );
    EXPECT_EQ( host.root.output_selection, HIL_TRANSPORT_MVP_OUTPUT_RELIABLE );
    EXPECT_EQ( host.root.session.retransmissions_committed, 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Output_Commit_Output( &host.root, 201u ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.session.retransmissions_committed, 1u );
}

TEST( TransportHandshake, DuplicateConfirmReissuesAckWithoutAnotherEvent )
{
    Harness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, 0u, 100u );
    const auto initiate = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 5u, 9u, 0u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &rig.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T response{};
    ASSERT_TRUE( PeekDecodeCommit( rig, 100u, &response ) );
    const auto confirm = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_CONFIRM, 5u, 10u, 100u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &confirm, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig.root.event_count, 1u );
    HIL_Transport_Mvp_Frame_T ack{};
    ASSERT_TRUE( PeekDecodeCommit( rig, 0u, &ack ) );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &confirm, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_DUPLICATE );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );
    EXPECT_EQ( rig.root.event_count, 1u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &confirm, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( rig.root.event_count, 1u );
}

TEST( TransportHandshake, FinalAcceptanceIsRetryableWhenEventOrControlCapacityIsUnavailable )
{
    Harness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, 0u, 100u );
    const auto initiate = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 5u, 9u, 0u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &rig.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T response{};
    ASSERT_TRUE( PeekDecodeCommit( rig, 100u, &response ) );
    const auto confirm = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_CONFIRM, 5u, 10u, 100u );

    const HIL_Transport_Event_T older{ HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,
                                       HIL_TRANSPORT_STATUS_NOT_READY,
                                       HIL_TRANSPORT_FAILURE_PROTOCOL, 0u };
    for ( std::size_t index = 0u; index < HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY; ++index )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &rig.root, &older ), HIL_TRANSPORT_STATUS_OK );
    }
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &confirm, &result ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM );
    EXPECT_EQ( rig.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( rig.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( rig.root.session.expected_receive_sequence, 10u );

    HIL_Transport_Event_T ignored{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &rig.root, &ignored ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Publish_Reset( &rig.root, 5u ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &confirm, &result ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM );
    EXPECT_EQ( rig.root.session.expected_receive_sequence, 10u );
}

TEST( TransportHandshake, HostFinalAckWaitsForEventCapacityWithoutReleasingConfirm )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 8u, 1u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 10u, &initiate ) );
    const auto response = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 8u, 4u, 1u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T confirm{};
    ASSERT_TRUE( PeekDecodeCommit( host, 20u, &confirm ) );
    const auto                  ack = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 8u, 0u, 2u );
    const HIL_Transport_Event_T older{ HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,
                                       HIL_TRANSPORT_STATUS_NOT_READY,
                                       HIL_TRANSPORT_FAILURE_PROTOCOL, 0u };
    for ( std::size_t index = 0u; index < HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY; ++index )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &host.root, &older ),
                   HIL_TRANSPORT_STATUS_OK );
    }

    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &ack, &result ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
}

TEST( TransportHandshake, RejectsWrongRoleIdentitySequenceAcknowledgementAndDuplicateSemantics )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 12u, 10u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 10u, &initiate ) );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;

    auto frame = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 12u, 3u, 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &frame, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE );
    frame = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 13u, 3u, 10u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &frame, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE );
    frame = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 12u, 3u, 99u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &frame, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE );

    frame = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 12u, 3u, 10u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &frame, &result ),
               HIL_TRANSPORT_STATUS_OK );
    frame.acknowledgement_sequence = 11u;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &frame, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE );
    frame = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_CONFIRM, 12u, 3u, 10u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &frame, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE );
}

TEST( TransportHandshake, RejectsInvalidReservedInitiateAndStaleAckWithoutMutation )
{
    Harness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, 0u, 2u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    auto                                       initiate =
        EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, HIL_TRANSPORT_SESSION_SEED_INVALID, 1u, 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE );
    initiate.session_identifier = HIL_TRANSPORT_SESSION_SEED_RESERVED;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE );
    EXPECT_EQ( rig.root.session.session_identifier_valid, 0u );

    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 9u, 4u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T published{};
    ASSERT_TRUE( PeekDecodeCommit( host, 10u, &published ) );
    const auto before = host.root.session;
    const auto ack    = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 9u, 0u, 99u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &ack, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE );
    EXPECT_EQ( host.root.session.reliable_state, before.reliable_state );
    EXPECT_EQ( host.root.session.retained_transmit_sequence, before.retained_transmit_sequence );
}

TEST( TransportHandshake, RejectsUnexpectedConfirmSequenceAndWrongPhaseWithoutMutation )
{
    Harness rig;
    rig.Initialize( HIL_TRANSPORT_ROLE_RIG, 0u, 20u );
    const auto initiate = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 6u, 8u, 0u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &initiate, &result ),
               HIL_TRANSPORT_STATUS_OK );
    const auto premature_confirm = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_CONFIRM, 6u, 9u, 20u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &premature_confirm, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_STALE );
    EXPECT_EQ( rig.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &rig.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T response{};
    ASSERT_TRUE( PeekDecodeCommit( rig, 10u, &response ) );
    const auto wrong_sequence = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_CONFIRM, 6u, 10u, 20u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &rig.root, &wrong_sequence, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE );
    EXPECT_EQ( rig.root.session.expected_receive_sequence, 9u );
    EXPECT_EQ( rig.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
}

TEST( TransportHandshake, HandshakeTransmitAndReceiveSequencesWrapNaturally )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 7u, UINT16_MAX );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 10u, &initiate ) );
    EXPECT_EQ( initiate.sequence, UINT16_MAX );
    const auto response =
        EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 7u, UINT16_MAX, UINT16_MAX );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &response, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.session.next_transmit_sequence, 0u );
    EXPECT_EQ( host.root.session.expected_receive_sequence, 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &host.root, 0u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T confirm{};
    ASSERT_TRUE( PeekDecodeCommit( host, 20u, &confirm ) );
    EXPECT_EQ( confirm.sequence, 0u );
    EXPECT_EQ( confirm.acknowledgement_sequence, UINT16_MAX );
}

TEST( TransportHandshake, ActiveResetAbandonsEveryHandshakePhaseAndClearsDuplicateMetadata )
{
    constexpr std::array<HIL_Transport_Mvp_Handshake_Phase_T, 4> phases{
        HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INITIATE_PENDING,
        HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE,
        HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_CONFIRM_PENDING,
        HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK,
    };

    for ( const auto phase : phases )
    {
        Harness host;
        host.Initialize( HIL_TRANSPORT_ROLE_HOST, 30u, 5u );
        host.root.session.handshake_phase                  = phase;
        host.root.session.last_accepted_receive_sequence   = 7u;
        host.root.session.accepted_receive_sequence_valid  = 1u;
        host.root.session.last_accepted_receive_frame_type = HIL_TRANSPORT_MVP_FRAME_RESPONSE;
        host.root.session.last_accepted_receive_acknowledgement_sequence = 5u;
        const auto reset = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 30u, 0u, 0u );
        HIL_Transport_Mvp_Handshake_Frame_Result_T result;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &reset, &result ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_ACCEPTED );
        EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
        EXPECT_EQ( host.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE );
        EXPECT_EQ( host.root.session.accepted_receive_sequence_valid, 0u );
        EXPECT_EQ( host.root.session.last_accepted_receive_frame_type,
                   HIL_TRANSPORT_MVP_FRAME_INVALID );
    }
}

TEST( TransportHandshake, LinkLossAndExplicitResetClearEveryEffectiveHandshakePhase )
{
    struct PhaseCase
    {
        HIL_Transport_Role_T                role;
        HIL_Transport_Mvp_Handshake_Phase_T phase;
    };
    constexpr std::array<PhaseCase, 7> phases{ {
        { HIL_TRANSPORT_ROLE_HOST, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INITIATE_PENDING },
        { HIL_TRANSPORT_ROLE_HOST, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE },
        { HIL_TRANSPORT_ROLE_HOST, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_CONFIRM_PENDING },
        { HIL_TRANSPORT_ROLE_HOST, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM_ACK },
        { HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE },
        { HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_RESPONSE_PENDING },
        { HIL_TRANSPORT_ROLE_RIG, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_CONFIRM },
    } };

    for ( const auto& phase_case : phases )
    {
        Harness    link_loss;
        const auto seed = phase_case.role == HIL_TRANSPORT_ROLE_HOST ? 100u : 0u;
        link_loss.Initialize( phase_case.role, seed, 1u );
        link_loss.root.session.handshake_phase = phase_case.phase;
        if ( phase_case.phase != HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE )
        {
            link_loss.root.session.session_identifier       = 100u;
            link_loss.root.session.session_identifier_valid = 1u;
        }
        ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Notify_Link_State(
                       &link_loss.root, HIL_TRANSPORT_LINK_STATE_DISCONNECTED ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( link_loss.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
        EXPECT_EQ( link_loss.root.session.handshake_phase,
                   HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE );
        EXPECT_EQ( link_loss.root.session.session_identifier_valid, 0u );

        Harness explicit_reset;
        explicit_reset.Initialize( phase_case.role, seed, 1u );
        explicit_reset.root.session.handshake_phase                 = phase_case.phase;
        explicit_reset.root.session.last_accepted_receive_sequence  = 4u;
        explicit_reset.root.session.accepted_receive_sequence_valid = 1u;
        explicit_reset.root.session.last_accepted_receive_frame_type =
            HIL_TRANSPORT_MVP_FRAME_CONFIRM;
        explicit_reset.root.session.last_accepted_receive_acknowledgement_sequence = 3u;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Explicit_Reset( &explicit_reset.root ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( explicit_reset.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
        EXPECT_EQ( explicit_reset.root.session.handshake_phase,
                   HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE );
        EXPECT_EQ( explicit_reset.root.session.accepted_receive_sequence_valid, 0u );
        EXPECT_EQ( explicit_reset.root.session.last_accepted_receive_frame_type,
                   HIL_TRANSPORT_MVP_FRAME_INVALID );
    }
}

TEST( TransportHandshake, ResetPublicationUsesControlLifecycleAndCannotReplacePinnedOutput )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 50u, 1u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Publish_Reset( &host.root, 50u ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T reset{};
    ASSERT_TRUE( PeekDecode( host, &reset ) );
    EXPECT_EQ( reset.type, HIL_TRANSPORT_MVP_FRAME_RESET );
    EXPECT_EQ( reset.session_identifier, 50u );
    EXPECT_EQ( reset.sequence, 0u );
    EXPECT_EQ( reset.acknowledgement_sequence, 0u );
    EXPECT_EQ( host.root.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Publish_Reset( &host.root, 50u ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Publish_Reset( &host.root, 51u ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( host.root.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
}

TEST( TransportHandshake, ValidatesPrivateInputsWithoutUsingInvalidArgumentForPeerSemantics )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 4u, 0u );
    HIL_Transport_Mvp_Handshake_Frame_Result_T result;
    const auto frame = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 4u, 0u, 0u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( nullptr, &frame, &result ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, nullptr, &result ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &frame, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( nullptr, 0u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Publish_Reset( nullptr, 1u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Publish_Reset( &host.root, 0u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    auto               semantic = frame;
    const std::uint8_t payload  = 1u;
    semantic.payload            = &payload;
    semantic.payload_size       = 1u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &host.root, &semantic, &result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( result, HIL_TRANSPORT_MVP_HANDSHAKE_FRAME_INCOMPATIBLE );
}

TEST( TransportHandshakeProcess, EveryValidModeRecordsAndSchedulesTheSameHandshakeWork )
{
    constexpr std::array<HIL_Transport_Operating_Mode_T, 3> modes{
        HIL_TRANSPORT_OPERATING_MODE_NORMAL,
        HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER,
        HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME,
    };

    for ( const auto mode : modes )
    {
        Harness host;
        host.Initialize( HIL_TRANSPORT_ROLE_HOST, 100u, 9u );
        auto context = Context( host );
        ASSERT_EQ( HIL_TRANSPORT_Process( &context, 50u, mode ), HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( host.root.base.operating_mode, mode );
        EXPECT_EQ( host.root.base.operating_mode_valid, 1u );
        EXPECT_EQ( host.root.session.handshake_phase,
                   HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE );
        EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_READY );
    }
}

TEST( TransportHandshakeProcess, InvalidModeDoesNotReplaceModeOrProgressPendingOrRetryWork )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 10u, 3u );
    auto context = Context( host );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 0u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 100u, &initiate ) );
    const auto                     before = host.root.session;
    HIL_Transport_Operating_Mode_T invalid_mode;
    std::memset( &invalid_mode, 0x7F, sizeof( invalid_mode ) );

    EXPECT_EQ( HIL_TRANSPORT_Process( &context, 1000u, invalid_mode ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( host.root.base.operating_mode, HIL_TRANSPORT_OPERATING_MODE_NORMAL );
    EXPECT_EQ( host.root.base.operating_mode_valid, 1u );
    EXPECT_EQ( host.root.session.reliable_state, before.reliable_state );
    EXPECT_EQ( host.root.session.retransmissions_committed, before.retransmissions_committed );
    EXPECT_EQ( host.root.session.reliable_last_committed_ms, before.reliable_last_committed_ms );
}

TEST( TransportHandshakeProcess, DisconnectedRecordsModeWithoutProgressAndFaultStopsProgress )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 15u, 2u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Notify_Link_State( &host.root,
                                                            HIL_TRANSPORT_LINK_STATE_DISCONNECTED ),
               HIL_TRANSPORT_STATUS_OK );
    auto context = Context( host );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.base.operating_mode, HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER );
    EXPECT_EQ( host.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );

    host.root.base.session_state   = HIL_TRANSPORT_SESSION_STATE_FAULT;
    host.root.session.state        = HIL_TRANSPORT_SESSION_STATE_FAULT;
    host.root.base.last_failure    = HIL_TRANSPORT_FAILURE_INTERNAL;
    host.root.session.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
    EXPECT_EQ( HIL_TRANSPORT_Process( &context, 2u, HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME ),
               HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    EXPECT_EQ( host.root.base.operating_mode, HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME );
    EXPECT_EQ( host.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE );
}

TEST( TransportHandshakeProcess, RecoveringWaitsForOldControlCommitThenStartsFreshAttempt )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 20u, 1u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Abandon( &host.root, HIL_TRANSPORT_FAILURE_PROTOCOL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Publish_Reset( &host.root, 20u ),
               HIL_TRANSPORT_STATUS_OK );
    auto context = Context( host );

    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 1u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE );
    HIL_Transport_Mvp_Frame_T reset{};
    ASSERT_TRUE( PeekDecodeCommit( host, 0u, &reset ) );

    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 2u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( host.root.session.session_identifier, 21u );
    EXPECT_EQ( host.root.session.next_host_session_identifier, 22u );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_READY );
}

TEST( TransportHandshakeProcess, TimeoutRetriesEveryReliableHandshakeTypeWithExactBytes )
{
    constexpr std::array<HIL_Transport_Mvp_Frame_Type_T, 3> types{
        HIL_TRANSPORT_MVP_FRAME_INITIATE,
        HIL_TRANSPORT_MVP_FRAME_RESPONSE,
        HIL_TRANSPORT_MVP_FRAME_CONFIRM,
    };

    for ( const auto type : types )
    {
        Harness                                    harness;
        HIL_Transport_Mvp_Handshake_Frame_Result_T result;
        if ( type == HIL_TRANSPORT_MVP_FRAME_RESPONSE )
        {
            harness.Initialize( HIL_TRANSPORT_ROLE_RIG, 0u, 40u );
            const auto initiate = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 60u, 7u, 0u );
            ASSERT_EQ(
                HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &harness.root, &initiate, &result ),
                HIL_TRANSPORT_STATUS_OK );
        }
        else
        {
            harness.Initialize( HIL_TRANSPORT_ROLE_HOST, 60u, 40u );
            if ( type == HIL_TRANSPORT_MVP_FRAME_CONFIRM )
            {
                ASSERT_EQ( HIL_TRANSPORT_MVP_Handshake_Process( &harness.root, 0u ),
                           HIL_TRANSPORT_STATUS_OK );
                HIL_Transport_Mvp_Frame_T initiate{};
                ASSERT_TRUE( PeekDecodeCommit( harness, 10u, &initiate ) );
                const auto response = EmptyFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 60u, 7u, 40u );
                ASSERT_EQ(
                    HIL_TRANSPORT_MVP_Handshake_Handle_Frame( &harness.root, &response, &result ),
                    HIL_TRANSPORT_STATUS_OK );
            }
        }

        auto context = Context( harness );
        ASSERT_EQ( HIL_TRANSPORT_Process( &context, 0u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( harness.root.session.retained_reliable_frame_type, type );
        const auto                retained = harness.encoded;
        HIL_Transport_Mvp_Frame_T published{};
        ASSERT_TRUE( PeekDecodeCommit( harness, 100u, &published ) );
        EXPECT_EQ( published.type, type );
        ASSERT_EQ( HIL_TRANSPORT_Process( &context, 109u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
        ASSERT_EQ( HIL_TRANSPORT_Process( &context, 110u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( harness.root.session.reliable_state,
                   HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
        EXPECT_EQ( harness.root.session.retransmissions_committed, 0u );
        EXPECT_EQ( harness.encoded, retained );
        HIL_Transport_Mvp_Frame_T retry{};
        ASSERT_TRUE( PeekDecodeCommit( harness, 120u, &retry ) );
        EXPECT_EQ( retry.type, type );
        EXPECT_EQ( retry.session_identifier, published.session_identifier );
        EXPECT_EQ( retry.sequence, published.sequence );
        EXPECT_EQ( retry.acknowledgement_sequence, published.acknowledgement_sequence );
        EXPECT_EQ( harness.root.session.retransmissions_committed, 1u );
    }
}

TEST( TransportHandshakeProcess,
      ExhaustionRecoversWithoutApplicationFailureEventAndUsesNewIdentity )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 70u, 8u );
    host.root.base.config.max_retries = 0u;
    auto context                      = Context( host );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 0u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 100u, &initiate ) );

    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 110u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.session.state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.base.last_failure, HIL_TRANSPORT_FAILURE_DELIVERY );
    EXPECT_EQ( host.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( host.root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE );
    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &host.root, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_SESSION_RESET );
    EXPECT_NE( event.type, HIL_TRANSPORT_EVENT_DELIVERY_FAILED );

    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 111u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( host.root.session.session_identifier, 71u );
    EXPECT_EQ( host.root.session.next_host_session_identifier, 72u );
    EXPECT_EQ( host.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_RESPONSE );
}

TEST( TransportHandshakeProcess, ExhaustionPreservesFullEventFifoAndReportsCapacity )
{
    Harness host;
    host.Initialize( HIL_TRANSPORT_ROLE_HOST, 80u, 1u );
    host.root.base.config.max_retries = 0u;
    auto context                      = Context( host );
    ASSERT_EQ( HIL_TRANSPORT_Process( &context, 0u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( host, 100u, &initiate ) );
    const HIL_Transport_Event_T event{ HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,
                                       HIL_TRANSPORT_STATUS_NOT_READY,
                                       HIL_TRANSPORT_FAILURE_PROTOCOL, 0u };
    for ( std::size_t index = 0u; index < HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY; ++index )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &host.root, &event ),
                   HIL_TRANSPORT_STATUS_OK );
    }

    EXPECT_EQ( HIL_TRANSPORT_Process( &context, 110u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( host.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( host.root.event_count, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY );
}

TEST( TransportHandshakeProcess, ZeroTimeoutDisablesRetryAndTimerWrapUsesUnsignedElapsedTime )
{
    Harness disabled;
    disabled.Initialize( HIL_TRANSPORT_ROLE_HOST, 90u, 1u );
    disabled.root.base.config.retransmit_timeout_ms = 0u;
    auto disabled_context                           = Context( disabled );
    ASSERT_EQ( HIL_TRANSPORT_Process( &disabled_context, 0u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Mvp_Frame_T initiate{};
    ASSERT_TRUE( PeekDecodeCommit( disabled, 20u, &initiate ) );
    ASSERT_EQ(
        HIL_TRANSPORT_Process( &disabled_context, UINT32_MAX, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( disabled.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );

    Harness wrapped;
    wrapped.Initialize( HIL_TRANSPORT_ROLE_HOST, 91u, 1u );
    auto wrapped_context = Context( wrapped );
    ASSERT_EQ( HIL_TRANSPORT_Process( &wrapped_context, 0u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_TRUE( PeekDecodeCommit( wrapped, UINT32_MAX - 4u, &initiate ) );
    ASSERT_EQ( HIL_TRANSPORT_Process( &wrapped_context, 4u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( wrapped.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    ASSERT_EQ( HIL_TRANSPORT_Process( &wrapped_context, 5u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( wrapped.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
}

}  // namespace
