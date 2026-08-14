#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "hil_rig_protocol/transport/transport.h"
#include "transport/internal/mvp/transport_events_mvp.h"
#include "transport/internal/mvp/transport_reliability_mvp.h"
#include "transport/internal/mvp/transport_session_mvp.h"

namespace {

constexpr std::size_t   RetainedCapacity        = 16u;
constexpr std::size_t   WorkspaceRegionCapacity = 8u;
constexpr std::size_t   FrameSize               = 11u;
constexpr std::uint16_t InitialSequence         = 0x3456u;
constexpr std::uint32_t TimeoutMs               = 10u;

struct ReliableSnapshot
{
    HIL_Transport_Mvp_Reliable_State_T         state;
    HIL_Transport_Mvp_Frame_Type_T             frame_type;
    std::uint16_t                              next_sequence;
    std::uint16_t                              retained_sequence;
    std::uint8_t                               retransmissions_committed;
    std::uint32_t                              last_committed_ms;
    std::size_t                                encoded_size;
    HIL_Transport_Mvp_Output_Selection_T       selection;
    std::array<std::uint8_t, RetainedCapacity> bytes;

    bool operator==( const ReliableSnapshot& other ) const
    {
        return state == other.state && frame_type == other.frame_type
               && next_sequence == other.next_sequence
               && retained_sequence == other.retained_sequence
               && retransmissions_committed == other.retransmissions_committed
               && last_committed_ms == other.last_committed_ms && encoded_size == other.encoded_size
               && selection == other.selection && bytes == other.bytes;
    }
};

void ExpectConfigEqual( const HIL_Transport_Config_T& actual,
                        const HIL_Transport_Config_T& expected )
{
    EXPECT_EQ( actual.max_application_message_size, expected.max_application_message_size );
    EXPECT_EQ( actual.max_encoded_frame_size, expected.max_encoded_frame_size );
    EXPECT_EQ( actual.session_seed, expected.session_seed );
    EXPECT_EQ( actual.initial_reliable_sequence, expected.initial_reliable_sequence );
    EXPECT_EQ( actual.connection_timeout_ms, expected.connection_timeout_ms );
    EXPECT_EQ( actual.retransmit_timeout_ms, expected.retransmit_timeout_ms );
    EXPECT_EQ( actual.max_retries, expected.max_retries );
}

class TransportReliabilityTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        for ( std::size_t index = 0u; index < retained_.size(); ++index )
        {
            retained_[index] = static_cast<std::uint8_t>( 0x90u + index );
        }
        /* Zeros and arbitrary values demonstrate that reliability treats bytes as opaque. */
        retained_[2] = 0u;
        retained_[7] = 0u;

        submitted_.fill( 0xA1u );
        parser_storage_.fill( 0xB2u );
        codec_storage_.fill( 0xC3u );
        received_.fill( 0xD4u );

