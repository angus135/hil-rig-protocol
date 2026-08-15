#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "hil_rig_protocol/transport/transport.h"
#include "transport/internal/mvp/transport_events_mvp.h"
#include "transport/internal/mvp/transport_frame_codec_mvp.h"
#include "transport/internal/common/transport_parser.h"
#include "transport/internal/mvp/transport_reliability_mvp.h"
#include "transport/internal/mvp/transport_session_mvp.h"
#include "transport/internal/mvp/transport_types_mvp.h"
}

namespace {

constexpr std::size_t   ApplicationCapacity = 32u;
constexpr std::size_t   EncodedCapacity     = 96u;
constexpr std::uint64_t SessionIdentifier   = 77u;

struct Harness
{
    HIL_Transport_Mvp_Root_T                                                       root{};
    std::array<std::uint8_t, ApplicationCapacity>                                  submitted{};
    std::array<std::uint8_t, EncodedCapacity>                                      encoded{};
    std::array<std::uint8_t, EncodedCapacity - 1u>                                 parser{};
    std::array<std::uint8_t, ApplicationCapacity + HIL_TRANSPORT_MVP_RAW_OVERHEAD> codec{};
    std::array<std::uint8_t, ApplicationCapacity>                                  received{};
    HIL_Transport_Context_T                                                        context{};

    void InitializeEstablished( std::uint16_t initial_sequence = 10u, std::uint8_t max_retries = 2u,
                                HIL_Transport_Role_T role = HIL_TRANSPORT_ROLE_HOST )
    {
        root                                          = {};
        root.base.config.max_application_message_size = ApplicationCapacity;
        root.base.config.max_encoded_frame_size       = EncodedCapacity;
        root.base.config.session_seed                 = role == HIL_TRANSPORT_ROLE_HOST
                                                            ? SessionIdentifier
                                                            : HIL_TRANSPORT_SESSION_SEED_INVALID;
        root.base.config.initial_reliable_sequence    = initial_sequence;
        root.base.config.retransmit_timeout_ms        = 10u;
        root.base.config.max_retries                  = max_retries;
        root.base.role                                = role;
        root.base.link_state                          = HIL_TRANSPORT_LINK_STATE_CONNECTED;
        root.base.session_state                       = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
        root.base.last_failure                        = HIL_TRANSPORT_FAILURE_NONE;

        ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Init( &root.session, role,
                                                   role == HIL_TRANSPORT_ROLE_HOST
                                                       ? SessionIdentifier
                                                       : HIL_TRANSPORT_SESSION_SEED_INVALID,
                                                   initial_sequence ),
                   HIL_TRANSPORT_STATUS_OK );
        root.session.link_state               = HIL_TRANSPORT_LINK_STATE_CONNECTED;
        root.session.link_state_observed      = 1u;
        root.session.state                    = HIL_TRANSPORT_SESSION_STATE_ESTABLISHED;
        root.session.handshake_phase          = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED;
        root.session.session_identifier       = SessionIdentifier;
        root.session.session_identifier_valid = 1u;
        root.session.last_failure             = HIL_TRANSPORT_FAILURE_NONE;

        root.submitted_message       = submitted.data();
        root.encoded_output          = encoded.data();
        root.encoded_output_capacity = encoded.size();
        ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &root.parser, parser.data(), parser.size() ),
                   HIL_TRANSPORT_STATUS_OK );
        root.codec_scratch      = codec.data();
        root.codec_scratch_size = codec.size();
        root.received_message   = received.data();
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Reset( &root ), HIL_TRANSPORT_STATUS_OK );

        context.implementation        = &root;
        context.implementation_size   = sizeof( root );
        context.initialization_cookie = HIL_TRANSPORT_INTERNAL_INITIALIZATION_COOKIE;
    }

    void SetSessionState( HIL_Transport_Session_State_T state )
    {
        root.base.session_state = state;
        root.session.state      = state;
        if ( state != HIL_TRANSPORT_SESSION_STATE_ESTABLISHED )
        {
            root.session.handshake_phase = HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_INACTIVE;
        }
    }
};

