#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "hil_rig_protocol/transport/transport.h"
#include "transport/internal/mvp/transport_control_output_mvp.h"
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

std::array<std::uint8_t, EncodedCapacity>
EncodeApplication( std::uint64_t session_identifier, std::uint16_t sequence,
                   const std::uint8_t* payload, std::size_t payload_size, std::size_t* output_size )
{
    std::array<std::uint8_t, ApplicationCapacity + HIL_TRANSPORT_MVP_RAW_OVERHEAD> raw{};
    std::array<std::uint8_t, EncodedCapacity>                                      output{};
    const HIL_Transport_Mvp_Frame_T frame{ HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE,
                                           session_identifier,
                                           sequence,
                                           0u,
                                           payload,
                                           payload_size };

    EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &frame, ApplicationCapacity, raw.data(), raw.size(),
                                               output.data(), output.size(), output_size ),
               HIL_TRANSPORT_STATUS_OK );
    return output;
}

HIL_Transport_Status_T FeedApplication( Harness& harness, std::uint64_t session_identifier,
                                        std::uint16_t sequence, const std::uint8_t* payload,
                                        std::size_t  payload_size,
                                        std::size_t* bytes_consumed = nullptr )
{
    std::size_t encoded_size = 0u;
    auto        encoded =
        EncodeApplication( session_identifier, sequence, payload, payload_size, &encoded_size );
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

void PrimeAcceptedReceive( Harness& harness, std::uint16_t sequence,
                           HIL_Transport_Mvp_Frame_Type_T frame_type )
{
    ASSERT_EQ( HIL_TRANSPORT_MVP_Session_Accept_Sequence( &harness.root.session, sequence ),
               HIL_TRANSPORT_STATUS_OK );
    harness.root.session.last_accepted_receive_frame_type               = frame_type;
    harness.root.session.last_accepted_receive_acknowledgement_sequence = 0u;
}

HIL_Transport_Mvp_Frame_T PeekDecodeOutput( Harness&                                       harness,
                                            std::array<std::uint8_t, ApplicationCapacity>* payload,
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
    EXPECT_EQ( HIL_TRANSPORT_MVP_Decode_Frame( output.data(), output_size - 1u, raw.data(),
                                               raw.size(), &frame, payload->data(), payload->size(),
                                               payload_size, &decode_result ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( decode_result, HIL_TRANSPORT_MVP_DECODE_VALID );
    return frame;
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

TEST( TransportApplication, RigRetryExhaustionPublishesResetBeforeWaitingForNewInitiate )
{
    Harness harness;
    harness.InitializeEstablished( 10u, 0u, HIL_TRANSPORT_ROLE_RIG );
    const std::array<std::uint8_t, 4u> payload{ 1u, 2u, 3u, 4u };
    SubmitPeekCommit( harness, payload, 100u );

    EXPECT_EQ( HIL_TRANSPORT_Process( &harness.context, 110u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_DELIVERY_FAILED );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( harness.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );
    EXPECT_EQ( harness.root.submitted_message_pending, 0u );

    std::array<std::uint8_t, ApplicationCapacity> decoded_payload{};
    std::size_t                                   decoded_size = 0u;
    const auto reset = PeekDecodeOutput( harness, &decoded_payload, &decoded_size );
    EXPECT_EQ( reset.type, HIL_TRANSPORT_MVP_FRAME_RESET );
    EXPECT_EQ( reset.session_identifier, SessionIdentifier );
    EXPECT_EQ( reset.sequence, 0u );
    EXPECT_EQ( reset.acknowledgement_sequence, 0u );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, 111u ), HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( HIL_TRANSPORT_Process( &harness.context, 112u, HIL_TRANSPORT_OPERATING_MODE_NORMAL ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_CONNECTING );
    EXPECT_EQ( harness.root.session.handshake_phase,
               HIL_TRANSPORT_MVP_HANDSHAKE_PHASE_WAITING_FOR_INITIATE );
    EXPECT_EQ( harness.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
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

TEST( TransportApplication, ExpectedInboundMessageIsRetainedAckedAndReadOnce )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, 20u, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    const std::array<std::uint8_t, 4u> payload{ 0x11u, 0x22u, 0x33u, 0x44u };

    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.received_message_pending, 1u );
    EXPECT_EQ( harness.root.received_message_size, payload.size() );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 22u );
    EXPECT_EQ( harness.root.session.last_accepted_receive_sequence, 21u );
    EXPECT_EQ( harness.root.session.last_accepted_receive_frame_type,
               HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );

    std::array<std::uint8_t, ApplicationCapacity> decoded{};
    std::size_t                                   decoded_size = 0u;
    auto ack = PeekDecodeOutput( harness, &decoded, &decoded_size );
    EXPECT_EQ( ack.type, HIL_TRANSPORT_MVP_FRAME_ACK );
    EXPECT_EQ( ack.acknowledgement_sequence, 21u );
    EXPECT_EQ( decoded_size, 0u );

    std::size_t message_size = 999u;
    EXPECT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, nullptr, 0u, &message_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( message_size, payload.size() );
    EXPECT_EQ( harness.root.received_message_pending, 1u );

    std::array<std::uint8_t, 2u> small{ 0xAAu, 0xBBu };
    EXPECT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, small.data(), small.size(),
                                                    &message_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( small[0], 0xAAu );
    EXPECT_EQ( harness.root.received_message_pending, 1u );

    std::array<std::uint8_t, ApplicationCapacity> output{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &message_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( message_size, payload.size() );
    EXPECT_TRUE( std::equal( payload.begin(), payload.end(), output.begin() ) );
    EXPECT_EQ( harness.root.received_message_pending, 0u );
    EXPECT_EQ( harness.root.received_message_size, 0u );

    message_size = 999u;
    EXPECT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &message_size ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( message_size, 0u );
}

TEST( TransportApplication, DuplicateInboundMessageIsReackedWithoutRedeliveryBeforeOrAfterRead )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, 20u, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    const std::array<std::uint8_t, 3u> payload{ 1u, 2u, 3u };

    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.received_message_pending, 1u );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 22u );

    std::array<std::uint8_t, ApplicationCapacity> output{};
    std::size_t                                   message_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &message_size ),
               HIL_TRANSPORT_STATUS_OK );
    std::array<std::uint8_t, ApplicationCapacity> ack_payload{};
    std::size_t                                   ack_payload_size = 0u;
    ( void )PeekDecodeOutput( harness, &ack_payload, &ack_payload_size );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, 10u ), HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.received_message_pending, 0u );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 22u );
    EXPECT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &message_size ),
               HIL_TRANSPORT_STATUS_NOT_READY );

    std::array<std::uint8_t, ApplicationCapacity> decoded{};
    std::size_t                                   decoded_size = 0u;
    auto ack = PeekDecodeOutput( harness, &decoded, &decoded_size );
    EXPECT_EQ( ack.type, HIL_TRANSPORT_MVP_FRAME_ACK );
    EXPECT_EQ( ack.acknowledgement_sequence, 21u );
}