        root_.base.config.max_application_message_size = submitted_.size();
        root_.base.config.max_encoded_frame_size       = retained_.size();
        root_.base.config.session_seed                 = UINT64_C( 0x123456789ABCDEF0 );
        root_.base.config.initial_reliable_sequence    = InitialSequence;
        root_.base.config.retransmit_timeout_ms        = TimeoutMs;
        root_.base.config.max_retries                  = 3u;
        root_.base.role                                = HIL_TRANSPORT_ROLE_HOST;
        root_.base.link_state                          = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
        root_.base.session_state                       = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.base.operating_mode                      = HIL_TRANSPORT_OPERATING_MODE_NORMAL;
        root_.session.role                             = HIL_TRANSPORT_ROLE_HOST;
        root_.session.link_state                       = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
        root_.session.state                            = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.session.handshake_phase                  = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE;
        root_.session.next_host_session_identifier     = root_.base.config.session_seed;
        root_.session.initial_reliable_sequence        = InitialSequence;
        root_.session.next_transmit_sequence           = InitialSequence;
        root_.submitted_message                        = submitted_.data();
        root_.encoded_output                           = retained_.data();
        root_.encoded_output_capacity                  = retained_.size();
        root_.parser.scratch_buffer                    = parser_storage_.data();
        root_.parser.scratch_buffer_size               = parser_storage_.size();
        root_.codec_scratch                            = codec_storage_.data();
        root_.codec_scratch_size                       = codec_storage_.size();
        root_.received_message                         = received_.data();
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
        original_bytes_ = retained_;
    }

    void Publish(
        HIL_Transport_Mvp_Frame_Type_T frame_type = HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Publish_Encoded(
                       &root_, frame_type, root_.session.next_transmit_sequence, FrameSize ),
                   HIL_TRANSPORT_STATUS_OK );
    }

    void Peek()
    {
        std::array<std::uint8_t, RetainedCapacity> output{};
        std::size_t                                output_size = 0u;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, output.data(), output.size(),
                                                              &output_size ),
                   HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( output_size, FrameSize );
    }

    void Commit( std::uint32_t now_ms = 100u )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Commit_Output( &root_, now_ms ),
                   HIL_TRANSPORT_STATUS_OK );
    }

    void PrepareAwaiting( std::uint32_t now_ms = 100u )
    {
        Publish();
        Peek();
        Commit( now_ms );
    }

    void ScheduleRetry( std::uint32_t now_ms = 110u )
    {
        HIL_Transport_Mvp_Reliability_Outcome_T outcome = HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, now_ms, &outcome ),
                   HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );
    }

    void SetValidState( HIL_Transport_Mvp_Reliable_State_T state )
    {
        root_.base.config.max_retries = 3u;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
        if ( state == HIL_TRANSPORT_MVP_RELIABLE_IDLE )
        {
            return;
        }
        Publish();
        if ( state == HIL_TRANSPORT_MVP_RELIABLE_READY )
        {
            return;
        }
        Peek();
        if ( state == HIL_TRANSPORT_MVP_RELIABLE_PEEKED )
        {
            return;
        }
        Commit();
        if ( state == HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK )
        {
            return;
        }
        if ( state == HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED )
        {
            HIL_Transport_Mvp_Reliability_Outcome_T outcome;
            root_.base.config.max_retries = 0u;
            ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 110u, &outcome ),
                       HIL_TRANSPORT_STATUS_OK );
            ASSERT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED );
            return;
        }
        ScheduleRetry();
        if ( state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY )
        {
            return;
        }
        ASSERT_EQ( state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED );
        Peek();
    }

    void PopulateSessionScopedState()
    {
        constexpr std::array<std::uint8_t, 4> partial_body{ 1u, 2u, 3u, 4u };
        std::size_t                           consumed = 0u;

        SetValidState( HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED );
        ASSERT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &root_.parser, partial_body.data(),
                                                    partial_body.size(), &consumed ),
                   HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA );
        ASSERT_EQ( consumed, partial_body.size() );
        root_.submitted_message_size    = 5u;
        root_.submitted_message_pending = 1u;
        root_.received_message_size     = 6u;
        root_.received_message_pending  = 1u;
        const HIL_Transport_Event_T event{ HIL_TRANSPORT_EVENT_DELIVERY_FAILED,
                                           HIL_TRANSPORT_STATUS_DELIVERY_FAILED,
                                           HIL_TRANSPORT_FAILURE_DELIVERY, 7u };
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &event ), HIL_TRANSPORT_STATUS_OK );
        root_.session.session_identifier        = UINT64_C( 0x0123456789ABCDEF );
        root_.session.session_identifier_valid  = 1u;
        root_.session.handshake_phase           = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED;
        root_.session.expected_receive_sequence = 0x7788u;
        root_.session.last_accepted_receive_sequence  = 0x6677u;
        root_.session.accepted_receive_sequence_valid = 1u;
        root_.session.last_valid_receive_ms           = UINT32_C( 0xAABBCCDD );
        root_.session.last_failure                    = HIL_TRANSPORT_FAILURE_INTERNAL;
        root_.base.last_failure                       = HIL_TRANSPORT_FAILURE_INTERNAL;
    }

    ReliableSnapshot Snapshot() const
    {
        return ReliableSnapshot{
            root_.session.reliable_state,
            root_.session.retained_reliable_frame_type,
            root_.session.next_transmit_sequence,
            root_.session.retained_transmit_sequence,
            root_.session.retransmissions_committed,
            root_.session.reliable_last_committed_ms,
            root_.encoded_output_size,
            root_.output_selection,
            retained_,
        };
    }

    HIL_Transport_Context_T Context()
    {
        return HIL_Transport_Context_T{ &root_, sizeof( root_ ),
                                        HIL_TRANSPORT_INTERNAL_INITIALIZATION_COOKIE };
    }

    HIL_Transport_Mvp_Root_T                          root_{};
    std::array<std::uint8_t, RetainedCapacity>        retained_{};
    std::array<std::uint8_t, RetainedCapacity>        original_bytes_{};
    std::array<std::uint8_t, WorkspaceRegionCapacity> submitted_{};
    std::array<std::uint8_t, WorkspaceRegionCapacity> parser_storage_{};
    std::array<std::uint8_t, WorkspaceRegionCapacity> codec_storage_{};
    std::array<std::uint8_t, WorkspaceRegionCapacity> received_{};
};

TEST_F( TransportReliabilityTest, StartsIdleAndSequenceReservationDoesNotPublishOrConsume )
{
    std::uint16_t sequence = 0u;

    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( root_.encoded_output_size, 0u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Reserve_Sequence( &root_.session, &sequence ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( sequence, InitialSequence );
    EXPECT_EQ( root_.session.next_transmit_sequence, InitialSequence );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
}

TEST_F( TransportReliabilityTest, ValidPublicationRetainsMetadataWithoutCopyingOrAdvancing )
{
    Publish( HIL_TRANSPORT_MVP_FRAME_CONFIRM );

    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_READY );
    EXPECT_EQ( root_.session.retained_reliable_frame_type, HIL_TRANSPORT_MVP_FRAME_CONFIRM );
    EXPECT_EQ( root_.session.retained_transmit_sequence, InitialSequence );
    EXPECT_EQ( root_.session.next_transmit_sequence, InitialSequence );
    EXPECT_EQ( root_.encoded_output_size, FrameSize );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 0u );
    EXPECT_EQ( retained_, original_bytes_ );
}