std::array<std::uint8_t, EncodedCapacity> EncodeAck( std::uint64_t session_identifier,
                                                     std::uint16_t acknowledgement_sequence,
                                                     std::size_t*  output_size )
{
    std::array<std::uint8_t, ApplicationCapacity + HIL_TRANSPORT_MVP_RAW_OVERHEAD> raw{};
    std::array<std::uint8_t, EncodedCapacity>                                      output{};
    const HIL_Transport_Mvp_Frame_T frame{ HIL_TRANSPORT_MVP_FRAME_ACK,
                                           session_identifier,
                                           0u,
                                           acknowledgement_sequence,
                                           nullptr,
                                           0u };

    EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &frame, ApplicationCapacity, raw.data(), raw.size(),
                                               output.data(), output.size(), output_size ),
               HIL_TRANSPORT_STATUS_OK );
    return output;
}

HIL_Transport_Mvp_Frame_T
PeekDecodeApplication( Harness& harness, std::array<std::uint8_t, ApplicationCapacity>* payload,
                       std::size_t* payload_size )
{
    std::array<std::uint8_t, EncodedCapacity>                                      output{};
    std::array<std::uint8_t, ApplicationCapacity + HIL_TRANSPORT_MVP_RAW_OVERHEAD> raw{};
    HIL_Transport_Mvp_Frame_T                                                      frame{};
    HIL_Transport_Mvp_Decode_Result_T decode_result = HIL_TRANSPORT_MVP_DECODE_MALFORMED;
    std::size_t                       output_size   = 0u;

    EXPECT_EQ(
        HIL_TRANSPORT_Peek_Output( &harness.context, output.data(), output.size(), &output_size ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_GT( output_size, 1u );
    EXPECT_EQ( output[output_size - 1u], 0u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Decode_Frame( output.data(), output_size - 1u, raw.data(),
                                               raw.size(), &frame, payload->data(), payload->size(),
                                               payload_size, &decode_result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( decode_result, HIL_TRANSPORT_MVP_DECODE_VALID );
    return frame;
}

void SubmitPeekCommit( Harness& harness, const std::array<std::uint8_t, 4u>& payload,
                       std::uint32_t now_ms )
{
    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &harness.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    std::array<std::uint8_t, ApplicationCapacity> decoded_payload{};
    std::size_t                                   decoded_size = 0u;
    ( void )PeekDecodeApplication( harness, &decoded_payload, &decoded_size );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, now_ms ), HIL_TRANSPORT_STATUS_OK );
}

HIL_Transport_Status_T FeedAck( Harness& harness, std::uint64_t session_identifier,
                                std::uint16_t acknowledgement_sequence,
                                std::size_t*  bytes_consumed = nullptr )
{
    std::size_t encoded_size = 0u;
    auto        encoded  = EncodeAck( session_identifier, acknowledgement_sequence, &encoded_size );
    std::size_t consumed = 0u;
    const auto  status =
        HIL_TRANSPORT_Receive_Bytes( &harness.context, encoded.data(), encoded_size, &consumed );
    EXPECT_EQ( consumed, encoded_size );
    if ( bytes_consumed != nullptr )
    {
        *bytes_consumed = consumed;
    }
    return status;
}

void FillEventQueue( Harness& harness )
{
    for ( std::size_t index = 0u; index < HIL_TRANSPORT_MVP_EVENT_QUEUE_CAPACITY; ++index )
    {
        const HIL_Transport_Event_T event{ HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED,
                                           HIL_TRANSPORT_STATUS_OK, HIL_TRANSPORT_FAILURE_NONE,
                                           index };
        ASSERT_EQ( HIL_TRANSPORT_MVP_Events_Publish( &harness.root, &event ),
                   HIL_TRANSPORT_STATUS_OK );
    }
}

TEST( TransportApplication, ValidatesInputAndRequiresEstablishedSession )
{
    Harness harness;
    harness.InitializeEstablished();
    const std::array<std::uint8_t, 1u>                       payload{ 0xA5u };
    const std::array<std::uint8_t, ApplicationCapacity + 1u> oversized{};

    EXPECT_EQ( HIL_TRANSPORT_Submit_Application_Data( nullptr, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_Submit_Application_Data( &harness.context, nullptr, 1u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_Submit_Application_Data( &harness.context, payload.data(), 0u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_Submit_Application_Data( &harness.context, oversized.data(),
                                                      oversized.size() ),
               HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE );

    for ( const auto state :
          { HIL_TRANSPORT_SESSION_STATE_DISCONNECTED, HIL_TRANSPORT_SESSION_STATE_CONNECTING,
            HIL_TRANSPORT_SESSION_STATE_RECOVERING, HIL_TRANSPORT_SESSION_STATE_FAULT } )
    {
        harness.InitializeEstablished();
        harness.SetSessionState( state );
        EXPECT_EQ( HIL_TRANSPORT_Submit_Application_Data( &harness.context, payload.data(),
                                                          payload.size() ),
                   HIL_TRANSPORT_STATUS_NOT_READY );
        EXPECT_EQ( HIL_TRANSPORT_Submit_Application_Data( &harness.context, oversized.data(),
                                                          oversized.size() ),
                   HIL_TRANSPORT_STATUS_NOT_READY );
        EXPECT_EQ( harness.root.submitted_message_pending, 0u );
        EXPECT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    }
}

TEST( TransportApplication, CopiesEncodesAndRetainsCallerPayloadForReliableDelivery )
{
    Harness harness;
    harness.InitializeEstablished();
    std::array<std::uint8_t, 4u> payload{ 0x10u, 0x00u, 0x20u, 0x30u };
    const auto                   original = payload;

    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &harness.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    payload.fill( 0xEEu );

    EXPECT_EQ( harness.root.submitted_message_pending, 1u );
    EXPECT_EQ( harness.root.submitted_message_size, original.size() );
    EXPECT_EQ( std::memcmp( harness.root.submitted_message, original.data(), original.size() ), 0 );
    EXPECT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_READY );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 10u );

    std::array<std::uint8_t, ApplicationCapacity> decoded_payload{};
    std::size_t                                   decoded_size = 0u;
    const auto frame = PeekDecodeApplication( harness, &decoded_payload, &decoded_size );
    EXPECT_EQ( frame.type, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    EXPECT_EQ( frame.session_identifier, SessionIdentifier );
    EXPECT_EQ( frame.sequence, 10u );
    EXPECT_EQ( frame.acknowledgement_sequence, 0u );
    EXPECT_EQ( decoded_size, original.size() );
    EXPECT_EQ( std::memcmp( decoded_payload.data(), original.data(), original.size() ), 0 );
}

TEST( TransportApplication, AcceptsConfiguredMaximumPayloadSize )
{
    Harness harness;
    harness.InitializeEstablished();
    std::array<std::uint8_t, ApplicationCapacity> payload{};
    for ( std::size_t index = 0u; index < payload.size(); ++index )
    {
        payload[index] = static_cast<std::uint8_t>( index );
    }

    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &harness.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    std::array<std::uint8_t, ApplicationCapacity> decoded_payload{};
    std::size_t                                   decoded_size = 0u;
    const auto frame = PeekDecodeApplication( harness, &decoded_payload, &decoded_size );

    EXPECT_EQ( frame.type, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    EXPECT_EQ( decoded_size, payload.size() );
    EXPECT_EQ( decoded_payload, payload );
}

TEST( TransportApplication, RejectsSecondSubmissionThroughoutReliableOwnership )
{
    Harness harness;
    harness.InitializeEstablished();
    const std::array<std::uint8_t, 4u> first{ 1u, 2u, 3u, 4u };
    const std::array<std::uint8_t, 4u> second{ 5u, 6u, 7u, 8u };

    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &harness.context, first.data(), first.size() ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &harness.context, second.data(), second.size() ),
        HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );

    std::array<std::uint8_t, ApplicationCapacity> decoded_payload{};
    std::size_t                                   decoded_size = 0u;
    ( void )PeekDecodeApplication( harness, &decoded_payload, &decoded_size );
    EXPECT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &harness.context, second.data(), second.size() ),
        HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );

    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, 100u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &harness.context, second.data(), second.size() ),
        HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );

    ASSERT_EQ( HIL_TRANSPORT_Process( &harness.context, 110u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
    EXPECT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &harness.context, second.data(), second.size() ),
        HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
}

