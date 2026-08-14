#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "hil_rig_protocol/transport/transport.h"
#include "transport/internal/mvp/transport_events_mvp.h"
#include "transport/internal/mvp/transport_frame_codec_mvp.h"

namespace {

using EventArray = std::array<HIL_Transport_Event_T, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY>;

HIL_Transport_Event_T Event( std::size_t identifier )
{
    constexpr std::array<HIL_Transport_Event_Type_T, 7u> Types{
        HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED, HIL_TRANSPORT_EVENT_SESSION_RESET,
        HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED,  HIL_TRANSPORT_EVENT_DELIVERY_FAILED,
        HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,      HIL_TRANSPORT_EVENT_CAPACITY_EXHAUSTED,
        HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED };
    constexpr std::array<HIL_Transport_Status_T, 4u> Statuses{
        HIL_TRANSPORT_STATUS_OK, HIL_TRANSPORT_STATUS_NOT_READY,
        HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED, HIL_TRANSPORT_STATUS_DELIVERY_FAILED };
    constexpr std::array<HIL_Transport_Failure_T, 4u> Failures{
        HIL_TRANSPORT_FAILURE_NONE, HIL_TRANSPORT_FAILURE_PROTOCOL, HIL_TRANSPORT_FAILURE_CAPACITY,
        HIL_TRANSPORT_FAILURE_DELIVERY };
    return HIL_Transport_Event_T{ Types[identifier % Types.size()],
                                  Statuses[identifier % Statuses.size()],
                                  Failures[identifier % Failures.size()], 100u + identifier };
}

void ExpectEventEqual( const HIL_Transport_Event_T& actual, const HIL_Transport_Event_T& expected )
{
    EXPECT_EQ( actual.type, expected.type );
    EXPECT_EQ( actual.status, expected.status );
    EXPECT_EQ( actual.failure, expected.failure );
    EXPECT_EQ( actual.required_capacity, expected.required_capacity );
}

EventArray SnapshotEvents( const HIL_Transport_Mvp_Root_T& root )
{
    EventArray snapshot{};
    std::copy( std::begin( root.event_queue ), std::end( root.event_queue ), snapshot.begin() );
    return snapshot;
}

void ExpectEventArrayEqual( const EventArray& actual, const EventArray& expected )
{
    for ( std::size_t index = 0u; index < actual.size(); ++index )
    {
        ExpectEventEqual( actual[index], expected[index] );
    }
}

template <typename Enum> Enum InvalidEnum( std::underlying_type_t<Enum> invalid_value )
{
    static_assert( sizeof( Enum ) == sizeof( std::underlying_type_t<Enum> ) );
    Enum result{};
    std::memcpy( &result, &invalid_value, sizeof( result ) );
    return result;
}

struct UnrelatedSnapshot
{
    HIL_Transport_Internal_State_T                                      base;
    HIL_Transport_Mvp_Session_T                                         session;
    HIL_Transport_Parser_T                                              parser;
    std::uint8_t*                                                       submitted_message;
    std::size_t                                                         submitted_message_size;
    std::uint8_t                                                        submitted_message_pending;
    std::uint8_t*                                                       encoded_output;
    std::size_t                                                         encoded_output_capacity;
    std::size_t                                                         encoded_output_size;
    HIL_Transport_Mvp_Output_Selection_T                                output_selection;
    std::array<std::uint8_t, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY> control;
    std::size_t                                                         control_output_size;
    HIL_Transport_Mvp_Control_Output_State_T                            control_output_state;
    std::uint8_t*                                                       codec_scratch;
    std::size_t                                                         codec_scratch_size;
    std::uint8_t*                                                       received_message;
    std::size_t                                                         received_message_size;
    std::uint8_t                                                        received_message_pending;
};

UnrelatedSnapshot SnapshotUnrelated( const HIL_Transport_Mvp_Root_T& root )
{
    UnrelatedSnapshot snapshot{ root.base,
                                root.session,
                                root.parser,
                                root.submitted_message,
                                root.submitted_message_size,
                                root.submitted_message_pending,
                                root.encoded_output,
                                root.encoded_output_capacity,
                                root.encoded_output_size,
                                root.output_selection,
                                {},
                                root.control_output_size,
                                root.control_output_state,
                                root.codec_scratch,
                                root.codec_scratch_size,
                                root.received_message,
                                root.received_message_size,
                                root.received_message_pending };
    std::copy( std::begin( root.control_output ), std::end( root.control_output ),
               snapshot.control.begin() );
    return snapshot;
}

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

void ExpectUnrelatedEqual( const HIL_Transport_Mvp_Root_T& root, const UnrelatedSnapshot& expected )
{
    ExpectConfigEqual( root.base.config, expected.base.config );
    EXPECT_EQ( root.base.role, expected.base.role );
    EXPECT_EQ( root.base.link_state, expected.base.link_state );
    EXPECT_EQ( root.base.session_state, expected.base.session_state );
    EXPECT_EQ( root.base.operating_mode, expected.base.operating_mode );
    EXPECT_EQ( root.base.operating_mode_valid, expected.base.operating_mode_valid );
    EXPECT_EQ( root.base.last_failure, expected.base.last_failure );
    EXPECT_EQ( root.session.role, expected.session.role );
    EXPECT_EQ( root.session.link_state, expected.session.link_state );
    EXPECT_EQ( root.session.state, expected.session.state );
    EXPECT_EQ( root.session.handshake_phase, expected.session.handshake_phase );
    EXPECT_EQ( root.session.session_identifier, expected.session.session_identifier );
    EXPECT_EQ( root.session.session_identifier_valid, expected.session.session_identifier_valid );
    EXPECT_EQ( root.session.next_host_session_identifier,
               expected.session.next_host_session_identifier );
    EXPECT_EQ( root.session.initial_reliable_sequence, expected.session.initial_reliable_sequence );
    EXPECT_EQ( root.session.next_transmit_sequence, expected.session.next_transmit_sequence );
    EXPECT_EQ( root.session.expected_receive_sequence, expected.session.expected_receive_sequence );
    EXPECT_EQ( root.session.retained_transmit_sequence,
               expected.session.retained_transmit_sequence );
    EXPECT_EQ( root.session.retained_reliable_frame_type,
               expected.session.retained_reliable_frame_type );
    EXPECT_EQ( root.session.last_accepted_receive_sequence,
               expected.session.last_accepted_receive_sequence );
    EXPECT_EQ( root.session.accepted_receive_sequence_valid,
               expected.session.accepted_receive_sequence_valid );
    EXPECT_EQ( root.session.reliable_state, expected.session.reliable_state );
    EXPECT_EQ( root.session.retransmissions_committed, expected.session.retransmissions_committed );
    EXPECT_EQ( root.session.reliable_last_committed_ms,
               expected.session.reliable_last_committed_ms );
    EXPECT_EQ( root.session.last_valid_receive_ms, expected.session.last_valid_receive_ms );
    EXPECT_EQ( root.session.last_failure, expected.session.last_failure );
    EXPECT_EQ( root.parser.scratch_buffer, expected.parser.scratch_buffer );
    EXPECT_EQ( root.parser.scratch_buffer_size, expected.parser.scratch_buffer_size );
    EXPECT_EQ( root.parser.accumulated_size, expected.parser.accumulated_size );
    EXPECT_EQ( root.parser.body_ready, expected.parser.body_ready );
    EXPECT_EQ( root.parser.discarding, expected.parser.discarding );
    EXPECT_EQ( root.submitted_message, expected.submitted_message );
    EXPECT_EQ( root.submitted_message_size, expected.submitted_message_size );
    EXPECT_EQ( root.submitted_message_pending, expected.submitted_message_pending );
    EXPECT_EQ( root.encoded_output, expected.encoded_output );
    EXPECT_EQ( root.encoded_output_capacity, expected.encoded_output_capacity );
    EXPECT_EQ( root.encoded_output_size, expected.encoded_output_size );
    EXPECT_EQ( root.output_selection, expected.output_selection );
    EXPECT_TRUE( std::equal( std::begin( root.control_output ), std::end( root.control_output ),
                             expected.control.begin() ) );
    EXPECT_EQ( root.control_output_size, expected.control_output_size );
    EXPECT_EQ( root.control_output_state, expected.control_output_state );
    EXPECT_EQ( root.codec_scratch, expected.codec_scratch );
    EXPECT_EQ( root.codec_scratch_size, expected.codec_scratch_size );
    EXPECT_EQ( root.received_message, expected.received_message );
    EXPECT_EQ( root.received_message_size, expected.received_message_size );
    EXPECT_EQ( root.received_message_pending, expected.received_message_pending );
}

class TransportEventsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        root_.base.config.max_application_message_size = submitted_.size();
        root_.base.config.max_encoded_frame_size       = encoded_.size();
        root_.base.config.session_seed                 = UINT64_C( 0x1122334455667788 );
        root_.base.config.initial_reliable_sequence    = 0x1234u;
        root_.base.config.retransmit_timeout_ms        = 77u;
        root_.base.config.max_retries                  = 3u;
        root_.base.role                                = HIL_TRANSPORT_ROLE_RIG;
        root_.base.link_state                          = HIL_TRANSPORT_LINK_STATE_CONNECTED;
        root_.base.session_state                       = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
        root_.base.operating_mode       = HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME;
        root_.base.operating_mode_valid = 1u;
        root_.base.last_failure         = HIL_TRANSPORT_FAILURE_LINK_LOST;

