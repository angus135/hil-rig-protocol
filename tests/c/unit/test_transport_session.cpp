#include <gtest/gtest.h>

extern "C" {
#include "transport/internal/mvp/transport_events_mvp.h"
#include "transport/internal/mvp/transport_session_mvp.h"
}

#include <array>
#include <cstring>
#include <limits>

namespace {

TEST(TransportSessionInit, ValidatesWithoutMutation)
{
    HIL_Transport_Mvp_Session_T session;
    std::memset(&session, 0xA5, sizeof(session));
    const auto original = session;
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Init(nullptr, HIL_TRANSPORT_ROLE_HOST, 1u, 0u),
              HIL_TRANSPORT_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Init(&session, static_cast<HIL_Transport_Role_T>(99), 1u, 0u),
              HIL_TRANSPORT_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(0, std::memcmp(&session, &original, sizeof(session)));
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Init(&session, HIL_TRANSPORT_ROLE_HOST, 0u, 0u),
              HIL_TRANSPORT_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Init(&session, HIL_TRANSPORT_ROLE_HOST, UINT64_MAX, 0u),
              HIL_TRANSPORT_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Init(&session, HIL_TRANSPORT_ROLE_RIG, 1u, 0u),
              HIL_TRANSPORT_STATUS_INVALID_ARGUMENT);
}

TEST(TransportSessionInit, InitializesHostCompletely)
{
    HIL_Transport_Mvp_Session_T session;
    std::memset(&session, 0xA5, sizeof(session));
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Init(&session, HIL_TRANSPORT_ROLE_HOST, 7u, UINT16_MAX),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(session.role, HIL_TRANSPORT_ROLE_HOST);
    EXPECT_EQ(session.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED);
    EXPECT_EQ(session.link_state_observed, 0u);
    EXPECT_EQ(session.state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED);
    EXPECT_EQ(session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE);
    EXPECT_EQ(session.next_host_session_identifier, 7u);
    EXPECT_EQ(session.initial_reliable_sequence, UINT16_MAX);
    EXPECT_EQ(session.next_transmit_sequence, UINT16_MAX);
    EXPECT_EQ(session.expected_receive_sequence, UINT16_MAX);
    EXPECT_EQ(session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE);
    EXPECT_EQ(session.retained_reliable_frame_type, HIL_TRANSPORT_MVP_FRAME_INVALID);
    EXPECT_EQ(session.last_failure, HIL_TRANSPORT_FAILURE_NONE);
}

TEST(TransportSessionInit, InitializesRigWithZeroSequence)
{
    HIL_Transport_Mvp_Session_T session;
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Init(&session, HIL_TRANSPORT_ROLE_RIG, 0u, 0u),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(session.next_host_session_identifier, 0u);
    EXPECT_EQ(session.next_transmit_sequence, 0u);
}

struct RootFixture : testing::Test
{
    HIL_Transport_Mvp_Root_T root{};
    std::array<uint8_t, 16> submitted{};
    std::array<uint8_t, 32> parser{};
    std::array<uint8_t, 64> encoded{};
    std::array<uint8_t, 32> codec{};
    std::array<uint8_t, 16> received{};
    void SetUp() override
    {
        ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Init(&root.session, HIL_TRANSPORT_ROLE_HOST, 5u, 9u),
                  HIL_TRANSPORT_STATUS_OK);
        root.base.role = HIL_TRANSPORT_ROLE_HOST;
        root.base.config.max_encoded_frame_size = encoded.size();
        root.base.config.max_retries = 2u;
        root.base.link_state = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
        root.base.session_state = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        ASSERT_EQ(HIL_TRANSPORT_Parser_Init(&root.parser, parser.data(), parser.size()),
                  HIL_TRANSPORT_STATUS_OK);
        root.encoded_output = encoded.data();
        root.encoded_output_capacity = encoded.size();
        root.submitted_message = submitted.data();
        root.codec_scratch = codec.data();
        root.codec_scratch_size = codec.size();
        root.received_message = received.data();
        root.control_output_state = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE;
    }
};