TEST_F( TransportReliabilityTest, InvalidPublicationIsRejectedWithoutSequenceOrByteMutation )
{
    const auto expect_idle_unchanged = [this]() {
        EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
        EXPECT_EQ( root_.session.next_transmit_sequence, InitialSequence );
        EXPECT_EQ( root_.encoded_output_size, 0u );
        EXPECT_EQ( retained_, original_bytes_ );
    };

    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Publish_Encoded(
                   nullptr, HIL_TRANSPORT_MVP_FRAME_INITIATE, InitialSequence, FrameSize ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Publish_Encoded(
                   &root_, HIL_TRANSPORT_MVP_FRAME_INITIATE, InitialSequence, 0u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    expect_idle_unchanged();
    EXPECT_EQ(
        HIL_TRANSPORT_MVP_Reliability_Publish_Encoded( &root_, HIL_TRANSPORT_MVP_FRAME_INITIATE,
                                                       InitialSequence, retained_.size() + 1u ),
        HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    expect_idle_unchanged();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Publish_Encoded( &root_, HIL_TRANSPORT_MVP_FRAME_ACK,
                                                              InitialSequence, FrameSize ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    expect_idle_unchanged();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Publish_Encoded( &root_, HIL_TRANSPORT_MVP_FRAME_RESET,
                                                              InitialSequence, FrameSize ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    expect_idle_unchanged();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Publish_Encoded(
                   &root_, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE,
                   static_cast<std::uint16_t>( InitialSequence + 1u ), FrameSize ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    expect_idle_unchanged();
}

TEST_F( TransportReliabilityTest, SecondPublicationCannotReplaceAnyOwnedLifecycleState )
{
    constexpr std::array<HIL_Transport_Mvp_Reliable_State_T, 6> OwnedStates{
        HIL_TRANSPORT_MVP_RELIABLE_READY,
        HIL_TRANSPORT_MVP_RELIABLE_PEEKED,
        HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK,
        HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY,
        HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED,
        HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED,
    };

    for ( const auto state : OwnedStates )
    {
        SCOPED_TRACE( static_cast<int>( state ) );
        SetValidState( state );
        const ReliableSnapshot before = Snapshot();
        EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Publish_Encoded(
                       &root_, HIL_TRANSPORT_MVP_FRAME_INITIATE,
                       root_.session.next_transmit_sequence, FrameSize ),
                   HIL_TRANSPORT_STATUS_NOT_READY );
        EXPECT_TRUE( Snapshot() == before );

        std::uint16_t sequence = 0u;
        EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Reserve_Sequence( &root_.session, &sequence ),
                   HIL_TRANSPORT_STATUS_NOT_READY );
        EXPECT_EQ( sequence, 0u );
        EXPECT_TRUE( Snapshot() == before );
    }
}

TEST_F( TransportReliabilityTest, SizeQueriesAndSmallBuffersNeverPinOrPartiallyCopy )
{
    Publish();
    const ReliableSnapshot before      = Snapshot();
    std::size_t            output_size = 99u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, nullptr, 0u, &output_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, FrameSize );
    EXPECT_TRUE( Snapshot() == before );

    std::array<std::uint8_t, FrameSize - 1u> small{};
    small.fill( 0xEEu );
    const auto untouched = small;
    output_size          = 0u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, small.data(), small.size(),
                                                          &output_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, FrameSize );
    EXPECT_EQ( small, untouched );
    EXPECT_TRUE( Snapshot() == before );
}

TEST_F( TransportReliabilityTest, ExactLargerAndRepeatedPeeksReturnStableIndependentCopies )
{
    Publish();
    std::array<std::uint8_t, FrameSize> exact{};
    std::size_t                         output_size = 0u;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, exact.data(), exact.size(),
                                                          &output_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output_size, FrameSize );
    EXPECT_TRUE( std::equal( exact.begin(), exact.end(), original_bytes_.begin() ) );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_PEEKED );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );

    exact.fill( 0x11u );
    std::array<std::uint8_t, RetainedCapacity + 4u> larger{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, larger.data(), larger.size(),
                                                          &output_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output_size, FrameSize );
    EXPECT_TRUE(
        std::equal( larger.begin(), larger.begin() + FrameSize, original_bytes_.begin() ) );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 0u );
    EXPECT_EQ( root_.session.next_transmit_sequence, InitialSequence );
}

TEST_F( TransportReliabilityTest, PeekRejectsInvalidArgumentsWithoutLifecycleMutation )
{
    Publish();
    const ReliableSnapshot before      = Snapshot();
    std::size_t            output_size = 77u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, nullptr, 1u, &output_size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( output_size, 0u );
    EXPECT_TRUE( Snapshot() == before );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, nullptr, 0u, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_TRUE( Snapshot() == before );
}

TEST_F( TransportReliabilityTest, InitialCommitBeginsTimingAndCanOnlyOccurOncePerPeek )
{
    Publish();
    const ReliableSnapshot ready = Snapshot();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Commit_Output( &root_, 70u ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_TRUE( Snapshot() == ready );

    Peek();
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Commit_Output( &root_, 123456u ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 123456u );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
    EXPECT_EQ( root_.encoded_output_size, FrameSize );
    EXPECT_EQ( retained_, original_bytes_ );

    const ReliableSnapshot awaiting = Snapshot();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Commit_Output( &root_, 123457u ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_TRUE( Snapshot() == awaiting );

    std::array<std::uint8_t, RetainedCapacity> output{};
    std::size_t                                output_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, output.data(), output.size(),
                                                          &output_size ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( output_size, 0u );
}

TEST_F( TransportReliabilityTest, PrivateLifecycleDoesNotOwnGlobalOutputSelection )
{
    root_.output_selection = HIL_TRANSPORT_MVP_OUTPUT_CONTROL;

    Publish();
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
    Peek();
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
    Commit();
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );

    HIL_Transport_Mvp_Frame_Type_T          completed;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                     &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
}

TEST_F( TransportReliabilityTest, CommitRejectsEveryValidStateWithoutPeekedSelection )
{
    constexpr std::array<HIL_Transport_Mvp_Reliable_State_T, 5> NonPeekedStates{
        HIL_TRANSPORT_MVP_RELIABLE_IDLE,         HIL_TRANSPORT_MVP_RELIABLE_READY,
        HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY,
        HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED,
    };

    for ( const auto state : NonPeekedStates )
    {
        SCOPED_TRACE( static_cast<int>( state ) );
        SetValidState( state );
        const ReliableSnapshot before = Snapshot();
        EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Commit_Output( &root_, 999u ),
                   HIL_TRANSPORT_STATUS_NOT_READY );
        EXPECT_TRUE( Snapshot() == before );
    }
}

TEST_F( TransportReliabilityTest, MatchingAckCompletesOnceAndReturnsFrameType )
{
    PrepareAwaiting();
    HIL_Transport_Mvp_Frame_Type_T          completed = HIL_TRANSPORT_MVP_FRAME_INVALID;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome   = HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                     &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED );
    EXPECT_EQ( completed, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    EXPECT_EQ( root_.session.next_transmit_sequence,
               static_cast<std::uint16_t>( InitialSequence + 1u ) );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( root_.encoded_output_size, 0u );
    EXPECT_EQ( root_.session.retained_reliable_frame_type, HIL_TRANSPORT_MVP_FRAME_INVALID );
    EXPECT_EQ( root_.session.retained_transmit_sequence, 0u );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 0u );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
    EXPECT_EQ( retained_, original_bytes_ );

    completed = HIL_TRANSPORT_MVP_FRAME_CONFIRM;
    outcome   = HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                     &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    EXPECT_EQ( completed, HIL_TRANSPORT_MVP_FRAME_INVALID );
    EXPECT_EQ( root_.session.next_transmit_sequence,
               static_cast<std::uint16_t>( InitialSequence + 1u ) );
}