        root_.session.role                         = HIL_TRANSPORT_ROLE_RIG;
        root_.session.link_state                   = HIL_TRANSPORT_LINK_STATE_CONNECTED;
        root_.session.state                        = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
        root_.session.handshake_phase              = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED;
        root_.session.session_identifier           = UINT64_C( 0x8877665544332211 );
        root_.session.session_identifier_valid     = 1u;
        root_.session.next_host_session_identifier = UINT64_C( 0x12345678 );
        root_.session.initial_reliable_sequence    = 0x1234u;
        root_.session.next_transmit_sequence       = 0x2345u;
        root_.session.expected_receive_sequence    = 0x3456u;
        root_.session.retained_transmit_sequence   = 0x4567u;
        root_.session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE;
        root_.session.last_accepted_receive_sequence  = 0x5678u;
        root_.session.accepted_receive_sequence_valid = 1u;
        root_.session.reliable_state                  = HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK;
        root_.session.retransmissions_committed       = 2u;
        root_.session.reliable_last_committed_ms      = 555u;
        root_.session.last_valid_receive_ms           = 444u;
        root_.session.last_failure                    = HIL_TRANSPORT_FAILURE_LINK_LOST;

        submitted_.fill( 0x11u );
        encoded_.fill( 0x22u );
        parser_.fill( 0x33u );
        codec_.fill( 0x44u );
        received_.fill( 0x55u );
        std::fill( std::begin( root_.control_output ), std::end( root_.control_output ), 0x66u );
        root_.submitted_message          = submitted_.data();
        root_.submitted_message_size     = 5u;
        root_.submitted_message_pending  = 1u;
        root_.encoded_output             = encoded_.data();
        root_.encoded_output_capacity    = encoded_.size();
        root_.encoded_output_size        = 7u;
        root_.output_selection           = HIL_TRANSPORT_MVP_OUTPUT_RELIABLE;
        root_.control_output_size        = 6u;
        root_.control_output_state       = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY;
        root_.parser.scratch_buffer      = parser_.data();
        root_.parser.scratch_buffer_size = parser_.size();
        root_.parser.accumulated_size    = 3u;
        root_.parser.body_ready          = 1u;
        root_.codec_scratch              = codec_.data();
        root_.codec_scratch_size         = codec_.size();
        root_.received_message           = received_.data();
        root_.received_message_size      = 4u;
        root_.received_message_pending   = 1u;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    }

    void ExpectFault() const
    {
        EXPECT_EQ( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
        EXPECT_EQ( root_.base.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
        EXPECT_EQ( root_.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT );
        EXPECT_EQ( root_.session.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
    }

    HIL_Transport_Mvp_Root_T      root_{};
    std::array<std::uint8_t, 16u> submitted_{};
    std::array<std::uint8_t, 24u> encoded_{};
    std::array<std::uint8_t, 16u> parser_{};
    std::array<std::uint8_t, 24u> codec_{};
    std::array<std::uint8_t, 16u> received_{};
};

TEST_F( TransportEventsTest, BasicLifecycleReportsPendingAndConsumesOneAtATime )
{
    std::uint8_t pending = 9u;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Get_Pending_Status( &root_, &pending ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending, 0u );

    HIL_Transport_Event_T destination = Event( 99u );
    const auto            sentinel    = destination;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &destination ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    ExpectEventEqual( destination, sentinel );

    const auto first  = Event( 0u );
    const auto second = Event( 1u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &first ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &second ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Get_Pending_Status( &root_, &pending ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending, 1u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &destination ), HIL_TRANSPORT_STATUS_OK );
    ExpectEventEqual( destination, first );
    EXPECT_EQ( root_.event_count, 1u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &destination ), HIL_TRANSPORT_STATUS_OK );
    ExpectEventEqual( destination, second );
    EXPECT_EQ( root_.event_count, 0u );
    EXPECT_EQ( root_.event_read_index, 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Get_Pending_Status( &root_, &pending ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending, 0u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &destination ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST_F( TransportEventsTest, FourDistinctEventsAreReadInPublicationOrder )
{
    for ( std::size_t index = 0u; index < HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY; ++index )
    {
        const auto event = Event( index );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &event ), HIL_TRANSPORT_STATUS_OK );
    }
    for ( std::size_t index = 0u; index < HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY; ++index )
    {
        HIL_Transport_Event_T actual{};
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
        ExpectEventEqual( actual, Event( index ) );
    }
}

TEST_F( TransportEventsTest, WraparoundRetainsFifoOrderAndEveryField )
{
    for ( std::size_t index = 0u; index < 4u; ++index )
    {
        const auto event = Event( index );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &event ), HIL_TRANSPORT_STATUS_OK );
    }
    for ( std::size_t index = 0u; index < 2u; ++index )
    {
        HIL_Transport_Event_T actual{};
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
        ExpectEventEqual( actual, Event( index ) );
    }
    for ( std::size_t index = 4u; index < 6u; ++index )
    {
        const auto event = Event( index );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &event ), HIL_TRANSPORT_STATUS_OK );
    }
    for ( std::size_t index = 2u; index < 6u; ++index )
    {
        HIL_Transport_Event_T actual{};
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
        ExpectEventEqual( actual, Event( index ) );
    }
}

