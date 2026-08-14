#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include "hil_rig_protocol/transport/transport.h"
#include "transport/internal/mvp/transport_control_output_mvp.h"
#include "transport/internal/mvp/transport_output_mvp.h"
#include "transport/internal/mvp/transport_reliability_mvp.h"
#include "transport/internal/transport_internal.h"

namespace {

constexpr std::size_t   ReliableCapacity = 32u;
constexpr std::size_t   ReliableSize     = 7u;
constexpr std::size_t   ControlSize      = 11u;
constexpr std::uint16_t InitialSequence  = 0x2468u;
constexpr std::uint32_t TimeoutMs        = 10u;

constexpr std::array<std::uint8_t, ReliableSize> ReliableBytes{
    0xA1u, 0xA2u, 0x00u, 0xA4u, 0xA5u, 0xA6u, 0xA7u,
};
constexpr std::array<std::uint8_t, ControlSize> ControlBytes{
    0xC1u, 0xC2u, 0x00u, 0xC4u, 0xC5u, 0xC6u, 0x00u, 0xC8u, 0xC9u, 0xCAu, 0xCBu,
};

struct ReliableSnapshot
{
    HIL_Transport_Mvp_Reliable_State_T         state;
    HIL_Transport_Mvp_Frame_Type_T             frame_type;
    std::uint16_t                              next_sequence;
    std::uint16_t                              retained_sequence;
    std::uint8_t                               retransmissions_committed;
    std::uint32_t                              last_committed_ms;
    std::size_t                                encoded_size;
    std::array<std::uint8_t, ReliableCapacity> bytes;

    bool operator==( const ReliableSnapshot& other ) const
    {
        return state == other.state && frame_type == other.frame_type
               && next_sequence == other.next_sequence
               && retained_sequence == other.retained_sequence
               && retransmissions_committed == other.retransmissions_committed
               && last_committed_ms == other.last_committed_ms && encoded_size == other.encoded_size
               && bytes == other.bytes;
    }
};

struct ControlSnapshot
{
    HIL_Transport_Mvp_Control_Output_State_T                            state;
    std::size_t                                                         size;
    std::array<std::uint8_t, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY> bytes;

    bool operator==( const ControlSnapshot& other ) const
    {
        return state == other.state && size == other.size && bytes == other.bytes;
    }
};

enum class InvalidRelationship
{
    InvalidSelection,
    NoneWithReliablePeeked,
    NoneWithControlPeeked,
    ReliableWithReliableNotPeeked,
    ControlWithControlNotPeeked,
    BothPeeked,
    ReliableWithOnlyControlPeeked,
    ControlWithOnlyReliablePeeked,
};

class TransportOutputTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        reliable_storage_.fill( 0x5Au );
        root_.base.config.max_encoded_frame_size    = reliable_storage_.size();
        root_.base.config.initial_reliable_sequence = InitialSequence;
        root_.base.config.retransmit_timeout_ms     = TimeoutMs;
        root_.base.config.max_retries               = 2u;
        root_.base.role                             = HIL_TRANSPORT_ROLE_HOST;
        root_.base.link_state                       = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
        root_.base.session_state                    = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.session.role                          = HIL_TRANSPORT_ROLE_HOST;
        root_.session.link_state                    = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
        root_.session.state                         = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.session.initial_reliable_sequence     = InitialSequence;
        root_.session.next_transmit_sequence        = InitialSequence;
        root_.encoded_output                        = reliable_storage_.data();
        root_.encoded_output_capacity               = reliable_storage_.size();
        ASSERT_EQ( HIL_TRANSPORT_MVP_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    }