TEST_F( TransportReliabilityTest, WrongAndOutOfStateAcksNeverMutateLifecycle )
{
    PrepareAwaiting();
    HIL_Transport_Mvp_Frame_Type_T          completed;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    ReliableSnapshot                        before = Snapshot();

    EXPECT_EQ(
        HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement(
            &root_, static_cast<std::uint16_t>( InitialSequence - 1u ), &completed, &outcome ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    EXPECT_TRUE( Snapshot() == before );

    constexpr std::array<HIL_Transport_Mvp_Reliable_State_T, 5> NonMatchingStates{
        HIL_TRANSPORT_MVP_RELIABLE_IDLE,      HIL_TRANSPORT_MVP_RELIABLE_READY,
        HIL_TRANSPORT_MVP_RELIABLE_PEEKED,    HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED,
        HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED,
    };
    for ( const auto state : NonMatchingStates )
    {
        SCOPED_TRACE( static_cast<int>( state ) );
        SetValidState( state );
        before = Snapshot();
        EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                         &completed, &outcome ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
        EXPECT_EQ( completed, HIL_TRANSPORT_MVP_FRAME_INVALID );
        EXPECT_TRUE( Snapshot() == before );
    }
}

TEST_F( TransportReliabilityTest, SessionAckClassificationMatchesOnlyCommittedUnpinnedStates )
{
    struct ClassificationCase
    {
        HIL_Transport_Mvp_Reliable_State_T state;
        HIL_Transport_Mvp_Ack_Result_T     expected;
    };
    constexpr std::array<ClassificationCase, 7> Cases{
        ClassificationCase{ HIL_TRANSPORT_MVP_RELIABLE_IDLE,
                            HIL_TRANSPORT_MVP_ACK_STALE_OR_UNEXPECTED },
        ClassificationCase{ HIL_TRANSPORT_MVP_RELIABLE_READY,
                            HIL_TRANSPORT_MVP_ACK_STALE_OR_UNEXPECTED },
        ClassificationCase{ HIL_TRANSPORT_MVP_RELIABLE_PEEKED,
                            HIL_TRANSPORT_MVP_ACK_STALE_OR_UNEXPECTED },
        ClassificationCase{ HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK,
                            HIL_TRANSPORT_MVP_ACK_MATCHED },
        ClassificationCase{ HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY,
                            HIL_TRANSPORT_MVP_ACK_MATCHED },
        ClassificationCase{ HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED,
                            HIL_TRANSPORT_MVP_ACK_STALE_OR_UNEXPECTED },
        ClassificationCase{ HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED,
                            HIL_TRANSPORT_MVP_ACK_STALE_OR_UNEXPECTED },
    };

    for ( const auto& test_case : Cases )
    {
        SCOPED_TRACE( static_cast<int>( test_case.state ) );
        SetValidState( test_case.state );
        const ReliableSnapshot         before          = Snapshot();
        const std::uint16_t            acknowledgement = root_.session.retained_transmit_sequence;
        HIL_Transport_Mvp_Ack_Result_T result          = HIL_TRANSPORT_MVP_ACK_MATCHED;

        ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Classify_Acknowledgement( &root_.session,
                                                                       acknowledgement, &result ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( result, test_case.expected );
        EXPECT_TRUE( Snapshot() == before );
    }
}

TEST_F( TransportReliabilityTest, SequenceWrapsNaturallyAfterExactAck )
{
    root_.session.next_transmit_sequence = std::numeric_limits<std::uint16_t>::max();
    PrepareAwaiting();
    HIL_Transport_Mvp_Frame_Type_T          completed;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement(
                   &root_, std::numeric_limits<std::uint16_t>::max(), &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED );
    EXPECT_EQ( root_.session.next_transmit_sequence, 0u );
}

TEST_F( TransportReliabilityTest, TimeoutUsesCommitBoundaryAndExactUnsignedDeadline )
{
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    Publish();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 100000u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    Peek();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 100000u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    Commit( 500u );

    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 509u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 510u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );

    SetValidState( HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 111u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );
}

TEST_F( TransportReliabilityTest, ZeroTimeoutDisablesProgressionAndUint32WrapIsSupported )
{
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    PrepareAwaiting( 20u );
    root_.base.config.retransmit_timeout_ms = 0u;
    const ReliableSnapshot disabled         = Snapshot();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending(
                   &root_, std::numeric_limits<std::uint32_t>::max(), &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    EXPECT_TRUE( Snapshot() == disabled );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    root_.base.config.retransmit_timeout_ms = TimeoutMs;
    PrepareAwaiting( std::numeric_limits<std::uint32_t>::max() - 4u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 4u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 5u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );
}

TEST_F( TransportReliabilityTest, DuplicateRequestMakesAwaitingBytesRetryableWithoutSpendingRetry )
{
    PrepareAwaiting();
    root_.output_selection = HIL_TRANSPORT_MVP_OUTPUT_CONTROL;
    const auto before      = Snapshot();
    HIL_Transport_Mvp_Reliability_Outcome_T outcome = HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Request_Retransmission(
                   &root_, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
    EXPECT_EQ( root_.session.retransmissions_committed, before.retransmissions_committed );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, before.last_committed_ms );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
    EXPECT_EQ( retained_, before.bytes );

    Peek();
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );
    Commit( 200u );
    EXPECT_EQ( root_.session.retransmissions_committed, 1u );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 200u );
}

TEST_F( TransportReliabilityTest, DuplicateRequestDoesNotDisturbAlreadyAvailableOrPinnedBytes )
{
    constexpr std::array<HIL_Transport_Mvp_Reliable_State_T, 4> states{
        HIL_TRANSPORT_MVP_RELIABLE_READY,
        HIL_TRANSPORT_MVP_RELIABLE_PEEKED,
        HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY,
        HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED,
    };

    for ( const auto state : states )
    {
        SetValidState( state );
        root_.output_selection = ( state == HIL_TRANSPORT_MVP_RELIABLE_PEEKED
                                   || state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED )
                                     ? HIL_TRANSPORT_MVP_OUTPUT_RELIABLE
                                     : HIL_TRANSPORT_MVP_OUTPUT_CONTROL;
        const auto before = Snapshot();
        HIL_Transport_Mvp_Reliability_Outcome_T outcome =
            HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Request_Retransmission(
                       &root_, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, &outcome ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
        EXPECT_TRUE( Snapshot() == before );
    }
}

TEST_F( TransportReliabilityTest, DuplicateRequestExhaustsWhenNoRetryAllowanceRemains )
{
    root_.base.config.max_retries = 0u;
    PrepareAwaiting();
    HIL_Transport_Mvp_Reliability_Outcome_T outcome = HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Request_Retransmission(
                   &root_, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );

    outcome = HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Request_Retransmission(
                   &root_, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
}

TEST_F( TransportReliabilityTest, DuplicateRequestValidatesTypeAndTreatsIdleAsNoWork )
{
    HIL_Transport_Mvp_Reliability_Outcome_T outcome =
        HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Request_Retransmission(
                   &root_, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Request_Retransmission(
                   &root_, HIL_TRANSPORT_MVP_FRAME_ACK, &outcome ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Request_Retransmission(
                   &root_, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    Publish( HIL_TRANSPORT_MVP_FRAME_CONFIRM );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Request_Retransmission(
                   &root_, HIL_TRANSPORT_MVP_FRAME_RESPONSE, &outcome ),
               HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    EXPECT_EQ( root_.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT );
    EXPECT_EQ( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
}

TEST_F( TransportReliabilityTest, RetransmissionReusesBytesAndCountsOnlyItsCommit )
{
    PrepareAwaiting( 100u );
    ScheduleRetry( 110u );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 100u );

    std::size_t output_size = 0u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, nullptr, 0u, &output_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, FrameSize );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );

    std::array<std::uint8_t, RetainedCapacity> retry_one{};
    std::array<std::uint8_t, RetainedCapacity> retry_two{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, retry_one.data(),
                                                          retry_one.size(), &output_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, retry_two.data(),
                                                          retry_two.size(), &output_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_TRUE(
        std::equal( retry_one.begin(), retry_one.begin() + FrameSize, original_bytes_.begin() ) );
    EXPECT_TRUE(
        std::equal( retry_one.begin(), retry_one.begin() + FrameSize, retry_two.begin() ) );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Commit_Output( &root_, 777u ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root_.session.retransmissions_committed, 1u );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 777u );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( retained_, original_bytes_ );

    HIL_Transport_Mvp_Frame_Type_T          completed;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                     &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED );
}

TEST_F( TransportReliabilityTest, MatchingLateAckCancelsUnpinnedRetry )
{
    PrepareAwaiting( 100u );
    ScheduleRetry( 110u );
    ASSERT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
    HIL_Transport_Mvp_Frame_Type_T          completed = HIL_TRANSPORT_MVP_FRAME_INVALID;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome   = HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                     &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED );
    EXPECT_EQ( completed, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( root_.session.next_transmit_sequence,
               static_cast<std::uint16_t>( InitialSequence + 1u ) );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );
    EXPECT_EQ( root_.encoded_output_size, 0u );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
    EXPECT_EQ( root_.session.retained_reliable_frame_type, HIL_TRANSPORT_MVP_FRAME_INVALID );
    EXPECT_EQ( root_.session.retained_transmit_sequence, 0u );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 0u );
    EXPECT_EQ( retained_, original_bytes_ );
    EXPECT_EQ( root_.event_count, 0u );
    EXPECT_EQ( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( root_.session.state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
}

TEST_F( TransportReliabilityTest, WrongLateAckLeavesRetryReadyUnchanged )
{
    PrepareAwaiting();
    ScheduleRetry();
    const ReliableSnapshot                  before    = Snapshot();
    HIL_Transport_Mvp_Frame_Type_T          completed = HIL_TRANSPORT_MVP_FRAME_CONFIRM;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome   = HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED;

    EXPECT_EQ(
        HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement(
            &root_, static_cast<std::uint16_t>( InitialSequence + 1u ), &completed, &outcome ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    EXPECT_EQ( completed, HIL_TRANSPORT_MVP_FRAME_INVALID );
    EXPECT_TRUE( Snapshot() == before );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );

    std::size_t required_size = 0u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, nullptr, 0u, &required_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( required_size, FrameSize );
    EXPECT_TRUE( Snapshot() == before );
}

TEST_F( TransportReliabilityTest, MatchingAckIsIgnoredWhileRetryOutputIsPinned )
{
    PrepareAwaiting();
    ScheduleRetry();

    /* A successful retry peek pins external output until commit or reset. */
    Peek();
    const ReliableSnapshot                  before = Snapshot();
    HIL_Transport_Mvp_Frame_Type_T          completed;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                     &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    EXPECT_EQ( completed, HIL_TRANSPORT_MVP_FRAME_INVALID );
    EXPECT_TRUE( Snapshot() == before );

    Commit( 200u );
    EXPECT_EQ( root_.session.retransmissions_committed, 1u );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                     &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED );
}

TEST_F( TransportReliabilityTest, MatchingLateAckWrapsSequenceExactlyOnce )
{
    root_.session.next_transmit_sequence = std::numeric_limits<std::uint16_t>::max();
    PrepareAwaiting();
    ScheduleRetry();
    HIL_Transport_Mvp_Frame_Type_T          completed;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement(
                   &root_, std::numeric_limits<std::uint16_t>::max(), &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED );
    EXPECT_EQ( root_.session.next_transmit_sequence, 0u );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement(
                   &root_, std::numeric_limits<std::uint16_t>::max(), &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    EXPECT_EQ( root_.session.next_transmit_sequence, 0u );
}

TEST_F( TransportReliabilityTest, ConfiguredRetryLimitsExposeExactlyPermittedCommits )
{
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;

    root_.base.config.max_retries = 0u;
    PrepareAwaiting();
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 110u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    root_.base.config.max_retries = 1u;
    PrepareAwaiting();
    ScheduleRetry();
    Peek();
    Commit( 120u );
    EXPECT_EQ( root_.session.retransmissions_committed, 1u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 130u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    root_.base.config.max_retries = 3u;
    PrepareAwaiting( 0u );
    for ( std::uint8_t retry = 1u; retry <= 3u; ++retry )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending(
                       &root_, static_cast<std::uint32_t>( retry ) * TimeoutMs, &outcome ),
                   HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );
        Peek();
        Commit( static_cast<std::uint32_t>( retry ) * TimeoutMs );
        EXPECT_EQ( root_.session.retransmissions_committed, retry );
    }
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 40u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED );
}

TEST_F( TransportReliabilityTest, Uint8MaximumRetryCounterCommitsWithoutOverflow )
{
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    root_.base.config.max_retries = std::numeric_limits<std::uint8_t>::max();
    PrepareAwaiting( 100u );
    root_.session.retransmissions_committed =
        static_cast<std::uint8_t>( std::numeric_limits<std::uint8_t>::max() - 1u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 110u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );
    Peek();
    Commit( 200u );
    EXPECT_EQ( root_.session.retransmissions_committed, std::numeric_limits<std::uint8_t>::max() );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 210u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED );
}

TEST_F( TransportReliabilityTest, ExhaustionPreservesOwnershipButExposesNoOutputOrPolicy )
{
    root_.base.config.max_retries = 0u;
    PrepareAwaiting();
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 110u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED );

    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED );
    EXPECT_EQ( root_.session.retained_reliable_frame_type,
               HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    EXPECT_EQ( root_.session.retained_transmit_sequence, InitialSequence );
    EXPECT_EQ( root_.session.next_transmit_sequence, InitialSequence );
    EXPECT_EQ( root_.encoded_output_size, FrameSize );
    EXPECT_EQ( root_.event_count, 0u );
    EXPECT_EQ( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );

    std::array<std::uint8_t, RetainedCapacity> output{};
    std::size_t                                output_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, output.data(), output.size(),
                                                          &output_size ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( output_size, 0u );
    const ReliableSnapshot exhausted = Snapshot();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 1000u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
    EXPECT_TRUE( Snapshot() == exhausted );
}