TEST_F( TransportEventsTest, MultipleWrapCyclesPreserveOrdering )
{
    for ( std::size_t index = 0u; index < 3u; ++index )
    {
        const auto event = Event( index );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &event ), HIL_TRANSPORT_STATUS_OK );
    }
    for ( std::size_t index = 0u; index < 20u; ++index )
    {
        HIL_Transport_Event_T actual{};
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
        ExpectEventEqual( actual, Event( index ) );
        const auto replacement = Event( index + 3u );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &replacement ),
                   HIL_TRANSPORT_STATUS_OK );
    }
    for ( std::size_t index = 20u; index < 23u; ++index )
    {
        HIL_Transport_Event_T actual{};
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
        ExpectEventEqual( actual, Event( index ) );
    }
}

TEST_F( TransportEventsTest, FullQueueRejectsWithoutMutationAndReusesOneFreedSlot )
{
    for ( std::size_t index = 0u; index < 4u; ++index )
    {
        const auto event = Event( index );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &event ), HIL_TRANSPORT_STATUS_OK );
    }
    const auto slots_before = SnapshotEvents( root_ );
    const auto read_before  = root_.event_read_index;
    const auto count_before = root_.event_count;
    const auto rejected     = Event( 4u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &rejected ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    ExpectEventArrayEqual( SnapshotEvents( root_ ), slots_before );
    EXPECT_EQ( root_.event_read_index, read_before );
    EXPECT_EQ( root_.event_count, count_before );

    HIL_Transport_Event_T actual{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
    ExpectEventEqual( actual, Event( 0u ) );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &rejected ), HIL_TRANSPORT_STATUS_OK );
    for ( std::size_t index = 1u; index < 5u; ++index )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
        ExpectEventEqual( actual, Event( index ) );
    }
}