TEST( TransportApplication, ExactAckCompletesDeliveryAdvancesSequenceAndPublishesOneEvent )
{
    Harness harness;
    harness.InitializeEstablished();
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );

    ASSERT_EQ( FeedAck( harness, SessionIdentifier, 10u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.submitted_message_pending, 0u );
    EXPECT_EQ( harness.root.submitted_message_size, 0u );
    EXPECT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 11u );

    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );
    EXPECT_EQ( event.status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.failure, HIL_TRANSPORT_FAILURE_NONE );
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportApplication, StaleAckDoesNotCompleteActiveDelivery )
{
    Harness harness;
    harness.InitializeEstablished();
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );

    ASSERT_EQ( FeedAck( harness, SessionIdentifier, 9u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.submitted_message_pending, 1u );
    EXPECT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 10u );

    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportApplication, DuplicateAckCannotPublishASecondDeliveryConfirmation )
{
    Harness harness;
    harness.InitializeEstablished();
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );

    ASSERT_EQ( FeedAck( harness, SessionIdentifier, 10u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );

    ASSERT_EQ( FeedAck( harness, SessionIdentifier, 10u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 11u );
    EXPECT_EQ( harness.root.submitted_message_pending, 0u );
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportApplication, DuplicateApplicationAckDoesNotResetEstablishedRigSession )
{
    Harness harness;
    harness.InitializeEstablished( 10u, 2u, HIL_TRANSPORT_ROLE_RIG );
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );

    ASSERT_EQ( FeedAck( harness, SessionIdentifier, 10u ), HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( event.type, HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED );

    ASSERT_EQ( FeedAck( harness, SessionIdentifier, 10u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( harness.root.session.state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( harness.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_ESTABLISHED );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 11u );
    EXPECT_EQ( harness.root.submitted_message_pending, 0u );

    std::size_t delivery_confirmed_events = 0u;
    std::size_t protocol_error_events     = 0u;
    std::size_t session_reset_events      = 0u;
    while ( HIL_TRANSPORT_Read_Event( &harness.context, &event ) == HIL_TRANSPORT_STATUS_OK )
    {
        if ( event.type == HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED )
        {
            ++delivery_confirmed_events;
        }
        else if ( event.type == HIL_TRANSPORT_EVENT_PROTOCOL_ERROR )
        {
            ++protocol_error_events;
        }
        else if ( event.type == HIL_TRANSPORT_EVENT_SESSION_RESET )
        {
            ++session_reset_events;
        }
    }
    EXPECT_EQ( delivery_confirmed_events, 0u );
    EXPECT_EQ( protocol_error_events, 1u );
    EXPECT_EQ( session_reset_events, 0u );
}

TEST( TransportApplication, LateAckCancelsAnUnpinnedRetry )
{
    Harness harness;
    harness.InitializeEstablished();
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );

    ASSERT_EQ( HIL_TRANSPORT_Process( &harness.context, 110u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_READY );
    ASSERT_EQ( FeedAck( harness, SessionIdentifier, 10u ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 11u );
}

TEST( TransportApplication, ExactAckForPinnedRetryIsRetainedUntilRetryCommit )
{
    Harness harness;
    harness.InitializeEstablished();
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );

    ASSERT_EQ( HIL_TRANSPORT_Process( &harness.context, 110u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    std::array<std::uint8_t, EncodedCapacity> retry{};
    std::size_t                               retry_size = 0u;
    ASSERT_EQ(
        HIL_TRANSPORT_Peek_Output( &harness.context, retry.data(), retry.size(), &retry_size ),
        HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_RETRANSMIT_PEEKED );

    EXPECT_EQ( FeedAck( harness, SessionIdentifier, 10u ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( harness.root.parser.body_ready, 1u );
    EXPECT_EQ( harness.root.submitted_message_pending, 1u );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 10u );

    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, 111u ), HIL_TRANSPORT_STATUS_OK );
    std::size_t consumed = 123u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &harness.context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, 0u );
    EXPECT_EQ( harness.root.parser.body_ready, 0u );
    EXPECT_EQ( harness.root.submitted_message_pending, 0u );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 11u );
}

TEST( TransportApplication, FullEventQueueDefersExactAckWithoutChangingDeliveryOwnership )
{
    Harness harness;
    harness.InitializeEstablished();
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );
    FillEventQueue( harness );

    EXPECT_EQ( FeedAck( harness, SessionIdentifier, 10u ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( harness.root.parser.body_ready, 1u );
    EXPECT_EQ( harness.root.submitted_message_pending, 1u );
    EXPECT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_AWAITING_ACK );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 10u );

    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ), HIL_TRANSPORT_STATUS_OK );
    std::size_t consumed = 1u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &harness.context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, 0u );
    EXPECT_EQ( harness.root.submitted_message_pending, 0u );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 11u );

    bool found_confirmation = false;
    while ( HIL_TRANSPORT_Read_Event( &harness.context, &event ) == HIL_TRANSPORT_STATUS_OK )
    {
        if ( event.type == HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED )
        {
            found_confirmation = true;
        }
    }
    EXPECT_TRUE( found_confirmation );
}