TEST_F(RootFixture, FirstDisconnectedIsSilentAndIdempotent)
{
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_DISCONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(root.session.link_state_observed, 1u);
    EXPECT_EQ(root.event_count, 0u);
    EXPECT_EQ(root.base.last_failure, HIL_TRANSPORT_FAILURE_NONE);
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_DISCONNECTED),
              HIL_TRANSPORT_STATUS_OK);
}

TEST_F(RootFixture, ConnectedBeginsHostAndRepeatedCallDoesNotAdvance)
{
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(root.session.state, HIL_TRANSPORT_SESSION_STATE_CONNECTING);
    EXPECT_EQ(root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INITIATE_PENDING);
    EXPECT_EQ(root.session.session_identifier, 5u);
    EXPECT_EQ(root.session.next_host_session_identifier, 6u);
    EXPECT_EQ(root.event_count, 1u);
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(root.session.next_host_session_identifier, 6u);
    EXPECT_EQ(root.event_count, 1u);
}

TEST_F(RootFixture, HostCursorWrapsReservedValues)
{
    root.session.next_host_session_identifier = UINT64_MAX - 1u;
    root.session.link_state_observed = 1u;
    root.session.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root.base.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Begin_Establishment(&root), HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(root.session.session_identifier, UINT64_MAX - 1u);
    EXPECT_EQ(root.session.next_host_session_identifier, 1u);
}

TEST_F(RootFixture, BeginNeedsObservedConnectedLinkAndFaultsOnDirtyState)
{
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Begin_Establishment(&root), HIL_TRANSPORT_STATUS_NOT_READY);
    root.session.link_state_observed = 1u;
    root.session.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root.base.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root.output_selection = HIL_TRANSPORT_MVP_OUTPUT_CONTROL;
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Begin_Establishment(&root), HIL_TRANSPORT_STATUS_INTERNAL_ERROR);
    EXPECT_EQ(root.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT);
}

TEST_F(RootFixture, DisconnectPublishesOrderedEventsAndCleansWork)
{
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    HIL_Transport_Event_T ignored;
    ASSERT_EQ(HIL_TRANSPORT_MVP_Events_Read(&root, &ignored), HIL_TRANSPORT_STATUS_OK);
    root.submitted_message_pending = 1u;
    root.submitted_message_size = 4u;
    root.parser.accumulated_size = 3u;
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_DISCONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(root.submitted_message_pending, 0u);
    EXPECT_EQ(root.parser.accumulated_size, 0u);
    EXPECT_EQ(root.session.state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED);
    HIL_Transport_Event_T first{}, second{};
    ASSERT_EQ(HIL_TRANSPORT_MVP_Events_Read(&root, &first), HIL_TRANSPORT_STATUS_OK);
    ASSERT_EQ(HIL_TRANSPORT_MVP_Events_Read(&root, &second), HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(first.type, HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED);
    EXPECT_EQ(second.type, HIL_TRANSPORT_EVENT_SESSION_RESET);
    EXPECT_EQ(second.failure, HIL_TRANSPORT_FAILURE_LINK_LOST);
}

TEST_F(RootFixture, FullQueueCannotPreventDisconnectAndIsNotRetried)
{
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    root.event_count = HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY;
    for (auto &event : root.event_queue)
        event = {HIL_TRANSPORT_EVENT_PROTOCOL_ERROR, HIL_TRANSPORT_STATUS_NOT_READY,
                 HIL_TRANSPORT_FAILURE_PROTOCOL, 0u};
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_DISCONNECTED),
              HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED);
    EXPECT_EQ(root.session.state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED);
    EXPECT_EQ(root.event_count, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY);
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_DISCONNECTED),
              HIL_TRANSPORT_STATUS_OK);
}