TEST_F( TransportEventsTest, PublicationCopiesCallerValueAndKeepsDuplicates )
{
    auto       source = Event( 5u );
    const auto copy   = source;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &source ), HIL_TRANSPORT_STATUS_OK );
    source = Event( 6u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &copy ), HIL_TRANSPORT_STATUS_OK );

    HIL_Transport_Event_T actual{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
    ExpectEventEqual( actual, copy );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
    ExpectEventEqual( actual, copy );
}

TEST_F( TransportEventsTest, InvalidArgumentsAndPublicationValuesDoNotMutateQueue )
{
    const auto valid = Event( 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &valid ), HIL_TRANSPORT_STATUS_OK );
    const auto slots_before = SnapshotEvents( root_ );
    const auto read_before  = root_.event_read_index;
    const auto count_before = root_.event_count;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Publish( nullptr, &valid ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Get_Pending_Status( &root_, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Reset( nullptr ), HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    std::uint8_t pending = 9u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Get_Pending_Status( nullptr, &pending ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( pending, 0u );

    auto invalid = valid;
    invalid.type = HIL_TRANSPORT_EVENT_NONE;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &invalid ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    invalid      = valid;
    invalid.type = InvalidEnum<HIL_Transport_Event_Type_T>( 99 );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &invalid ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    invalid        = valid;
    invalid.status = InvalidEnum<HIL_Transport_Status_T>( 99 );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &invalid ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    invalid         = valid;
    invalid.failure = InvalidEnum<HIL_Transport_Failure_T>( 99 );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &invalid ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    ExpectEventArrayEqual( SnapshotEvents( root_ ), slots_before );
    EXPECT_EQ( root_.event_read_index, read_before );
    EXPECT_EQ( root_.event_count, count_before );
}

TEST_F( TransportEventsTest, ReadDestinationIsPreservedOnEveryFailure )
{
    HIL_Transport_Event_T destination = Event( 42u );
    const auto            sentinel    = destination;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &destination ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    ExpectEventEqual( destination, sentinel );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( nullptr, &destination ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    ExpectEventEqual( destination, sentinel );
    root_.event_count = HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY + 1u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &destination ),
               HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    ExpectEventEqual( destination, sentinel );
}

TEST_F( TransportEventsTest, ResetCanonicalizesEveryQueueShapeWithoutClearingSlots )
{
    struct QueueShape
    {
        std::size_t read_index;
        std::size_t count;
    };
    constexpr std::array<QueueShape, 6u> Shapes{ QueueShape{ 0u, 0u }, QueueShape{ 0u, 1u },
                                                 QueueShape{ 1u, 2u }, QueueShape{ 3u, 2u },
                                                 QueueShape{ 0u, 4u }, QueueShape{ 99u, 99u } };

    for ( const auto& shape : Shapes )
    {
        for ( std::size_t index = 0u; index < 4u; ++index )
        {
            root_.event_queue[index] = Event( index + 10u );
        }
        root_.event_read_index  = shape.read_index;
        root_.event_count       = shape.count;
        const auto slots_before = SnapshotEvents( root_ );
        const auto unrelated    = SnapshotUnrelated( root_ );

        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( root_.event_read_index, 0u );
        EXPECT_EQ( root_.event_count, 0u );
        ExpectEventArrayEqual( SnapshotEvents( root_ ), slots_before );
        ExpectUnrelatedEqual( root_, unrelated );
        std::uint8_t pending = 1u;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Get_Pending_Status( &root_, &pending ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( pending, 0u );
        HIL_Transport_Event_T destination = Event( 50u );
        EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &destination ),
                   HIL_TRANSPORT_STATUS_NOT_READY );
    }
}

TEST_F( TransportEventsTest, MetadataCorruptionEntersFaultWithoutRepairOrExposure )
{
    constexpr std::array<std::pair<std::size_t, std::size_t>, 3u> Cases{
        std::pair<std::size_t, std::size_t>{ HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY, 0u },
        std::pair<std::size_t, std::size_t>{ HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY + 1u, 0u },
        std::pair<std::size_t, std::size_t>{ 0u, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY + 1u } };
    for ( const auto& test_case : Cases )
    {
        SetUp();
        root_.event_read_index            = test_case.first;
        root_.event_count                 = test_case.second;
        HIL_Transport_Event_T destination = Event( 60u );
        const auto            sentinel    = destination;
        EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &destination ),
                   HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
        ExpectEventEqual( destination, sentinel );
        ExpectFault();
        EXPECT_EQ( root_.event_read_index, test_case.first );
        EXPECT_EQ( root_.event_count, test_case.second );
    }
}