TEST( TransportApplication, DuplicateSequenceFromDifferentSemanticFrameIsIncompatible )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, 21u, HIL_TRANSPORT_MVP_FRAME_CONFIRM );
    const std::array<std::uint8_t, 2u> payload{ 7u, 8u };

    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.received_message_pending, 0u );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );

    bool                  protocol_error = false;
    bool                  session_reset  = false;
    HIL_Transport_Event_T event{};
    while ( HIL_TRANSPORT_Read_Event( &harness.context, &event ) == HIL_TRANSPORT_STATUS_OK )
    {
        protocol_error = protocol_error || event.type == HIL_TRANSPORT_EVENT_PROTOCOL_ERROR;
        session_reset  = session_reset || event.type == HIL_TRANSPORT_EVENT_SESSION_RESET;
    }
    EXPECT_TRUE( protocol_error );
    EXPECT_TRUE( session_reset );
}

TEST( TransportApplication, UnreadMessageBackpressureRetainsNextFrameUntilRead )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, 20u, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    const std::array<std::uint8_t, 3u> first{ 1u, 2u, 3u };
    const std::array<std::uint8_t, 4u> second{ 4u, 5u, 6u, 7u };

    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 21u, first.data(), first.size() ),
               HIL_TRANSPORT_STATUS_OK );
    std::array<std::uint8_t, ApplicationCapacity> first_ack_payload{};
    std::size_t                                   first_ack_payload_size = 0u;
    ( void )PeekDecodeOutput( harness, &first_ack_payload, &first_ack_payload_size );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, 1u ), HIL_TRANSPORT_STATUS_OK );

    EXPECT_EQ( FeedApplication( harness, SessionIdentifier, 22u, second.data(), second.size() ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( harness.root.parser.body_ready, 1u );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 22u );
    EXPECT_EQ( harness.root.received_message_size, first.size() );
    EXPECT_TRUE( std::equal( first.begin(), first.end(), harness.received.begin() ) );
    EXPECT_EQ( harness.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );

    std::array<std::uint8_t, ApplicationCapacity> output{};
    std::size_t                                   message_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &message_size ),
               HIL_TRANSPORT_STATUS_OK );
    std::size_t consumed = 99u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &harness.context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( consumed, 0u );
    EXPECT_EQ( harness.root.parser.body_ready, 0u );
    EXPECT_EQ( harness.root.received_message_pending, 1u );
    EXPECT_EQ( harness.root.received_message_size, second.size() );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 23u );
    EXPECT_TRUE( std::equal( second.begin(), second.end(), harness.received.begin() ) );
}