TEST_F(RootFixture, ExplicitResetClearsEventsRepairsFaultAndPreservesCursor)
{
    root.session.link_state_observed          = 2u;
    root.session.role                         = HIL_TRANSPORT_ROLE_RIG;
    root.session.link_state                   = HIL_TRANSPORT_LINK_STATE_DISCONNECTED;
    root.base.link_state                      = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root.session.state                        = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.base.session_state                   = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.event_count                          = 99u;
    root.event_read_index                     = 99u;
    root.session.next_host_session_identifier = 42u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Explicit_Reset( &root ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root.event_count, 0u );
    EXPECT_EQ( root.event_read_index, 0u );
    EXPECT_EQ( root.session.role, HIL_TRANSPORT_ROLE_HOST );
    EXPECT_EQ( root.session.link_state, HIL_TRANSPORT_LINK_STATE_CONNECTED );
    EXPECT_EQ( root.session.link_state_observed, 1u );
    EXPECT_EQ( root.session.state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( root.base.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
    EXPECT_EQ( root.session.next_host_session_identifier, 42u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Begin_Establishment( &root ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root.session.state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( root.session.session_identifier, 42u );
    EXPECT_EQ( root.session.next_host_session_identifier, 43u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Begin_Establishment( &root ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( root.session.next_host_session_identifier, 43u );
}

TEST_F( RootFixture, ExplicitResetPreservesAValidUnobservedDisconnectedLink )
{
    root.session.state               = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.base.session_state          = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.session.last_failure        = HIL_TRANSPORT_FAILURE_INTERNAL;
    root.base.last_failure           = HIL_TRANSPORT_FAILURE_INTERNAL;
    root.session.link_state_observed = 0u;

    ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Explicit_Reset( &root ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root.session.link_state_observed, 0u );
    EXPECT_EQ( root.base.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( root.session.state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Begin_Establishment( &root ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST_F( RootFixture, ExplicitResetRejectsAnUnrecoverableHostIdentityCursor )
{
    root.session.state                        = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.base.session_state                   = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.session.last_failure                 = HIL_TRANSPORT_FAILURE_INTERNAL;
    root.base.last_failure                    = HIL_TRANSPORT_FAILURE_INTERNAL;
    root.session.link_state_observed          = 2u;
    root.session.next_host_session_identifier = HIL_TRANSPORT_SESSION_SEED_INVALID;
    root.event_count                          = 1u;
    root.event_queue[0] = { HIL_TRANSPORT_EVENT_PROTOCOL_ERROR, HIL_TRANSPORT_STATUS_NOT_READY,
                            HIL_TRANSPORT_FAILURE_PROTOCOL, 0u };
    root.submitted_message_pending = 1u;
    root.submitted_message_size    = 3u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Explicit_Reset( &root ),
               HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    EXPECT_EQ( root.event_count, 0u );
    EXPECT_EQ( root.session.link_state_observed, 1u );
    EXPECT_EQ( root.submitted_message_pending, 0u );
    EXPECT_EQ( root.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
    EXPECT_EQ( root.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT );
    EXPECT_EQ( root.base.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
    EXPECT_EQ( root.session.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
}

TEST_F(RootFixture, FaultNotificationUpdatesLinkButPreservesFault)
{
    root.session.state = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.base.session_state = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.session.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
    root.base.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(root.base.link_state, HIL_TRANSPORT_LINK_STATE_CONNECTED);
    EXPECT_EQ(root.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT);
    EXPECT_EQ(root.event_count, 0u);
}

TEST_F(RootFixture, RigConnectionWaitsForInitiateWithoutAnIdentity)
{
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Init(&root.session, HIL_TRANSPORT_ROLE_RIG, 0u, 13u),
              HIL_TRANSPORT_STATUS_OK);
    root.base.role = HIL_TRANSPORT_ROLE_RIG;
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING);
    EXPECT_EQ(root.session.handshake_phase, HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE);
    EXPECT_EQ(root.session.session_identifier, 0u);
    EXPECT_EQ(root.session.session_identifier_valid, 0u);
    EXPECT_EQ(root.session.next_host_session_identifier, 0u);
}

TEST_F(RootFixture, ReconnectionAllocatesNewIdentityAndNeverResumesOldWork)
{
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    const auto first_identity = root.session.session_identifier;
    root.session.next_transmit_sequence = 21u;
    root.session.expected_receive_sequence = 22u;
    root.session.last_accepted_receive_sequence = 21u;
    root.session.accepted_receive_sequence_valid = 1u;
    root.session.reliable_state = HIL_TRANSPORT_MVP_RELIABLE_READY;
    root.session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE;
    root.session.retained_transmit_sequence = 21u;
    root.encoded_output_size = 3u;
    root.control_output_state = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY;
    root.control_output_size = 2u;
    root.submitted_message_pending = 1u;
    root.submitted_message_size = 3u;
    root.received_message_pending = 1u;
    root.received_message_size = 4u;
    root.parser.accumulated_size = 5u;
    root.parser.discarding = 1u;

    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root,
                                                          HIL_TRANSPORT_LINK_STATE_DISCONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE);
    EXPECT_EQ(root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE);
    EXPECT_EQ(root.submitted_message_pending, 0u);
    EXPECT_EQ(root.received_message_pending, 0u);
    EXPECT_EQ(root.parser.accumulated_size, 0u);
    EXPECT_EQ(root.session.next_transmit_sequence, 9u);
    EXPECT_EQ(root.session.expected_receive_sequence, 9u);
    EXPECT_EQ(root.session.accepted_receive_sequence_valid, 0u);

    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_NE(root.session.session_identifier, first_identity);
    EXPECT_EQ(root.session.session_identifier, 6u);
    EXPECT_EQ(root.session.next_host_session_identifier, 7u);
}

TEST_F(RootFixture, OneRemainingEventSlotKeepsLinkEventAndStillDisconnects)
{
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    ASSERT_EQ(HIL_TRANSPORT_MVP_Events_Reset(&root), HIL_TRANSPORT_STATUS_OK);
    const HIL_Transport_Event_T older{HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,
                                      HIL_TRANSPORT_STATUS_NOT_READY,
                                      HIL_TRANSPORT_FAILURE_PROTOCOL, 0u};
    for (size_t index = 0u; index < HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY - 1u; ++index)
        ASSERT_EQ(HIL_TRANSPORT_MVP_Events_Publish(&root, &older), HIL_TRANSPORT_STATUS_OK);

    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root,
                                                          HIL_TRANSPORT_LINK_STATE_DISCONNECTED),
              HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED);
    EXPECT_EQ(root.base.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED);
    EXPECT_EQ(root.base.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED);
    EXPECT_EQ(root.base.last_failure, HIL_TRANSPORT_FAILURE_LINK_LOST);
    EXPECT_EQ(root.event_count, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY);
    HIL_Transport_Event_T event{};
    for (size_t index = 0u; index < HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY - 1u; ++index)
    {
        ASSERT_EQ(HIL_TRANSPORT_MVP_Events_Read(&root, &event), HIL_TRANSPORT_STATUS_OK);
        EXPECT_EQ(event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR);
    }
    ASSERT_EQ(HIL_TRANSPORT_MVP_Events_Read(&root, &event), HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(event.type, HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED);
    EXPECT_EQ(event.failure, HIL_TRANSPORT_FAILURE_LINK_LOST);
}

TEST_F(RootFixture, FullQueueOnConnectionStillBeginsAndDoesNotRetryPublication)
{
    const HIL_Transport_Event_T older{HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,
                                      HIL_TRANSPORT_STATUS_NOT_READY,
                                      HIL_TRANSPORT_FAILURE_PROTOCOL, 0u};
    for (size_t index = 0u; index < HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY; ++index)
        ASSERT_EQ(HIL_TRANSPORT_MVP_Events_Publish(&root, &older), HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED);
    EXPECT_EQ(root.session.state, HIL_TRANSPORT_SESSION_STATE_CONNECTING);
    EXPECT_EQ(root.session.session_identifier, 5u);
    EXPECT_EQ(root.event_count, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY);
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(root.session.next_host_session_identifier, 6u);
    EXPECT_EQ(root.event_count, HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY);
}

TEST_F(RootFixture, EventCorruptionWinsOverCapacityAndCleanupPreservesFault)
{
    ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root, HIL_TRANSPORT_LINK_STATE_CONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    root.event_count = HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY + 1u;
    root.submitted_message_pending = 1u;
    root.submitted_message_size = 2u;
    root.parser.accumulated_size = 3u;

    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root,
                                                          HIL_TRANSPORT_LINK_STATE_DISCONNECTED),
              HIL_TRANSPORT_STATUS_INTERNAL_ERROR);
    EXPECT_EQ(root.base.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED);
    EXPECT_EQ(root.session.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED);
    EXPECT_EQ(root.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT);
    EXPECT_EQ(root.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT);
    EXPECT_EQ(root.base.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL);
    EXPECT_EQ(root.session.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL);
    EXPECT_EQ(root.submitted_message_pending, 0u);
    EXPECT_EQ(root.parser.accumulated_size, 0u);
}

TEST_F(RootFixture, FaultDisconnectionReleasesOwnershipWithoutPublishingEvents)
{
    root.base.session_state = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.session.state = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.base.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
    root.session.last_failure = HIL_TRANSPORT_FAILURE_INTERNAL;
    root.submitted_message_pending = 1u;
    root.submitted_message_size = 4u;
    root.parser.accumulated_size = 3u;
    root.control_output_state = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED;
    root.control_output_size = 2u;
    root.output_selection = HIL_TRANSPORT_MVP_OUTPUT_CONTROL;

    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(&root,
                                                          HIL_TRANSPORT_LINK_STATE_DISCONNECTED),
              HIL_TRANSPORT_STATUS_OK);
    EXPECT_EQ(root.session.link_state_observed, 1u);
    EXPECT_EQ(root.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT);
    EXPECT_EQ(root.base.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL);
    EXPECT_EQ(root.submitted_message_pending, 0u);
    EXPECT_EQ(root.parser.accumulated_size, 0u);
    EXPECT_EQ(root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE);
    EXPECT_EQ(root.event_count, 0u);
}

TEST_F( RootFixture, FaultDisconnectionPropagatesInvalidOutputStorageCleanupFailure )
{
    root.base.session_state                   = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.session.state                        = HIL_TRANSPORT_SESSION_STATE_FAULT;
    root.base.last_failure                    = HIL_TRANSPORT_FAILURE_INTERNAL;
    root.session.last_failure                 = HIL_TRANSPORT_FAILURE_INTERNAL;
    root.submitted_message_pending            = 1u;
    root.submitted_message_size               = 4u;
    root.parser.accumulated_size              = 3u;
    root.session.reliable_state               = HIL_TRANSPORT_MVP_RELIABLE_READY;
    root.session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE;
    root.session.retained_transmit_sequence   = root.session.next_transmit_sequence;
    root.encoded_output_size                  = 3u;
    root.encoded_output                       = nullptr;

    EXPECT_EQ(
        HIL_TRANSPORT_MVP_Session_Notify_Link_State( &root, HIL_TRANSPORT_LINK_STATE_DISCONNECTED ),
        HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    EXPECT_EQ( root.base.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED );
    EXPECT_EQ( root.session.link_state, HIL_TRANSPORT_LINK_STATE_DISCONNECTED );
    EXPECT_EQ( root.session.link_state_observed, 1u );
    EXPECT_EQ( root.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
    EXPECT_EQ( root.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT );
    EXPECT_EQ( root.base.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
    EXPECT_EQ( root.session.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
    EXPECT_EQ( root.submitted_message_pending, 0u );
    EXPECT_EQ( root.parser.accumulated_size, 0u );
    EXPECT_EQ( root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( root.encoded_output_size, 0u );
    EXPECT_EQ( root.event_count, 0u );
}

TEST_F( RootFixture, AbandonCleanupFailureClearsOwnershipAndEntersFault )
{
    root.submitted_message_pending            = 1u;
    root.submitted_message_size               = 4u;
    root.parser.accumulated_size              = 3u;
    root.session.reliable_state               = HIL_TRANSPORT_MVP_RELIABLE_READY;
    root.session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE;
    root.session.retained_transmit_sequence   = root.session.next_transmit_sequence;
    root.encoded_output_size                  = 3u;
    root.encoded_output                       = nullptr;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Session_Abandon( &root, HIL_TRANSPORT_FAILURE_DELIVERY ),
               HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    EXPECT_EQ( root.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
    EXPECT_EQ( root.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT );
    EXPECT_EQ( root.base.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
    EXPECT_EQ( root.session.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
    EXPECT_EQ( root.submitted_message_pending, 0u );
    EXPECT_EQ( root.parser.accumulated_size, 0u );
    EXPECT_EQ( root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( root.encoded_output_size, 0u );
    EXPECT_EQ( root.event_count, 0u );
}

TEST_F( RootFixture, AbandonMapsEverySupportedFailureAndRejectsResetReasons )
{
    struct Mapping
    {
        HIL_Transport_Failure_T failure;
        HIL_Transport_Status_T status;
    };
    constexpr std::array<Mapping, 5> mappings{{
        {HIL_TRANSPORT_FAILURE_LINK_LOST, HIL_TRANSPORT_STATUS_NOT_READY},
        {HIL_TRANSPORT_FAILURE_CONNECTION_TIMEOUT, HIL_TRANSPORT_STATUS_TIMEOUT},
        {HIL_TRANSPORT_FAILURE_DELIVERY, HIL_TRANSPORT_STATUS_DELIVERY_FAILED},
        {HIL_TRANSPORT_FAILURE_PROTOCOL, HIL_TRANSPORT_STATUS_NOT_READY},
        {HIL_TRANSPORT_FAILURE_CAPACITY, HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED},
    }};
    root.session.link_state_observed = 1u;
    root.session.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root.base.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    for (const auto &mapping : mappings)
    {
        ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Abandon(&root, mapping.failure),
                  HIL_TRANSPORT_STATUS_OK);
        HIL_Transport_Event_T event{};
        ASSERT_EQ(HIL_TRANSPORT_MVP_Events_Read(&root, &event), HIL_TRANSPORT_STATUS_OK);
        EXPECT_EQ(event.type, HIL_TRANSPORT_EVENT_SESSION_RESET);
        EXPECT_EQ(event.status, mapping.status);
        EXPECT_EQ(event.failure, mapping.failure);
        EXPECT_EQ(root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING);
    }
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Abandon(&root, HIL_TRANSPORT_FAILURE_NONE),
              HIL_TRANSPORT_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Abandon(&root, HIL_TRANSPORT_FAILURE_LOCAL_RESET),
              HIL_TRANSPORT_STATUS_INVALID_ARGUMENT);
}

TEST_F(RootFixture, EveryReliableLifecycleIsReleasedWithoutClearingBackingBytes)
{
    constexpr std::array<HIL_Transport_Mvp_Reliable_State_T, 7> states{
        HIL_TRANSPORT_MVP_RELIABLE_IDLE,
        HIL_TRANSPORT_MVP_RELIABLE_READY,
        HIL_TRANSPORT_MVP_RELIABLE_PEEKED,
        HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK,
        HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY,
        HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED,
        HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED,
    };
    encoded.fill(0xA5u);
    const auto retained_bytes = encoded;
    root.session.link_state_observed = 1u;
    root.session.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root.base.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    for (const auto state : states)
    {
        root.session.reliable_state = state;
        if (state == HIL_TRANSPORT_MVP_RELIABLE_IDLE)
        {
            root.encoded_output_size = 0u;
            root.session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_INVALID;
            root.session.retained_transmit_sequence = 0u;
        }
        else
        {
            root.encoded_output_size = 3u;
            root.session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE;
            root.session.retained_transmit_sequence = root.session.next_transmit_sequence;
        }
        root.output_selection = (state == HIL_TRANSPORT_MVP_RELIABLE_PEEKED
                                 || state == HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED)
                                    ? HIL_TRANSPORT_MVP_OUTPUT_RELIABLE
                                    : HIL_TRANSPORT_MVP_OUTPUT_NONE;
        ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Abandon(&root, HIL_TRANSPORT_FAILURE_DELIVERY),
                  HIL_TRANSPORT_STATUS_OK);
        EXPECT_EQ(root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE);
        EXPECT_EQ(root.encoded_output_size, 0u);
        EXPECT_EQ(root.output_selection, HIL_TRANSPORT_MVP_OUTPUT_NONE);
        EXPECT_EQ(encoded, retained_bytes);
        ASSERT_EQ(HIL_TRANSPORT_MVP_Events_Reset(&root), HIL_TRANSPORT_STATUS_OK);
    }
}

TEST_F(RootFixture, ExplicitResetWorksFromEveryStateAndPreservesRetainedSetup)
{
    constexpr std::array<HIL_Transport_Session_State_T, 5> states{
        HIL_TRANSPORT_SESSION_STATE_DISCONNECTED,
        HIL_TRANSPORT_SESSION_STATE_CONNECTING,
        HIL_TRANSPORT_SESSION_STATE_ESTABLISHED,
        HIL_TRANSPORT_SESSION_STATE_RECOVERING,
        HIL_TRANSPORT_SESSION_STATE_FAULT,
    };
    root.session.link_state_observed = 1u;
    root.session.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root.base.link_state = HIL_TRANSPORT_LINK_STATE_CONNECTED;
    root.session.next_host_session_identifier = 44u;
    const auto* parser_pointer = root.parser.scratch_buffer;
    const auto parser_capacity = root.parser.scratch_buffer_size;
    const auto* output_pointer = root.encoded_output;
    const auto output_capacity = root.encoded_output_capacity;
    for (const auto state : states)
    {
        root.session.state = state;
        root.base.session_state = state;
        root.event_read_index = 99u;
        root.event_count = 99u;
        ASSERT_EQ(HIL_TRANSPORT_MVP_Session_Explicit_Reset(&root), HIL_TRANSPORT_STATUS_OK);
        EXPECT_EQ(root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING);
        EXPECT_EQ(root.session.state, HIL_TRANSPORT_SESSION_STATE_RECOVERING);
        EXPECT_EQ(root.base.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET);
        EXPECT_EQ(root.event_read_index, 0u);
        EXPECT_EQ(root.event_count, 0u);
        EXPECT_EQ(root.session.link_state_observed, 1u);
        EXPECT_EQ(root.session.next_host_session_identifier, 44u);
        EXPECT_EQ(root.parser.scratch_buffer, parser_pointer);
        EXPECT_EQ(root.parser.scratch_buffer_size, parser_capacity);
        EXPECT_EQ(root.encoded_output, output_pointer);
        EXPECT_EQ(root.encoded_output_capacity, output_capacity);
    }
}

TEST_F(RootFixture, PublicAdapterValidatesBeforeMutation)
{
    HIL_Transport_Context_T invalid_context{};
    EXPECT_EQ(HIL_TRANSPORT_Notify_Link_State(&invalid_context,
                                               HIL_TRANSPORT_LINK_STATE_CONNECTED, 123u),
              HIL_TRANSPORT_STATUS_INVALID_ARGUMENT);

    HIL_Transport_Context_T context{&root, sizeof(root),
                                    HIL_TRANSPORT_INTERNAL_INITIALIZATION_COOKIE};
    const auto original = root;
    EXPECT_EQ(HIL_TRANSPORT_Notify_Link_State(&context,
                                               static_cast<HIL_Transport_Link_State_T>(99), 123u),
              HIL_TRANSPORT_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(0, std::memcmp(&root, &original, sizeof(root)));
}

TEST_F(RootFixture, InvalidLinkDoesNotMutate)
{
    const auto original = root;
    EXPECT_EQ(HIL_TRANSPORT_MVP_Session_Notify_Link_State(
                  &root, static_cast<HIL_Transport_Link_State_T>(99)),
              HIL_TRANSPORT_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(0, std::memcmp(&root, &original, sizeof(root)));
}

} // namespace