TEST_F( TransportEventsTest, CorruptOccupiedValuesEnterFaultAndAreNeverExposed )
{
    enum class Field
    {
        Type,
        Status,
        Failure
    };
    for ( const auto field : { Field::Type, Field::Status, Field::Failure } )
    {
        SetUp();
        root_.event_queue[0] = Event( 0u );
        root_.event_count    = 1u;
        if ( field == Field::Type )
        {
            root_.event_queue[0].type = InvalidEnum<HIL_Transport_Event_Type_T>( 99 );
        }
        else if ( field == Field::Status )
        {
            root_.event_queue[0].status = InvalidEnum<HIL_Transport_Status_T>( 99 );
        }
        else
        {
            root_.event_queue[0].failure = InvalidEnum<HIL_Transport_Failure_T>( 99 );
        }
        HIL_Transport_Event_T destination = Event( 61u );
        const auto            sentinel    = destination;
        EXPECT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &destination ),
                   HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
        ExpectEventEqual( destination, sentinel );
        ExpectFault();
        EXPECT_EQ( root_.event_read_index, 0u );
        EXPECT_EQ( root_.event_count, 1u );
    }
}

TEST_F( TransportEventsTest, InvalidStaleBytesOutsideOccupiedRangeAreIgnored )
{
    for ( auto& event : root_.event_queue )
    {
        event.type    = InvalidEnum<HIL_Transport_Event_Type_T>( 99 );
        event.status  = InvalidEnum<HIL_Transport_Status_T>( 99 );
        event.failure = InvalidEnum<HIL_Transport_Failure_T>( 99 );
    }
    root_.event_queue[2]   = Event( 2u );
    root_.event_read_index = 2u;
    root_.event_count      = 1u;
    std::uint8_t pending   = 0u;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Get_Pending_Status( &root_, &pending ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending, 1u );
    HIL_Transport_Event_T actual{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
    ExpectEventEqual( actual, Event( 2u ) );
    EXPECT_EQ( root_.event_read_index, 0u );
    EXPECT_EQ( root_.event_count, 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Get_Pending_Status( &root_, &pending ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending, 0u );
}

TEST_F( TransportEventsTest, EventOperationsPreserveAllUnrelatedSubsystemState )
{
    const auto expected = SnapshotUnrelated( root_ );
    const auto first    = Event( 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &first ), HIL_TRANSPORT_STATUS_OK );
    ExpectUnrelatedEqual( root_, expected );
    std::uint8_t pending = 0u;
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Get_Pending_Status( &root_, &pending ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending, 1u );
    ExpectUnrelatedEqual( root_, expected );
    HIL_Transport_Event_T actual{};
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Read( &root_, &actual ), HIL_TRANSPORT_STATUS_OK );
    ExpectUnrelatedEqual( root_, expected );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &root_, &first ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    ExpectUnrelatedEqual( root_, expected );
    EXPECT_TRUE( std::all_of( submitted_.begin(), submitted_.end(),
                              []( std::uint8_t byte ) { return byte == 0x11u; } ) );
    EXPECT_TRUE( std::all_of( encoded_.begin(), encoded_.end(),
                              []( std::uint8_t byte ) { return byte == 0x22u; } ) );
    EXPECT_TRUE( std::all_of( parser_.begin(), parser_.end(),
                              []( std::uint8_t byte ) { return byte == 0x33u; } ) );
    EXPECT_TRUE( std::all_of( codec_.begin(), codec_.end(),
                              []( std::uint8_t byte ) { return byte == 0x44u; } ) );
    EXPECT_TRUE( std::all_of( received_.begin(), received_.end(),
                              []( std::uint8_t byte ) { return byte == 0x55u; } ) );
}