TEST( TransportApplication, DifferentControlOutputDefersExpectedInboundMessageWithoutCommit )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, 20u, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    std::size_t unrelated_size = 0u;
    auto        unrelated      = EncodeAck( SessionIdentifier, 99u, &unrelated_size );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &harness.root, unrelated.data(),
                                                                 unrelated_size ),
               HIL_TRANSPORT_STATUS_OK );
    const std::array<std::uint8_t, 2u> payload{ 9u, 10u };

    EXPECT_EQ( FeedApplication( harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( harness.root.parser.body_ready, 1u );
    EXPECT_EQ( harness.root.received_message_pending, 0u );
    EXPECT_EQ( harness.root.received_message_size, 0u );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 21u );

    std::array<std::uint8_t, ApplicationCapacity> unrelated_payload{};
    std::size_t                                   unrelated_payload_size = 0u;
    ( void )PeekDecodeOutput( harness, &unrelated_payload, &unrelated_payload_size );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, 2u ), HIL_TRANSPORT_STATUS_OK );
    std::size_t consumed = 99u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &harness.context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.received_message_pending, 1u );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 22u );
}

TEST( TransportApplication, ReceiveSequenceWrapDeliversEachMessageOnce )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, UINT16_MAX - 1u, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    const std::array<std::uint8_t, 1u>            first{ 0xAAu };
    const std::array<std::uint8_t, 1u>            second{ 0xBBu };
    std::array<std::uint8_t, ApplicationCapacity> output{};
    std::size_t                                   size = 0u;

    ASSERT_EQ(
        FeedApplication( harness, SessionIdentifier, UINT16_MAX, first.data(), first.size() ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 0u );
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &size ),
               HIL_TRANSPORT_STATUS_OK );
    std::array<std::uint8_t, ApplicationCapacity> wrap_ack_payload{};
    std::size_t                                   wrap_ack_payload_size = 0u;
    ( void )PeekDecodeOutput( harness, &wrap_ack_payload, &wrap_ack_payload_size );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, 1u ), HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 0u, second.data(), second.size() ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 1u );
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output[0], second[0] );
}