TEST( TransportApplication, RetryExhaustionReportsFailureAndStartsFreshSessionRecovery )
{
    Harness harness;
    harness.InitializeEstablished( 10u, 0u );
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );

    EXPECT_EQ( HIL_TRANSPORT_Process( &harness.context, 110u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_DELIVERY_FAILED );
    EXPECT_EQ( harness.root.submitted_message_pending, 0u );
    EXPECT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( harness.root.base.last_failure, HIL_TRANSPORT_FAILURE_DELIVERY );
    EXPECT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &harness.context, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_NOT_READY );

    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_DELIVERY_FAILED );
    EXPECT_EQ( event.status, HIL_TRANSPORT_STATUS_DELIVERY_FAILED );
    EXPECT_EQ( event.failure, HIL_TRANSPORT_FAILURE_DELIVERY );
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_SESSION_RESET );
    EXPECT_EQ( event.failure, HIL_TRANSPORT_FAILURE_DELIVERY );
}

TEST( TransportApplication, RetryExhaustionWaitsForFailureEventCapacityBeforeAbandoningSession )
{
    Harness harness;
    harness.InitializeEstablished( 10u, 0u );
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );
    FillEventQueue( harness );

    EXPECT_EQ( HIL_TRANSPORT_Process( &harness.context, 110u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_EXHAUSTED );
    EXPECT_EQ( harness.root.submitted_message_pending, 1u );

    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( HIL_TRANSPORT_Process( &harness.context, 111u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_DELIVERY_FAILED );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( harness.root.submitted_message_pending, 0u );

    std::size_t failure_events = 0u;
    while ( HIL_TRANSPORT_Read_Event( &harness.context, &event ) == HIL_TRANSPORT_STATUS_OK )
    {
        if ( event.type == HIL_TRANSPORT_EVENT_DELIVERY_FAILED )
        {
            ++failure_events;
        }
    }
    EXPECT_EQ( failure_events, 1u );
}

TEST( TransportApplication, AcknowledgementAdvancesTransmitSequenceAcrossUint16Wrap )
{
    Harness harness;
    harness.InitializeEstablished( UINT16_MAX );
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );

    ASSERT_EQ( FeedAck( harness, SessionIdentifier, UINT16_MAX ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.session.next_transmit_sequence, 0u );
}

TEST( TransportApplication, ResetAndLinkLossReleasePendingApplicationOwnership )
{
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };

    Harness reset_harness;
    reset_harness.InitializeEstablished();
    ASSERT_EQ( HIL_TRANSPORT_Submit_Application_Data( &reset_harness.context, payload.data(),
                                                      payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Reset( &reset_harness.context ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( reset_harness.root.submitted_message_pending, 0u );
    EXPECT_EQ( reset_harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );

    Harness link_harness;
    link_harness.InitializeEstablished();
    ASSERT_EQ( HIL_TRANSPORT_Submit_Application_Data( &link_harness.context, payload.data(),
                                                      payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    const auto status = HIL_TRANSPORT_Notify_Link_State(
        &link_harness.context, HIL_TRANSPORT_LINK_STATE_DISCONNECTED, 200u );
    EXPECT_TRUE( ( status == HIL_TRANSPORT_STATUS_OK )
                 || ( status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED ) );
    EXPECT_EQ( link_harness.root.submitted_message_pending, 0u );
    EXPECT_EQ( link_harness.root.session.reliable_state, HIL_TRANSPORT_MVP_RELIABLE_IDLE );
    EXPECT_EQ( link_harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
}

}  // namespace