struct PublicFixture
{
    PublicFixture()
    {
        HIL_TRANSPORT_Default_Config( &config );
        config.session_seed = UINT64_C( 0xABCDEF );
        EXPECT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_LE( required_size, workspace.size() );
        storage = HIL_Transport_Storage_T{ workspace.data(), required_size };
        EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &storage ),
                   HIL_TRANSPORT_STATUS_OK );
        root = static_cast<HIL_Transport_Mvp_Root_T*>( context.implementation );
    }

    HIL_Transport_Config_T config{};
    std::size_t            required_size{};
    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, 4096u> workspace{};
    HIL_Transport_Storage_T   storage{};
    HIL_Transport_Context_T   context{};
    HIL_Transport_Mvp_Root_T* root{};
};

TEST( TransportEventsPublic, ReadsFifoReportsPendingAndExplicitResetClearsAll )
{
    PublicFixture fixture;
    ASSERT_NE( fixture.root, nullptr );
    const auto first  = Event( 0u );
    const auto second = Event( 1u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( fixture.root, &first ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( fixture.root, &second ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Status_Snapshot_T status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &fixture.context, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.event_pending, 1u );

    HIL_Transport_Event_T actual{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &fixture.context, &actual ), HIL_TRANSPORT_STATUS_OK );
    ExpectEventEqual( actual, first );
    EXPECT_EQ( fixture.root->event_count, 1u );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &fixture.context, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.event_pending, 1u );
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &fixture.context, &actual ), HIL_TRANSPORT_STATUS_OK );
    ExpectEventEqual( actual, second );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &fixture.context, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.event_pending, 0u );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( fixture.root, &first ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( fixture.root, &second ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Reset( &fixture.context ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( fixture.root->event_read_index, 0u );
    EXPECT_EQ( fixture.root->event_count, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &fixture.context, &actual ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportEventsPublic, InvalidArgumentsAndCorruptionPreserveOutputs )
{
    PublicFixture fixture;
    ASSERT_NE( fixture.root, nullptr );
    HIL_Transport_Event_T   event    = Event( 12u );
    const auto              sentinel = event;
    HIL_Transport_Context_T invalid_context{};
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &invalid_context, &event ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    ExpectEventEqual( event, sentinel );
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &fixture.context, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    fixture.root->event_read_index = HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY;
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &fixture.context, &event ),
               HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    ExpectEventEqual( event, sentinel );
    EXPECT_EQ( fixture.root->base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );

    HIL_Transport_Status_Snapshot_T status{};
    status.role          = HIL_TRANSPORT_ROLE_RIG;
    status.event_pending = 1u;
    EXPECT_EQ( HIL_TRANSPORT_Get_Status( &fixture.context, &status ),
               HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    EXPECT_EQ( status.role, HIL_TRANSPORT_ROLE_HOST );
    EXPECT_EQ( status.event_pending, 0u );
}

TEST( TransportEventsStorage, RequiredSizeIncludesEmbeddedQueueAndExactCapacityInitializes )
{
    HIL_Transport_Config_T config{};
    HIL_TRANSPORT_Default_Config( &config );
    config.session_seed       = UINT64_C( 0x123456 );
    std::size_t required_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_OK );
    const std::size_t expected_size =
        sizeof( HIL_Transport_Mvp_Root_T ) + config.max_application_message_size
        + config.max_encoded_frame_size + ( config.max_encoded_frame_size - 1u )
        + config.max_application_message_size + HIL_TRANSPORT_MVP_RAW_OVERHEAD
        + config.max_application_message_size;
    EXPECT_EQ( required_size, expected_size );
    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, 4096u> workspace{};
    ASSERT_LE( required_size, workspace.size() );
    EXPECT_EQ( reinterpret_cast<std::uintptr_t>( workspace.data() )
                   % HIL_TRANSPORT_WORKSPACE_ALIGNMENT,
               0u );
    HIL_Transport_Context_T context{};
    HIL_Transport_Storage_T exact{ workspace.data(), required_size };
    EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &exact ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_NE( context.implementation, nullptr );
    auto* root = static_cast<HIL_Transport_Mvp_Root_T*>( context.implementation );
    EXPECT_EQ( root->event_read_index, 0u );
    EXPECT_EQ( root->event_count, 0u );

    HIL_Transport_Context_T too_small_context{};
    HIL_Transport_Storage_T too_small{ workspace.data(), required_size - 1u };
    EXPECT_EQ(
        HIL_TRANSPORT_Init( &too_small_context, HIL_TRANSPORT_ROLE_HOST, &config, &too_small ),
        HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( too_small_context.implementation, nullptr );
}

}  // namespace