TEST( TransportApplication, PreviousSessionApplicationIsRejectedWithoutResettingEstablishedSession )
{
    Harness harness;
    harness.InitializeEstablished();
    const std::array<std::uint8_t, 1u> payload{ 3u };

    ASSERT_EQ(
        FeedApplication( harness, SessionIdentifier - 1u, 5u, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_ESTABLISHED );
    EXPECT_EQ( harness.root.received_message_pending, 0u );
    HIL_Transport_Event_T event{};
    ASSERT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( event.type, HIL_TRANSPORT_EVENT_PROTOCOL_ERROR );
    EXPECT_EQ( HIL_TRANSPORT_Read_Event( &harness.context, &event ),
               HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportApplication, UnrelatedSessionApplicationTriggersMandatoryRecovery )
{
    Harness harness;
    harness.InitializeEstablished();
    const std::array<std::uint8_t, 2u> payload{ 3u, 4u };

    ASSERT_EQ(
        FeedApplication( harness, SessionIdentifier + 10u, 5u, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );

    EXPECT_EQ( harness.root.received_message_pending, 0u );
    EXPECT_EQ( harness.root.received_message_size, 0u );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( harness.root.session.state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( harness.root.parser.body_ready, 0u );
    EXPECT_EQ( harness.root.parser.accumulated_size, 0u );

    bool                  protocol_error = false;
    bool                  session_reset  = false;
    HIL_Transport_Event_T event{};
    while ( HIL_TRANSPORT_Read_Event( &harness.context, &event ) == HIL_TRANSPORT_STATUS_OK )
    {
        protocol_error = protocol_error || event.type == HIL_TRANSPORT_EVENT_PROTOCOL_ERROR;
        session_reset  = session_reset || event.type == HIL_TRANSPORT_EVENT_SESSION_RESET;
    }
    EXPECT_TRUE( protocol_error );
    EXPECT_TRUE( session_reset );

    std::array<std::uint8_t, ApplicationCapacity> decoded{};
    std::size_t                                   decoded_size = 0u;
    const auto reset = PeekDecodeOutput( harness, &decoded, &decoded_size );
    EXPECT_EQ( reset.type, HIL_TRANSPORT_MVP_FRAME_RESET );
    EXPECT_EQ( reset.session_identifier, SessionIdentifier );
    EXPECT_EQ( reset.sequence, 0u );
    EXPECT_EQ( reset.acknowledgement_sequence, 0u );
}

TEST( TransportApplication, IncompatibleApplicationSequenceAbandonsSession )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, 20u, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    const std::array<std::uint8_t, 1u> payload{ 3u };

    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 23u, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( harness.root.received_message_pending, 0u );
}

TEST( TransportApplication, LocalReliableOutputRemainsUnchangedWhilePeerMessageGeneratesAck )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, 20u, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    const std::array<std::uint8_t, 4u> local{ 1u, 2u, 3u, 4u };
    const std::array<std::uint8_t, 3u> peer{ 5u, 6u, 7u };

    ASSERT_EQ(
        HIL_TRANSPORT_Submit_Application_Data( &harness.context, local.data(), local.size() ),
        HIL_TRANSPORT_STATUS_OK );
    const auto reliable_before   = harness.encoded;
    const auto reliable_size     = harness.root.encoded_output_size;
    const auto reliable_sequence = harness.root.session.retained_transmit_sequence;

    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 21u, peer.data(), peer.size() ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.encoded_output_size, reliable_size );
    EXPECT_EQ( harness.root.session.retained_transmit_sequence, reliable_sequence );
    EXPECT_EQ( harness.encoded, reliable_before );
    EXPECT_EQ( harness.root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );
    EXPECT_EQ( harness.root.received_message_pending, 1u );
}

TEST( TransportApplication, ResetAndLinkLossReleaseUnreadApplicationOwnership )
{
    const std::array<std::uint8_t, 2u> payload{ 1u, 2u };

    Harness reset_harness;
    reset_harness.InitializeEstablished();
    ASSERT_EQ(
        FeedApplication( reset_harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( HIL_TRANSPORT_Reset( &reset_harness.context ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( reset_harness.root.received_message_pending, 0u );
    EXPECT_EQ( reset_harness.root.received_message_size, 0u );

    Harness link_harness;
    link_harness.InitializeEstablished();
    ASSERT_EQ(
        FeedApplication( link_harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
        HIL_TRANSPORT_STATUS_OK );
    const auto status = HIL_TRANSPORT_Notify_Link_State(
        &link_harness.context, HIL_TRANSPORT_LINK_STATE_DISCONNECTED, 5u );
    EXPECT_TRUE( status == HIL_TRANSPORT_STATUS_OK
                 || status == HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( link_harness.root.received_message_pending, 0u );
    EXPECT_EQ( link_harness.root.received_message_size, 0u );
}

TEST( TransportApplication, ReadApplicationValidatesArgumentsAndPrivateOwnershipInvariant )
{
    Harness harness;
    harness.InitializeEstablished();
    std::array<std::uint8_t, ApplicationCapacity> output{};
    std::size_t                                   size = 77u;

    EXPECT_EQ( HIL_TRANSPORT_Read_Application_Data( nullptr, output.data(), output.size(), &size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( size, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, nullptr, 1u, &size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( size, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    harness.root.received_message_pending = 1u;
    harness.root.received_message_size    = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &size ),
               HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
}

TEST( TransportApplication, GetStatusRejectsCorruptUnreadMessageMetadata )
{
    const auto expect_invalid_metadata_fault = []( Harness& harness ) {
        HIL_Transport_Status_Snapshot_T status{};
        status.application_message_pending = 1u;

        EXPECT_EQ( HIL_TRANSPORT_Get_Status( &harness.context, &status ),
                   HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
        EXPECT_EQ( status.application_message_pending, 0u );
        EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
        EXPECT_EQ( harness.root.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT );
    };

    {
        Harness harness;
        harness.InitializeEstablished();
        harness.root.received_message_pending = 2u;
        harness.root.received_message_size    = 1u;
        expect_invalid_metadata_fault( harness );
    }

    {
        Harness harness;
        harness.InitializeEstablished();
        harness.root.received_message_pending = 0u;
        harness.root.received_message_size    = 1u;
        expect_invalid_metadata_fault( harness );
    }

    {
        Harness harness;
        harness.InitializeEstablished();
        harness.root.received_message_pending = 1u;
        harness.root.received_message_size    = 0u;
        expect_invalid_metadata_fault( harness );
    }

    {
        Harness harness;
        harness.InitializeEstablished();
        harness.root.received_message_pending = 1u;
        harness.root.received_message_size    = ApplicationCapacity + 1u;
        expect_invalid_metadata_fault( harness );
    }
}

TEST( TransportApplication, AcceptsMaximumSizedInboundPayload )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, 20u, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    std::array<std::uint8_t, ApplicationCapacity> payload{};
    for ( std::size_t i = 0u; i < payload.size(); ++i )
    {
        payload[i] = static_cast<std::uint8_t>( i );
    }

    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_OK );
    HIL_Transport_Status_Snapshot_T status{};
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &harness.context, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.application_message_pending, 1u );

    std::array<std::uint8_t, ApplicationCapacity> output{};
    std::size_t                                   size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( size, payload.size() );
    EXPECT_EQ( output, payload );
    ASSERT_EQ( HIL_TRANSPORT_Get_Status( &harness.context, &status ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( status.application_message_pending, 0u );
}

TEST( TransportApplication, OlderNonDuplicateApplicationSequenceAbandonsSession )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, 20u, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    const std::array<std::uint8_t, 1u> first{ 1u };
    const std::array<std::uint8_t, 1u> old{ 2u };
    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 21u, first.data(), first.size() ),
               HIL_TRANSPORT_STATUS_OK );
    std::array<std::uint8_t, ApplicationCapacity> ack_payload{};
    std::size_t                                   ack_payload_size = 0u;
    ( void )PeekDecodeOutput( harness, &ack_payload, &ack_payload_size );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, 1u ), HIL_TRANSPORT_STATUS_OK );
    std::array<std::uint8_t, ApplicationCapacity> output{};
    std::size_t                                   size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &size ),
               HIL_TRANSPORT_STATUS_OK );

    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 19u, old.data(), old.size() ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.base.session_state, HIL_TRANSPORT_SESSION_STATE_RECOVERING );
    EXPECT_EQ( harness.root.received_message_pending, 0u );
}

TEST( TransportApplication, ConflictingControlOutputDefersDuplicateWithoutRedelivery )
{
    Harness harness;
    harness.InitializeEstablished();
    PrimeAcceptedReceive( harness, 20u, HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE );
    const std::array<std::uint8_t, 2u> payload{ 4u, 5u };
    ASSERT_EQ( FeedApplication( harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_OK );

    std::array<std::uint8_t, ApplicationCapacity> ack_payload{};
    std::size_t                                   ack_payload_size = 0u;
    ( void )PeekDecodeOutput( harness, &ack_payload, &ack_payload_size );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, 1u ), HIL_TRANSPORT_STATUS_OK );

    std::array<std::uint8_t, ApplicationCapacity> output{};
    std::size_t                                   size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Read_Application_Data( &harness.context, output.data(), output.size(),
                                                    &size ),
               HIL_TRANSPORT_STATUS_OK );

    std::size_t unrelated_size = 0u;
    auto        unrelated      = EncodeAck( SessionIdentifier, 99u, &unrelated_size );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &harness.root, unrelated.data(),
                                                                 unrelated_size ),
               HIL_TRANSPORT_STATUS_OK );

    EXPECT_EQ( FeedApplication( harness, SessionIdentifier, 21u, payload.data(), payload.size() ),
               HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED );
    EXPECT_EQ( harness.root.parser.body_ready, 1u );
    EXPECT_EQ( harness.root.received_message_pending, 0u );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 22u );

    std::array<std::uint8_t, ApplicationCapacity> unrelated_payload{};
    std::size_t                                   unrelated_payload_size = 0u;
    ( void )PeekDecodeOutput( harness, &unrelated_payload, &unrelated_payload_size );
    ASSERT_EQ( HIL_TRANSPORT_Commit_Output( &harness.context, 2u ), HIL_TRANSPORT_STATUS_OK );
    std::size_t consumed = 99u;
    ASSERT_EQ( HIL_TRANSPORT_Receive_Bytes( &harness.context, nullptr, 0u, &consumed ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( harness.root.parser.body_ready, 0u );
    EXPECT_EQ( harness.root.received_message_pending, 0u );
    EXPECT_EQ( harness.root.session.expected_receive_sequence, 22u );

    std::array<std::uint8_t, ApplicationCapacity> duplicate_ack_payload{};
    std::size_t                                   duplicate_ack_payload_size = 0u;
    auto                                          duplicate_ack =
        PeekDecodeOutput( harness, &duplicate_ack_payload, &duplicate_ack_payload_size );
    EXPECT_EQ( duplicate_ack.type, HIL_TRANSPORT_MVP_FRAME_ACK );
    EXPECT_EQ( duplicate_ack.acknowledgement_sequence, 21u );
}

}  // namespace