TEST_F( TransportReliabilityTest, ResetClearsOwnershipFromEveryStateWithoutErasingBytes )
{
    constexpr std::array<HIL_Transport_Mvp_Reliable_State_T, 7> States{
        HIL_TRANSPORT_MVP_RELIABLE_IDLE,
        HIL_TRANSPORT_MVP_RELIABLE_READY,
        HIL_TRANSPORT_MVP_RELIABLE_PEEKED,
        HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK,
        HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY,
        HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED,
        HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED,
    };

    for ( const auto state : States )
    {
        SCOPED_TRACE( static_cast<int>( state ) );
        SetValidState( state );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
        EXPECT_EQ( root_.encoded_output_size, 0u );
        EXPECT_EQ( root_.session.retained_reliable_frame_type, HIL_TRANSPORT_MVP_FRAME_INVALID );
        EXPECT_EQ( root_.session.retained_transmit_sequence, 0u );
        EXPECT_EQ( root_.session.retransmissions_committed, 0u );
        EXPECT_EQ( root_.session.reliable_last_committed_ms, 0u );
        EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
        EXPECT_EQ( retained_, original_bytes_ );

        HIL_Transport_Mvp_Frame_Type_T          completed;
        HIL_Transport_Mvp_Reliability_Outcome_T outcome;
        EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                         &completed, &outcome ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_NO_CHANGE );
        Publish( HIL_TRANSPORT_MVP_FRAME_INITIATE );
    }
}