    void PublishReliable()
    {
        std::copy( ReliableBytes.begin(), ReliableBytes.end(), reliable_storage_.begin() );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Publish_Encoded(
                       &root_, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE,
                       root_.session.next_transmit_sequence, ReliableSize ),
                   HIL_TRANSPORT_STATUS_OK );
    }

    void PublishControl()
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, ControlBytes.data(),
                                                                     ControlBytes.size() ),
                   HIL_TRANSPORT_STATUS_OK );
    }

    std::array<std::uint8_t, ReliableCapacity> PeekOutput( std::size_t expected_size )
    {
        std::array<std::uint8_t, ReliableCapacity> output{};
        output.fill( 0xEEu );
        std::size_t output_size = 0u;
        EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Peek_Output( &root_, output.data(), output.size(),
                                                         &output_size ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( output_size, expected_size );
        return output;
    }

    void CommitOutput( std::uint32_t now_ms = 100u )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Output_Commit_Output( &root_, now_ms ),
                   HIL_TRANSPORT_STATUS_OK );
    }

    void PrepareAwaiting( std::uint32_t now_ms = 100u )
    {
        PublishReliable();
        ( void )PeekOutput( ReliableSize );
        CommitOutput( now_ms );
        ASSERT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    }

    void PrepareRetryReady( std::uint32_t now_ms = 110u )
    {
        HIL_Transport_Mvp_Reliability_Outcome_T outcome;
        PrepareAwaiting( now_ms - TimeoutMs );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, now_ms, &outcome ),
                   HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );
    }

    void PrepareRetryAwaiting()
    {
        PrepareRetryReady();
        ( void )PeekOutput( ReliableSize );
        CommitOutput( 120u );
        ASSERT_EQ( root_.session.retransmissions_committed, 1u );
    }

    void PrivatePeekReliable()
    {
        std::array<std::uint8_t, ReliableCapacity> output{};
        std::size_t                                output_size = 0u;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Peek_Output( &root_, output.data(), output.size(),
                                                              &output_size ),
                   HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( output_size, ReliableSize );
    }

    void PrivatePeekControl()
    {
        std::array<std::uint8_t, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY> output{};
        std::size_t                                                         output_size = 0u;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, output.data(),
                                                                 output.size(), &output_size ),
                   HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( output_size, ControlSize );
    }

    void ResetOutputs()
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
        root_.base.config.max_retries        = 2u;
        root_.session.next_transmit_sequence = InitialSequence;
        root_.base.session_state             = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.base.last_failure              = HIL_TRANSPORT_FAILURE_NONE;
        root_.session.state                  = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.session.last_failure           = HIL_TRANSPORT_FAILURE_NONE;
    }

    ReliableSnapshot SnapshotReliable() const
    {
        return ReliableSnapshot{
            root_.session.reliable_state,
            root_.session.retained_reliable_frame_type,
            root_.session.next_transmit_sequence,
            root_.session.retained_transmit_sequence,
            root_.session.retransmissions_committed,
            root_.session.reliable_last_committed_ms,
            root_.encoded_output_size,
            reliable_storage_,
        };
    }

    ControlSnapshot SnapshotControl() const
    {
        ControlSnapshot snapshot{ root_.control_output_state, root_.control_output_size, {} };
        std::copy( std::begin( root_.control_output ), std::end( root_.control_output ),
                   snapshot.bytes.begin() );
        return snapshot;
    }

    HIL_Transport_Context_T Context()
    {
        return HIL_Transport_Context_T{ &root_, sizeof( root_ ),
                                        HIL_TRANSPORT_INTERNAL_INITIALIZATION_COOKIE };
    }

    void ExpectOutputBytes( const std::array<std::uint8_t, ReliableCapacity>& output,
                            const std::uint8_t* expected, std::size_t expected_size ) const
    {
        EXPECT_TRUE( std::equal( output.begin(), output.begin() + expected_size, expected ) );
    }

    void ExpectPending( bool output_expected, bool delivery_expected )
    {
        std::uint8_t output_pending   = 9u;
        std::uint8_t delivery_pending = 9u;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Output_Get_Pending_Status( &root_, &output_pending,
                                                                &delivery_pending ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( output_pending != 0u, output_expected );
        EXPECT_EQ( delivery_pending != 0u, delivery_expected );
    }

    void ExpectResetState() const
    {
        EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
        EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
        EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
        EXPECT_EQ( root_.encoded_output_size, 0u );
        EXPECT_EQ( root_.control_output_size, 0u );
    }

    void PrepareInvalidRelationship( InvalidRelationship relationship )
    {
        switch ( relationship )
        {
            case InvalidRelationship::InvalidSelection:
                root_.output_selection = static_cast<HIL_Transport_Mvp_Output_Selection_T>( 99 );
                break;

            case InvalidRelationship::NoneWithReliablePeeked:
                PublishReliable();
                PrivatePeekReliable();
                root_.output_selection = HIL_TRANSPORT_MVP_OUTPUT_NONE;
                break;

            case InvalidRelationship::NoneWithControlPeeked:
                PublishControl();
                PrivatePeekControl();
                root_.output_selection = HIL_TRANSPORT_MVP_OUTPUT_NONE;
                break;

            case InvalidRelationship::ReliableWithReliableNotPeeked:
                PublishReliable();
                root_.output_selection = HIL_TRANSPORT_MVP_OUTPUT_RELIABLE;
                break;

            case InvalidRelationship::ControlWithControlNotPeeked:
                PublishControl();
                root_.output_selection = HIL_TRANSPORT_MVP_OUTPUT_CONTROL;
                break;

            case InvalidRelationship::BothPeeked:
                PublishReliable();
                PublishControl();
                PrivatePeekReliable();
                PrivatePeekControl();
                root_.output_selection = HIL_TRANSPORT_MVP_OUTPUT_RELIABLE;
                break;

            case InvalidRelationship::ReliableWithOnlyControlPeeked:
                PublishControl();
                PrivatePeekControl();
                root_.output_selection = HIL_TRANSPORT_MVP_OUTPUT_RELIABLE;
                break;

            case InvalidRelationship::ControlWithOnlyReliablePeeked:
                PublishReliable();
                PrivatePeekReliable();
                root_.output_selection = HIL_TRANSPORT_MVP_OUTPUT_CONTROL;
                break;
        }
    }

    void ExpectFault() const
    {
        EXPECT_EQ( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
        EXPECT_EQ( root_.base.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
        EXPECT_EQ( root_.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT );
        EXPECT_EQ( root_.session.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
    }

    HIL_Transport_Mvp_Root_T                   root_{};
    std::array<std::uint8_t, ReliableCapacity> reliable_storage_{};
};

TEST_F( TransportOutputTest, NoOutputIsNotReady )
{
    std::array<std::uint8_t, ReliableCapacity> output{};
    std::size_t                                output_size = 99u;

    EXPECT_EQ(
        HIL_TRANSPORT_MVP_Output_Peek_Output( &root_, output.data(), output.size(), &output_size ),
        HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( output_size, 0u );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Commit_Output( &root_, 1u ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST_F( TransportOutputTest, ReliableOnlyReturnsReliableBytes )
{
    PublishReliable();
    const auto output = PeekOutput( ReliableSize );

    ExpectOutputBytes( output, ReliableBytes.data(), ReliableBytes.size() );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_RELIABLE );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_PEEKED );
}

TEST_F( TransportOutputTest, ControlOnlyReturnsControlBytes )
{
    PublishControl();
    const auto output = PeekOutput( ControlSize );

    ExpectOutputBytes( output, ControlBytes.data(), ControlBytes.size() );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED );
}

TEST_F( TransportOutputTest, ControlPriorityAndCommitExposeReliableNext )
{
    PublishReliable();
    PublishControl();

    const auto control = PeekOutput( ControlSize );
    ExpectOutputBytes( control, ControlBytes.data(), ControlBytes.size() );
    CommitOutput( 0xFFFFFFFFu );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_READY );

    const auto reliable = PeekOutput( ReliableSize );
    ExpectOutputBytes( reliable, ReliableBytes.data(), ReliableBytes.size() );
}

TEST_F( TransportOutputTest, ReliableCommitExposesControlNext )
{
    PublishReliable();
    const auto reliable = PeekOutput( ReliableSize );
    ExpectOutputBytes( reliable, ReliableBytes.data(), ReliableBytes.size() );
    PublishControl();

    CommitOutput( 77u );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 77u );
    const auto control = PeekOutput( ControlSize );
    ExpectOutputBytes( control, ControlBytes.data(), ControlBytes.size() );
}

TEST_F( TransportOutputTest, QueriesAndSmallBuffersKeepControlPriorityWithoutPinning )
{
    PublishReliable();
    PublishControl();
    const auto  reliable_before = SnapshotReliable();
    const auto  control_before  = SnapshotControl();
    std::size_t output_size     = 99u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Peek_Output( &root_, nullptr, 0u, &output_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, ControlSize );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
    EXPECT_TRUE( SnapshotReliable() == reliable_before );
    EXPECT_TRUE( SnapshotControl() == control_before );

    std::array<std::uint8_t, ControlSize - 1u> small{};
    small.fill( 0xE5u );
    const auto untouched = small;
    output_size          = 99u;
    EXPECT_EQ(
        HIL_TRANSPORT_MVP_Output_Peek_Output( &root_, small.data(), small.size(), &output_size ),
        HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, ControlSize );
    EXPECT_EQ( small, untouched );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
    EXPECT_TRUE( SnapshotReliable() == reliable_before );
    EXPECT_TRUE( SnapshotControl() == control_before );

    const auto output = PeekOutput( ControlSize );
    ExpectOutputBytes( output, ControlBytes.data(), ControlBytes.size() );
}

TEST_F( TransportOutputTest, InvalidPeekArgumentsClearSizeWithoutLifecycleMutation )
{
    PublishReliable();
    PublishControl();
    const auto  reliable_before = SnapshotReliable();
    const auto  control_before  = SnapshotControl();
    std::size_t output_size     = 99u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Peek_Output( &root_, nullptr, 1u, &output_size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( output_size, 0u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Peek_Output( &root_, nullptr, 0u, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    output_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Peek_Output( nullptr, nullptr, 0u, &output_size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( output_size, 0u );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
    EXPECT_TRUE( SnapshotReliable() == reliable_before );
    EXPECT_TRUE( SnapshotControl() == control_before );
}

TEST_F( TransportOutputTest, ReliableSelectionRemainsPinnedWhenControlBecomesReady )
{
    PublishReliable();
    const auto first = PeekOutput( ReliableSize );
    PublishControl();

    const auto repeated = PeekOutput( ReliableSize );
    EXPECT_EQ( repeated, first );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_RELIABLE );
    CommitOutput( 55u );

    const auto control = PeekOutput( ControlSize );
    ExpectOutputBytes( control, ControlBytes.data(), ControlBytes.size() );
}

TEST_F( TransportOutputTest, ControlSelectionRemainsPinnedWhenReliableBecomesReady )
{
    PublishControl();
    const auto first = PeekOutput( ControlSize );
    PublishReliable();

    const auto repeated = PeekOutput( ControlSize );
    EXPECT_EQ( repeated, first );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
    CommitOutput( 0xFFFFFFFFu );

    const auto reliable = PeekOutput( ReliableSize );
    ExpectOutputBytes( reliable, ReliableBytes.data(), ReliableBytes.size() );
}

TEST_F( TransportOutputTest, ControlCommitLeavesEveryReliableFieldUnchanged )
{
    PrepareRetryAwaiting();
    PublishControl();
    ( void )PeekOutput( ControlSize );
    const auto reliable_before = SnapshotReliable();

    CommitOutput( 0xFFFFFFFFu );
    EXPECT_TRUE( SnapshotReliable() == reliable_before );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE );
}

TEST_F( TransportOutputTest, ReliableCommitLeavesEveryControlFieldUnchanged )
{
    PublishReliable();
    ( void )PeekOutput( ReliableSize );
    PublishControl();
    const auto control_before = SnapshotControl();

    CommitOutput( 88u );
    EXPECT_TRUE( SnapshotControl() == control_before );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 88u );
}

TEST_F( TransportOutputTest, ReliableCompletionDoesNotClearPinnedControlSelection )
{
    PrepareAwaiting( 44u );
    PublishControl();
    const auto first = PeekOutput( ControlSize );

    HIL_Transport_Mvp_Frame_Type_T          completed;
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Accept_Acknowledgement( &root_, InitialSequence,
                                                                     &completed, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_ACKNOWLEDGED );
    EXPECT_EQ( completed, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    EXPECT_EQ( root_.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );

    const auto repeated = PeekOutput( ControlSize );
    EXPECT_EQ( repeated, first );
    CommitOutput();
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
}

TEST_F( TransportOutputTest, ReliableRetryReadyDoesNotDisplacePinnedControlSelection )
{
    PrepareAwaiting( 100u );
    PublishControl();
    const auto                              first = PeekOutput( ControlSize );
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 110u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRANSMIT_READY );
    EXPECT_EQ( root_.output_selection, HIL_TRANSPORT_MVP_OUTPUT_CONTROL );
    const auto repeated = PeekOutput( ControlSize );
    EXPECT_EQ( repeated, first );
}

TEST_F( TransportOutputTest, PendingStatusAggregatesOutputButKeepsDeliveryReliableOnly )
{
    ExpectPending( false, false );

    PublishReliable();
    ExpectPending( true, true );
    ResetOutputs();

    PrepareAwaiting();
    ExpectPending( false, true );
    PublishControl();
    ExpectPending( true, true );
    ResetOutputs();

    PublishControl();
    ExpectPending( true, false );
    ResetOutputs();

    root_.base.config.max_retries = 0u;
    PrepareAwaiting();
    HIL_Transport_Mvp_Reliability_Outcome_T outcome;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Reliability_Process_Pending( &root_, 110u, &outcome ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( outcome, HIL_TRANSPORT_MVP_RELIABILITY_RETRIES_EXHAUSTED );
    PublishControl();
    ExpectPending( true, true );
    ResetOutputs();

    PrepareRetryReady();
    PublishControl();
    ExpectPending( true, true );
    ResetOutputs();

    PublishReliable();
    ( void )PeekOutput( ReliableSize );
    ExpectPending( true, true );
    ResetOutputs();

    PublishControl();
    ( void )PeekOutput( ControlSize );
    ExpectPending( true, false );
}

TEST_F( TransportOutputTest, ResetClearsEverySpecifiedOwnershipCombination )
{
    for ( int scenario = 0; scenario < 7; ++scenario )
    {
        SCOPED_TRACE( scenario );
        ResetOutputs();
        switch ( scenario )
        {
            case 0:
                break;
            case 1:
                PublishReliable();
                break;
            case 2:
                PublishControl();
                break;
            case 3:
                PublishReliable();
                ( void )PeekOutput( ReliableSize );
                break;
            case 4:
                PublishControl();
                ( void )PeekOutput( ControlSize );
                break;
            case 5:
                PrepareAwaiting();
                PublishControl();
                break;
            case 6:
                root_.output_selection = static_cast<HIL_Transport_Mvp_Output_Selection_T>( 99 );
                break;
            default:
                FAIL() << "Unexpected reset scenario";
                break;
        }

        ASSERT_EQ( HIL_TRANSPORT_MVP_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
        ExpectResetState();
    }
}

TEST_F( TransportOutputTest, InvalidCrossOutputRelationshipsFaultWithoutExposingBytes )
{
    constexpr std::array<InvalidRelationship, 8> Relationships{
        InvalidRelationship::InvalidSelection,
        InvalidRelationship::NoneWithReliablePeeked,
        InvalidRelationship::NoneWithControlPeeked,
        InvalidRelationship::ReliableWithReliableNotPeeked,
        InvalidRelationship::ControlWithControlNotPeeked,
        InvalidRelationship::BothPeeked,
        InvalidRelationship::ReliableWithOnlyControlPeeked,
        InvalidRelationship::ControlWithOnlyReliablePeeked,
    };

    for ( const auto relationship : Relationships )
    {
        SCOPED_TRACE( static_cast<int>( relationship ) );
        ResetOutputs();
        PrepareInvalidRelationship( relationship );
        const auto                                 selection_before = root_.output_selection;
        std::array<std::uint8_t, ReliableCapacity> output{};
        output.fill( 0xD5u );
        const auto  untouched   = output;
        std::size_t output_size = 99u;

        EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Peek_Output( &root_, output.data(), output.size(),
                                                         &output_size ),
                   HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
        EXPECT_EQ( output_size, 0u );
        EXPECT_EQ( output, untouched );
        EXPECT_EQ( root_.output_selection, selection_before );
        ExpectFault();

        EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Commit_Output( &root_, 123u ),
                   HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
        EXPECT_EQ( root_.output_selection, selection_before );

        std::uint8_t output_pending   = 9u;
        std::uint8_t delivery_pending = 9u;
        EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Get_Pending_Status( &root_, &output_pending,
                                                                &delivery_pending ),
                   HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
        EXPECT_EQ( output_pending, 0u );
        EXPECT_EQ( delivery_pending, 0u );
        ExpectFault();
    }
}

TEST_F( TransportOutputTest, PublicFacadeArbitratesAndCommitsBothLifecycles )
{
    HIL_Transport_Context_T context = Context();
    PublishReliable();
    PublishControl();
    HIL_Transport_Status_Snapshot_T status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &context, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.output_pending, 1u );
    EXPECT_EQ( status.reliable_delivery_pending, 1u );

    std::array<std::uint8_t, ReliableCapacity> output{};
    std::size_t                                output_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Peek_Output( &context, output.data(), output.size(), &output_size ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( output_size, ControlSize );
    EXPECT_TRUE( std::equal( output.begin(), output.begin() + ControlSize, ControlBytes.begin() ) );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &context, 0xFFFFFFFFu ), HIL_TRANSPORT_STATUS_OK );

    output.fill( 0u );
    ASSERT_EQ( HIL_TRANSPORT_Peek_Output( &context, output.data(), output.size(), &output_size ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( output_size, ReliableSize );
    EXPECT_TRUE(
        std::equal( output.begin(), output.begin() + ReliableSize, ReliableBytes.begin() ) );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &context, 123u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root_.session.reliable_last_committed_ms, 123u );
}

TEST_F( TransportOutputTest, PendingStatusArgumentsAreDefensivelyCleared )
{
    std::uint8_t output_pending   = 9u;
    std::uint8_t delivery_pending = 9u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Get_Pending_Status( &root_, nullptr, &delivery_pending ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( delivery_pending, 9u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Output_Get_Pending_Status( &root_, &output_pending, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( output_pending, 9u );

    EXPECT_EQ(
        HIL_TRANSPORT_MVP_Output_Get_Pending_Status( nullptr, &output_pending, &delivery_pending ),
        HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( output_pending, 0u );
    EXPECT_EQ( delivery_pending, 0u );
}

}  // namespace