TEST_F( TransportReliabilityTest, PendingStatusMatchesEveryLifecycleState )
{
    constexpr std::array<HIL_Transport_Mvp_Reliable_State_T, 7> States{
        HIL_TRANSPORT_MVP_RELIABLE_IDLE,
        HIL_TRANSPORT_MVP_RELIABLE_READY,
        HIL_TRANSPORT_MVP_RELIABLE_PEEKED,
        HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK,
        HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY,
        HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED,
        HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED,
    };

    for ( const auto state : States )
    {
        SCOPED_TRACE( static_cast<int>( state ) );
        SetValidState( state );
        std::uint8_t output_pending   = 9u;
        std::uint8_t delivery_pending = 9u;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Get_Pending_Status( &root_, &output_pending,
                                                                     &delivery_pending ),
                   HIL_TRANSPORT_STATUS_OK );
        const bool expected_output = state == HIL_TRANSPORT_MVP_RELIABLE_READY
                                     || state == HIL_TRANSPORT_MVP_RELIABLE_PEEKED
                                     || state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY
                                     || state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED;
        EXPECT_EQ( output_pending != 0u, expected_output );
        EXPECT_EQ( delivery_pending != 0u, state != HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    }
}

TEST_F( TransportReliabilityTest, InvalidPrivateStatesFaultWithoutExposingOrOverrunningOutput )
{
    const auto expect_fault_on_peek = [this]() {
        std::array<std::uint8_t, RetainedCapacity> output{};
        output.fill( 0xCCu );
        const auto  untouched   = output;
        std::size_t output_size = 99u;
        EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, output.data(), output.size(),
                                                              &output_size ),
                   HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
        EXPECT_EQ( output_size, 0u );
        EXPECT_EQ( output, untouched );
        EXPECT_EQ( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
        EXPECT_EQ( root_.base.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
        EXPECT_EQ( root_.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT );
        EXPECT_EQ( root_.session.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
    };
    const auto restore_base = [this]() {
        root_.base.session_state   = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.base.last_failure    = HIL_TRANSPORT_FAILURE_NONE;
        root_.session.state        = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.session.last_failure = HIL_TRANSPORT_FAILURE_NONE;
    };

    Publish();
    root_.encoded_output_size = 0u;
    expect_fault_on_peek();
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    restore_base();

    Publish();
    root_.encoded_output_size = retained_.size() + 1u;
    expect_fault_on_peek();
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    restore_base();

    Publish();
    root_.session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_ACK;
    expect_fault_on_peek();
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    restore_base();

    Publish();
    root_.session.next_transmit_sequence = static_cast<std::uint16_t>( InitialSequence + 1u );
    expect_fault_on_peek();
    root_.session.next_transmit_sequence = InitialSequence;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    restore_base();

    SetValidState( HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
    root_.session.retransmissions_committed = root_.base.config.max_retries;
    expect_fault_on_peek();
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    restore_base();

    Publish();
    root_.session.retransmissions_committed =
        static_cast<std::uint8_t>( root_.base.config.max_retries + 1u );
    expect_fault_on_peek();
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    restore_base();

    SetValidState( HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED );
    root_.session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_INVALID;
    expect_fault_on_peek();
}

TEST_F( TransportReliabilityTest, PrivateNullArgumentsAreRejectedWithoutMutation )
{
    Publish();
    const ReliableSnapshot                  before = Snapshot();
    HIL_Transport_Mvp_Frame_Type_T          completed;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    std::uint8_t                            pending = 0u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                     nullptr, &outcome ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                     &completed, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 0u, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Get_Pending_Status( &root_, nullptr, &pending ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Get_Pending_Status( &root_, &pending, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Reliability_Reset( nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_TRUE( Snapshot() == before );
}

TEST_F( TransportReliabilityTest, PublicDisconnectedResetClearsAllSessionScopedState )
{
    HIL_Transport_Context_T context = Context();
    PopulateSessionScopedState();
    root_.base.link_state                      = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
    root_.session.link_state                   = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
    root_.base.session_state                   = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    root_.session.state                        = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    root_.base.operating_mode                  = HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME;
    root_.base.operating_mode_valid            = 1u;
    root_.session.next_host_session_identifier = UINT64_C( 0xABCDEF0123456789 );

    const HIL_Transport_Config_T retained_config              = root_.base.config;
    void* const                  retained_implementation      = context.implementation;
    const std::size_t            retained_implementation_size = context.implementation_size;
    auto* const                  retained_submitted           = root_.submitted_message;
    auto* const                  retained_encoded             = root_.encoded_output;
    const std::size_t            retained_encoded_capacity    = root_.encoded_output_capacity;
    auto* const                  retained_parser              = root_.parser.scratch_buffer;
    const std::size_t            retained_parser_capacity     = root_.parser.scratch_buffer_size;
    auto* const                  retained_codec               = root_.codec_scratch;
    const std::size_t            retained_codec_capacity      = root_.codec_scratch_size;
    auto* const                  retained_received            = root_.received_message;
    const std::uint64_t retained_identity_cursor = root_.session.next_host_session_identifier;
    const auto          retained_bytes           = retained_;
    const auto          submitted_bytes          = submitted_;
    const auto          parser_bytes             = parser_storage_;
    const auto          codec_bytes              = codec_storage_;
    const auto          received_bytes           = received_;

    ASSERT_EQ( HIL_TRANSPORT_Reset( &context ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( root_.session.state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( root_.base.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED );
    EXPECT_EQ( root_.session.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED );
    EXPECT_EQ( root_.base.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    EXPECT_EQ( root_.session.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( root_.encoded_output_size, 0u );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
    EXPECT_EQ( root_.session.retained_reliable_frame_type, HIL_TRANSPORT_MVP_FRAME_INVALID );
    EXPECT_EQ( root_.session.retained_transmit_sequence, 0u );
    EXPECT_EQ( root_.session.retransmissions_committed, 0u );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 0u );
    EXPECT_EQ( root_.parser.accumulated_size, 0u );
    EXPECT_EQ( root_.parser.body_ready, 0u );
    EXPECT_EQ( root_.parser.discarding, 0u );
    EXPECT_EQ( root_.submitted_message_size, 0u );
    EXPECT_EQ( root_.submitted_message_pending, 0u );
    EXPECT_EQ( root_.received_message_size, 0u );
    EXPECT_EQ( root_.received_message_pending, 0u );
    EXPECT_EQ( root_.event_read_index, 0u );
    EXPECT_EQ( root_.event_count, 0u );
    EXPECT_EQ( root_.session.session_identifier, 0u );
    EXPECT_EQ( root_.session.session_identifier_valid, 0u );
    EXPECT_EQ( root_.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE );
    EXPECT_EQ( root_.session.next_transmit_sequence, InitialSequence );
    EXPECT_EQ( root_.session.expected_receive_sequence, InitialSequence );
    EXPECT_EQ( root_.session.last_accepted_receive_sequence, 0u );
    EXPECT_EQ( root_.session.accepted_receive_sequence_valid, 0u );
    EXPECT_EQ( root_.session.last_valid_receive_ms, 0u );

    EXPECT_EQ( context.implementation, retained_implementation );
    EXPECT_EQ( context.implementation_size, retained_implementation_size );
    EXPECT_EQ( context.initialization_cookie, HIL_TRANSPORT_INTERNAL_INITIALIZATION_COOKIE );
    ExpectConfigEqual( root_.base.config, retained_config );
    EXPECT_EQ( root_.base.role, HIL_TRANSPORT_ROLE_HOST );
    EXPECT_EQ( root_.session.role, HIL_TRANSPORT_ROLE_HOST );
    EXPECT_EQ( root_.submitted_message, retained_submitted );
    EXPECT_EQ( root_.encoded_output, retained_encoded );
    EXPECT_EQ( root_.encoded_output_capacity, retained_encoded_capacity );
    EXPECT_EQ( root_.parser.scratch_buffer, retained_parser );
    EXPECT_EQ( root_.parser.scratch_buffer_size, retained_parser_capacity );
    EXPECT_EQ( root_.codec_scratch, retained_codec );
    EXPECT_EQ( root_.codec_scratch_size, retained_codec_capacity );
    EXPECT_EQ( root_.received_message, retained_received );
    EXPECT_EQ( root_.session.next_host_session_identifier, retained_identity_cursor );
    EXPECT_EQ( root_.base.operating_mode, HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME );
    EXPECT_EQ( root_.base.operating_mode_valid, 1u );
    EXPECT_EQ( retained_, retained_bytes );
    EXPECT_EQ( submitted_, submitted_bytes );
    EXPECT_EQ( parser_storage_, parser_bytes );
    EXPECT_EQ( codec_storage_, codec_bytes );
    EXPECT_EQ( received_, received_bytes );
}

TEST_F( TransportReliabilityTest, PublicConnectedResetEntersRecoveryWithoutStartingHandshake )
{
    HIL_Transport_Context_T context = Context();
    PopulateSessionScopedState();
    root_.base.link_state    = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root_.session.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root_.base.session_state = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    root_.session.state      = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;

    ASSERT_EQ( HIL_TRANSPORT_Reset( &context ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root_.base.link_state, HIL_TRANSPORT_LINK_STATE_CONNECTED );
    EXPECT_EQ( root_.session.link_state, HIL_TRANSPORT_LINK_STATE_CONNECTED );
    EXPECT_EQ( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( root_.session.state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( root_.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE );
    EXPECT_EQ( root_.base.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    EXPECT_EQ( root_.session.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( root_.encoded_output_size, 0u );
}

TEST_F( TransportReliabilityTest, PublicResetClearsFaultUsingRetainedLinkObservation )
{
    HIL_Transport_Context_T context = Context();
    PopulateSessionScopedState();
    root_.base.link_state    = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root_.session.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root_.base.session_state = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root_.session.state      = HIL_TRANSPORT_SESSION_STATE_FAULT;

    ASSERT_EQ( HIL_TRANSPORT_Reset( &context ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( root_.session.state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( root_.base.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    EXPECT_EQ( root_.session.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( root_.encoded_output_size, 0u );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
}

TEST_F( TransportReliabilityTest, PublicStatusReportsCompleteImplementedSnapshot )
{
    HIL_Transport_Context_T context = Context();
    Publish();
    root_.base.role                 = HIL_TRANSPORT_ROLE_RIG;
    root_.base.link_state           = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root_.base.session_state        = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
    root_.base.operating_mode       = HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER;
    root_.base.operating_mode_valid = 1u;
    root_.base.last_failure         = HIL_TRANSPORT_FAILURE_PROTOCOL;
    root_.received_message_pending  = 1u;
    const HIL_Transport_Event_T event{ HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,
                                       HIL_TRANSPORT_STATUS_INVALID_ARGUMENT,
                                       HIL_TRANSPORT_FAILURE_PROTOCOL, 0u };
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &event ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Status_Snapshot_T status{};

    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &context, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.role, HIL_TRANSPORT_ROLE_RIG );
    EXPECT_EQ( status.link_state, HIL_TRANSPORT_LINK_STATE_CONNECTED );
    EXPECT_EQ( status.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( status.operating_mode, HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER );
    EXPECT_EQ( status.operating_mode_valid, 1u );
    EXPECT_EQ( status.output_pending, 1u );
    EXPECT_EQ( status.reliable_delivery_pending, 1u );
    EXPECT_EQ( status.application_message_pending, 1u );
    EXPECT_EQ( status.event_pending, 1u );
    EXPECT_EQ( status.last_failure, HIL_TRANSPORT_FAILURE_PROTOCOL );
}

TEST( TransportReliabilityPublicArguments, InvalidContextsAndPointerCombinationsAreRejected )
{
    HIL_Transport_Context_T      context{};
    std::array<std::uint8_t, 8u> output{};
    std::size_t                  output_size = 99u;

    EXPECT_EQ( HIL_TRANSPORT_Peek_Output( &context, output.data(), output.size(), &output_size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( output_size, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Peek_Output( &context, nullptr, 1u, &output_size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( output_size, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Peek_Output( &context, nullptr, 0u, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_Commit_Output( &context, 0u ), HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    HIL_Transport_Mvp_Root_T      root{};
    HIL_Transport_Context_T       invalid_reset_context{ &root, sizeof( root ), 0u };
    const HIL_Transport_Context_T unchanged_context = invalid_reset_context;
    EXPECT_EQ( HIL_TRANSPORT_Reset( &invalid_reset_context ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( invalid_reset_context.implementation, unchanged_context.implementation );
    EXPECT_EQ( invalid_reset_context.implementation_size, unchanged_context.implementation_size );
    EXPECT_EQ( invalid_reset_context.initialization_cookie,
               unchanged_context.initialization_cookie );

    HIL_Transport_Status_Snapshot_T status{};
    status.role                        = HIL_TRANSPORT_ROLE_RIG;
    status.link_state                  = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    status.session_state               = HIL_TRANSPORT_SESSION_STATE_FAULT;
    status.operating_mode              = HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER;
    status.operating_mode_valid        = 1u;
    status.output_pending              = 1u;
    status.application_message_pending = 1u;
    status.event_pending               = 1u;
    status.reliable_delivery_pending   = 1u;
    status.last_failure                = HIL_TRANSPORT_FAILURE_INTERNAL;
    EXPECT_EQ( HIL_TRANSPORT_Get_Status( &context, &status ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( status.role, HIL_TRANSPORT_ROLE_HOST );
    EXPECT_EQ( status.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED );
    EXPECT_EQ( status.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( status.operating_mode, HIL_TRANSPORT_OPERATING_MODE_NORMAL );
    EXPECT_EQ( status.operating_mode_valid, 0u );
    EXPECT_EQ( status.output_pending, 0u );
    EXPECT_EQ( status.application_message_pending, 0u );
    EXPECT_EQ( status.event_pending, 0u );
    EXPECT_EQ( status.reliable_delivery_pending, 0u );
    EXPECT_EQ( status.last_failure, HIL_TRANSPORT_FAILURE_NONE );
}

}  // namespace
