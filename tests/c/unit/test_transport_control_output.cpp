#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <gtest/gtest.h>

#include "hil_rig_protocol/transport/transport.h"
#include "transport/internal/mvp/transport_control_output_mvp.h"
#include "transport/internal/mvp/transport_frame_codec_mvp.h"

namespace {

constexpr std::array<std::uint8_t, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY> ControlBytes{
    0x21u, 0x00u, 0x43u, 0x65u, 0x87u, 0xA9u, 0xCBu, 0xEDu, 0x10u, 0x32u,
    0x54u, 0x76u, 0x98u, 0xBAu, 0xDCu, 0xFEu, 0x11u, 0x22u, 0x33u, 0x00u };
constexpr std::array<std::uint8_t, 8u> DifferentBytes{ 0x90u, 0x81u, 0x72u, 0x63u,
                                                       0x54u, 0x45u, 0x36u, 0x00u };
constexpr std::size_t                  RepresentativeSize = 8u;

using ControlArray = std::array<std::uint8_t, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY>;

ControlArray SnapshotControlBytes( const HIL_Transport_Mvp_Root_T& root )
{
    ControlArray result{};
    std::copy( std::begin( root.control_output ), std::end( root.control_output ), result.begin() );
    return result;
}

void ExpectControlBytes( const HIL_Transport_Mvp_Root_T& root, const ControlArray& expected )
{
    EXPECT_TRUE( std::equal( std::begin( root.control_output ), std::end( root.control_output ),
                             expected.begin() ) );
}

HIL_Transport_Mvp_Control_Output_State_T InvalidControlState()
{
    using StateStorage = std::underlying_type_t<HIL_Transport_Mvp_Control_Output_State_T>;
    static_assert( sizeof( StateStorage ) == sizeof( HIL_Transport_Mvp_Control_Output_State_T ) );
    const StateStorage                       invalid_value = 99;
    HIL_Transport_Mvp_Control_Output_State_T state         = HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE;
    std::memcpy( &state, &invalid_value, sizeof( state ) );
    return state;
}

struct IsolationSnapshot
{
    HIL_Transport_Config_T               config;
    HIL_Transport_Role_T                 base_role;
    HIL_Transport_Link_State_T           base_link;
    HIL_Transport_Session_State_T        base_state;
    HIL_Transport_Failure_T              base_failure;
    HIL_Transport_Role_T                 session_role;
    HIL_Transport_Link_State_T           session_link;
    HIL_Transport_Session_State_T        session_state;
    HIL_Transport_Failure_T              session_failure;
    HIL_Transport_Mvp_Handshake_Phase_T  handshake_phase;
    HIL_Transport_Mvp_Reliable_State_T   reliable_state;
    std::uint8_t*                        encoded_output;
    std::size_t                          encoded_output_capacity;
    std::size_t                          encoded_output_size;
    std::array<std::uint8_t, 24u>        reliable_bytes;
    HIL_Transport_Mvp_Frame_Type_T       retained_type;
    std::uint16_t                        retained_sequence;
    std::uint16_t                        next_sequence;
    std::uint16_t                        expected_sequence;
    std::uint8_t                         retries;
    std::uint32_t                        committed_ms;
    std::uint64_t                        session_identifier;
    std::uint8_t                         session_identifier_valid;
    std::uint64_t                        next_host_session_identifier;
    std::uint16_t                        initial_sequence;
    HIL_Transport_Parser_T               parser;
    std::uint8_t*                        submitted_message;
    std::size_t                          submitted_size;
    std::uint8_t                         submitted_pending;
    std::uint8_t*                        received_message;
    std::size_t                          received_size;
    std::uint8_t                         received_pending;
    HIL_Transport_Event_T                pending_event;
    std::uint8_t                         event_pending;
    HIL_Transport_Mvp_Output_Selection_T output_selection;
};

class TransportControlOutputTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        for ( std::size_t index = 0u; index < reliable_bytes_.size(); ++index )
        {
            reliable_bytes_[index] = static_cast<std::uint8_t>( 0xA0u + index );
        }
        parser_bytes_.fill( 0xB2u );
        submitted_bytes_.fill( 0xC3u );
        received_bytes_.fill( 0xD4u );

        root_.session.reliable_state               = HIL_TRANSPORT_MVP_RELIABLE_PEEKED;
        root_.session.retained_reliable_frame_type = HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE;
        root_.session.retained_transmit_sequence   = 0x1234u;
        root_.session.next_transmit_sequence       = 0x1234u;
        root_.session.expected_receive_sequence    = 0x5678u;
        root_.session.retransmissions_committed    = 2u;
        root_.session.reliable_last_committed_ms   = 0xABCDEF01u;
        root_.session.session_identifier           = UINT64_C( 0x1122334455667788 );
        root_.session.session_identifier_valid     = 1u;
        root_.encoded_output                       = reliable_bytes_.data();
        root_.encoded_output_capacity              = reliable_bytes_.size();
        root_.encoded_output_size                  = 17u;
        root_.output_selection                     = HIL_TRANSPORT_MVP_OUTPUT_RELIABLE;
        root_.parser.scratch_buffer                = parser_bytes_.data();
        root_.parser.scratch_buffer_size           = parser_bytes_.size();
        root_.parser.accumulated_size              = 3u;
        root_.parser.body_ready                    = 1u;
        root_.submitted_message                    = submitted_bytes_.data();
        root_.submitted_message_size               = 4u;
        root_.submitted_message_pending            = 1u;
        root_.received_message                     = received_bytes_.data();
        root_.received_message_size                = 5u;
        root_.received_message_pending             = 1u;
        root_.pending_event.type                   = HIL_TRANSPORT_EVENT_PROTOCOL_ERROR;
        root_.pending_event.status                 = HIL_TRANSPORT_STATUS_NOT_READY;
        root_.pending_event.failure                = HIL_TRANSPORT_FAILURE_PROTOCOL;
        root_.pending_event.required_capacity      = 99u;
        root_.event_pending                        = 1u;
    }

    void Publish( std::size_t size = RepresentativeSize )
    {
        ASSERT_EQ(
            HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, ControlBytes.data(), size ),
            HIL_TRANSPORT_STATUS_OK );
    }

    void Peek( std::size_t size = RepresentativeSize )
    {
        std::array<std::uint8_t, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY> output{};
        std::size_t                                                         output_size = 0u;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, output.data(),
                                                                 output.size(), &output_size ),
                   HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( output_size, size );
        ASSERT_TRUE( std::equal( output.begin(), output.begin() + size, ControlBytes.begin() ) );
    }

    IsolationSnapshot Snapshot() const
    {
        return { root_.base.config,
                 root_.base.role,
                 root_.base.link_state,
                 root_.base.session_state,
                 root_.base.last_failure,
                 root_.session.role,
                 root_.session.link_state,
                 root_.session.state,
                 root_.session.last_failure,
                 root_.session.handshake_phase,
                 root_.session.reliable_state,
                 root_.encoded_output,
                 root_.encoded_output_capacity,
                 root_.encoded_output_size,
                 reliable_bytes_,
                 root_.session.retained_reliable_frame_type,
                 root_.session.retained_transmit_sequence,
                 root_.session.next_transmit_sequence,
                 root_.session.expected_receive_sequence,
                 root_.session.retransmissions_committed,
                 root_.session.reliable_last_committed_ms,
                 root_.session.session_identifier,
                 root_.session.session_identifier_valid,
                 root_.session.next_host_session_identifier,
                 root_.session.initial_reliable_sequence,
                 root_.parser,
                 root_.submitted_message,
                 root_.submitted_message_size,
                 root_.submitted_message_pending,
                 root_.received_message,
                 root_.received_message_size,
                 root_.received_message_pending,
                 root_.pending_event,
                 root_.event_pending,
                 root_.output_selection };
    }

    void ExpectIsolationUnchanged( const IsolationSnapshot& expected ) const
    {
        EXPECT_EQ( root_.base.config.max_application_message_size,
                   expected.config.max_application_message_size );
        EXPECT_EQ( root_.base.config.max_encoded_frame_size,
                   expected.config.max_encoded_frame_size );
        EXPECT_EQ( root_.base.config.session_seed, expected.config.session_seed );
        EXPECT_EQ( root_.base.config.initial_reliable_sequence,
                   expected.config.initial_reliable_sequence );
        EXPECT_EQ( root_.base.config.connection_timeout_ms, expected.config.connection_timeout_ms );
        EXPECT_EQ( root_.base.config.retransmit_timeout_ms, expected.config.retransmit_timeout_ms );
        EXPECT_EQ( root_.base.config.max_retries, expected.config.max_retries );
        EXPECT_EQ( root_.base.role, expected.base_role );
        EXPECT_EQ( root_.base.link_state, expected.base_link );
        EXPECT_EQ( root_.base.session_state, expected.base_state );
        EXPECT_EQ( root_.base.last_failure, expected.base_failure );
        EXPECT_EQ( root_.session.role, expected.session_role );
        EXPECT_EQ( root_.session.link_state, expected.session_link );
        EXPECT_EQ( root_.session.state, expected.session_state );
        EXPECT_EQ( root_.session.last_failure, expected.session_failure );
        EXPECT_EQ( root_.session.handshake_phase, expected.handshake_phase );
        EXPECT_EQ( root_.session.reliable_state, expected.reliable_state );
        EXPECT_EQ( root_.encoded_output, expected.encoded_output );
        EXPECT_EQ( root_.encoded_output_capacity, expected.encoded_output_capacity );
        EXPECT_EQ( root_.encoded_output_size, expected.encoded_output_size );
        EXPECT_EQ( reliable_bytes_, expected.reliable_bytes );
        EXPECT_EQ( root_.session.retained_reliable_frame_type, expected.retained_type );
        EXPECT_EQ( root_.session.retained_transmit_sequence, expected.retained_sequence );
        EXPECT_EQ( root_.session.next_transmit_sequence, expected.next_sequence );
        EXPECT_EQ( root_.session.expected_receive_sequence, expected.expected_sequence );
        EXPECT_EQ( root_.session.retransmissions_committed, expected.retries );
        EXPECT_EQ( root_.session.reliable_last_committed_ms, expected.committed_ms );
        EXPECT_EQ( root_.session.session_identifier, expected.session_identifier );
        EXPECT_EQ( root_.session.session_identifier_valid, expected.session_identifier_valid );
        EXPECT_EQ( root_.session.next_host_session_identifier,
                   expected.next_host_session_identifier );
        EXPECT_EQ( root_.session.initial_reliable_sequence, expected.initial_sequence );
        EXPECT_EQ( root_.parser.scratch_buffer, expected.parser.scratch_buffer );
        EXPECT_EQ( root_.parser.scratch_buffer_size, expected.parser.scratch_buffer_size );
        EXPECT_EQ( root_.parser.accumulated_size, expected.parser.accumulated_size );
        EXPECT_EQ( root_.parser.body_ready, expected.parser.body_ready );
        EXPECT_EQ( root_.parser.discarding, expected.parser.discarding );
        EXPECT_EQ( root_.submitted_message, expected.submitted_message );
        EXPECT_EQ( root_.submitted_message_size, expected.submitted_size );
        EXPECT_EQ( root_.submitted_message_pending, expected.submitted_pending );
        EXPECT_EQ( root_.received_message, expected.received_message );
        EXPECT_EQ( root_.received_message_size, expected.received_size );
        EXPECT_EQ( root_.received_message_pending, expected.received_pending );
        EXPECT_EQ( root_.pending_event.type, expected.pending_event.type );
        EXPECT_EQ( root_.pending_event.status, expected.pending_event.status );
        EXPECT_EQ( root_.pending_event.failure, expected.pending_event.failure );
        EXPECT_EQ( root_.pending_event.required_capacity,
                   expected.pending_event.required_capacity );
        EXPECT_EQ( root_.event_pending, expected.event_pending );
        EXPECT_EQ( root_.output_selection, expected.output_selection );
    }

    void ExpectFault() const
    {
        EXPECT_EQ( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
        EXPECT_EQ( root_.session.state, HIL_TRANSPORT_SESSION_STATE_FAULT );
        EXPECT_EQ( root_.base.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
        EXPECT_EQ( root_.session.last_failure, HIL_TRANSPORT_FAILURE_INTERNAL );
    }

    HIL_Transport_Mvp_Root_T      root_{};
    std::array<std::uint8_t, 24u> reliable_bytes_{};
    std::array<std::uint8_t, 16u> parser_bytes_{};
    std::array<std::uint8_t, 16u> submitted_bytes_{};
    std::array<std::uint8_t, 16u> received_bytes_{};
};

TEST_F( TransportControlOutputTest, CapacityAndZeroInitializationAreFixedAndIdle )
{
    static_assert( HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY == 20u );
    HIL_Transport_Mvp_Root_T zero_root{};
    std::uint8_t             pending = 1u;

    EXPECT_EQ( zero_root.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( zero_root.control_output_size, 0u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( &zero_root, &pending ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending, 0u );
}

TEST( TransportControlOutputProfile, ExactWorkspaceInitializesControlAndOneByteLessFails )
{
    HIL_Transport_Config_T config{};
    HIL_TRANSPORT_Default_Config( &config );
    config.session_seed       = UINT64_C( 0x1234 );
    std::size_t required_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_OK );

    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, 4096u> workspace{};
    ASSERT_LE( required_size, workspace.size() );
    HIL_Transport_Context_T context{};
    HIL_Transport_Storage_T exact{ workspace.data(), required_size };
    ASSERT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &exact ),
               HIL_TRANSPORT_STATUS_OK );
    auto* root = static_cast<HIL_Transport_Mvp_Root_T*>( context.implementation );
    ASSERT_NE( root, nullptr );
    EXPECT_EQ( root->control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( root->control_output_size, 0u );

    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, 4096u> short_workspace{};
    HIL_Transport_Context_T                                                      short_context{};
    HIL_Transport_Storage_T short_storage{ short_workspace.data(), required_size - 1u };
    EXPECT_EQ(
        HIL_TRANSPORT_Init( &short_context, HIL_TRANSPORT_ROLE_HOST, &config, &short_storage ),
        HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
}

TEST( TransportControlOutputCapacity, ExistingCodecFitsAckAndReset )
{
    constexpr std::array<HIL_Transport_Mvp_Frame_Type_T, 2u> Types{ HIL_TRANSPORT_MVP_FRAME_ACK,
                                                                    HIL_TRANSPORT_MVP_FRAME_RESET };
    for ( const auto type : Types )
    {
        HIL_Transport_Mvp_Frame_T frame{
            type,
            UINT64_C( 0x0102030405060708 ),
            0u,
            static_cast<std::uint16_t>( type == HIL_TRANSPORT_MVP_FRAME_ACK ? 0xBEEFu : 0u ),
            nullptr,
            0u };
        std::array<std::uint8_t, HIL_TRANSPORT_MVP_RAW_OVERHEAD>            raw{};
        std::array<std::uint8_t, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY> encoded{};
        std::size_t                                                         encoded_size = 0u;
        EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &frame, 1u, raw.data(), raw.size(),
                                                   encoded.data(), encoded.size(), &encoded_size ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_GT( encoded_size, 0u );
        EXPECT_LE( encoded_size, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_CAPACITY );
        EXPECT_EQ( encoded[encoded_size - 1u], 0u );
    }
}

TEST_F( TransportControlOutputTest, PublicationValidatesArgumentsBeforeState )
{
    std::fill( std::begin( root_.control_output ), std::end( root_.control_output ), 0x5Au );
    const auto original = SnapshotControlBytes( root_ );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( nullptr, ControlBytes.data(),
                                                                 RepresentativeSize ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ(
        HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, nullptr, RepresentativeSize ),
        HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, ControlBytes.data(), 0u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, ControlBytes.data(), 21u ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( root_.control_output_size, 0u );
    ExpectControlBytes( root_, original );

    root_.control_output_state = InvalidControlState();
    EXPECT_EQ(
        HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, nullptr, RepresentativeSize ),
        HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_NE( root_.base.session_state, HIL_TRANSPORT_SESSION_STATE_FAULT );
    EXPECT_EQ( root_.control_output_state, InvalidControlState() );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
}

TEST_F( TransportControlOutputTest, PublishesOwnedCopiesAtAllAcceptedSizes )
{
    constexpr std::array<std::size_t, 3u> Sizes{ 1u, RepresentativeSize, 20u };
    for ( const std::size_t size : Sizes )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
        auto source = ControlBytes;
        ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, source.data(), size ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );
        EXPECT_EQ( root_.control_output_size, size );
        EXPECT_TRUE(
            std::equal( root_.control_output, root_.control_output + size, source.begin() ) );
        source.fill( 0xEEu );
        EXPECT_TRUE(
            std::equal( root_.control_output, root_.control_output + size, ControlBytes.begin() ) );
    }

    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    {
        std::array<std::uint8_t, 3u> temporary{ 7u, 8u, 9u };
        ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, temporary.data(),
                                                                     temporary.size() ),
                   HIL_TRANSPORT_STATUS_OK );
    }
    EXPECT_EQ( root_.control_output[0], 7u );
    EXPECT_EQ( root_.control_output[1], 8u );
    EXPECT_EQ( root_.control_output[2], 9u );
}

TEST_F( TransportControlOutputTest, PublicationSupportsExactAndPartialSourceOverlap )
{
    std::copy( ControlBytes.begin(), ControlBytes.end(), root_.control_output );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, root_.control_output,
                                                                 RepresentativeSize ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_TRUE( std::equal( root_.control_output, root_.control_output + RepresentativeSize,
                             ControlBytes.begin() ) );

    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    std::copy( ControlBytes.begin(), ControlBytes.end(), root_.control_output );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, root_.control_output + 2u,
                                                                 RepresentativeSize ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_TRUE( std::equal( root_.control_output, root_.control_output + RepresentativeSize,
                             ControlBytes.begin() + 2u ) );
}

TEST_F( TransportControlOutputTest, IdenticalPublicationIsIdempotentWhenReadyOrPeeked )
{
    constexpr std::array<std::size_t, 2u> Sizes{ 1u, 20u };
    for ( const std::size_t size : Sizes )
    {
        for ( const bool peeked : { false, true } )
        {
            ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
            Publish( size );
            if ( peeked )
            {
                Peek( size );
            }
            const auto state    = root_.control_output_state;
            const auto retained = SnapshotControlBytes( root_ );
            const auto isolated = Snapshot();
            EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded(
                           &root_, root_.control_output, size ),
                       HIL_TRANSPORT_STATUS_OK );
            auto separate = ControlBytes;
            EXPECT_EQ(
                HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, separate.data(), size ),
                HIL_TRANSPORT_STATUS_OK );
            EXPECT_EQ( root_.control_output_state, state );
            EXPECT_EQ( root_.control_output_size, size );
            ExpectControlBytes( root_, retained );
            ExpectIsolationUnchanged( isolated );
        }
    }
}

TEST_F( TransportControlOutputTest, DifferentPublicationCannotReplaceReadyOrPeekedItem )
{
    for ( const bool peeked : { false, true } )
    {
        const auto try_candidate = [this, peeked]( const std::uint8_t* candidate,
                                                   std::size_t         size ) {
            ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
            Publish();
            if ( peeked )
            {
                Peek();
            }
            const auto state    = root_.control_output_state;
            const auto retained = SnapshotControlBytes( root_ );
            const auto isolated = Snapshot();
            EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, candidate, size ),
                       HIL_TRANSPORT_STATUS_NOT_READY );
            EXPECT_EQ( root_.control_output_state, state );
            EXPECT_EQ( root_.control_output_size, RepresentativeSize );
            ExpectControlBytes( root_, retained );
            ExpectIsolationUnchanged( isolated );
        };

        try_candidate( ControlBytes.data(), RepresentativeSize - 1u );
        auto changed = ControlBytes;
        changed[0] ^= 0xFFu;
        try_candidate( changed.data(), RepresentativeSize );
        changed = ControlBytes;
        changed[RepresentativeSize / 2u] ^= 0xFFu;
        try_candidate( changed.data(), RepresentativeSize );
        changed = ControlBytes;
        changed[RepresentativeSize - 1u] ^= 0xFFu;
        try_candidate( changed.data(), RepresentativeSize );
        try_candidate( DifferentBytes.data(), DifferentBytes.size() );
    }
}

TEST_F( TransportControlOutputTest, PeekValidatesArgumentsAndIdleIsNotReady )
{
    std::array<std::uint8_t, 24u> destination{};
    destination.fill( 0x5Au );
    const auto  original    = destination;
    std::size_t output_size = 99u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, destination.data(),
                                                             destination.size(), nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( nullptr, destination.data(),
                                                             destination.size(), &output_size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( output_size, 0u );
    output_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, nullptr, 1u, &output_size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( output_size, 0u );
    output_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, destination.data(),
                                                             destination.size(), &output_size ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( output_size, 0u );
    output_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, nullptr, 0u, &output_size ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( output_size, 0u );
    EXPECT_EQ( destination, original );
}

TEST_F( TransportControlOutputTest, SizeQueriesAndSmallDestinationsPreserveReadyItem )
{
    Publish();
    std::array<std::uint8_t, 24u> destination{};
    destination.fill( 0x5Au );
    const auto  original    = destination;
    std::size_t output_size = 0u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, nullptr, 0u, &output_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, RepresentativeSize );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );

    output_size = 0u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, destination.data(), 0u,
                                                             &output_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, RepresentativeSize );
    EXPECT_EQ( destination, original );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );

    output_size = 0u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output(
                   &root_, destination.data(), RepresentativeSize - 1u, &output_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, RepresentativeSize );
    EXPECT_EQ( destination, original );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY );
}

TEST_F( TransportControlOutputTest, ExactAndLargerPeeksPinAndRepeatedlyReturnSameBytes )
{
    Publish();
    std::array<std::uint8_t, 24u> destination{};
    destination.fill( 0x5Au );
    std::size_t output_size = 0u;
    const auto  isolated    = Snapshot();

    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, destination.data(),
                                                             RepresentativeSize, &output_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output_size, RepresentativeSize );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED );
    EXPECT_TRUE( std::equal( destination.begin(), destination.begin() + RepresentativeSize,
                             ControlBytes.begin() ) );
    EXPECT_TRUE( std::all_of( destination.begin() + RepresentativeSize, destination.end(),
                              []( std::uint8_t value ) { return value == 0x5Au; } ) );
    ExpectIsolationUnchanged( isolated );

    output_size = 0u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, nullptr, 0u, &output_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, RepresentativeSize );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED );

    destination.fill( 0x4Cu );
    const auto before_small_peek = destination;
    output_size                  = 0u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output(
                   &root_, destination.data(), RepresentativeSize - 1u, &output_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, RepresentativeSize );
    EXPECT_EQ( destination, before_small_peek );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED );

    destination.fill( 0x6Bu );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, destination.data(),
                                                             destination.size(), &output_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_TRUE( std::equal( destination.begin(), destination.begin() + RepresentativeSize,
                             ControlBytes.begin() ) );
    EXPECT_TRUE( std::all_of( destination.begin() + RepresentativeSize, destination.end(),
                              []( std::uint8_t value ) { return value == 0x6Bu; } ) );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED );
}

TEST_F( TransportControlOutputTest, CommitOnlyReleasesPeekedItemWithoutClearingBytes )
{
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Commit_Output( nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Commit_Output( &root_ ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    Publish();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Commit_Output( &root_ ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    Peek();
    const auto retained = SnapshotControlBytes( root_ );
    const auto isolated = Snapshot();
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Commit_Output( &root_ ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( root_.control_output_size, 0u );
    ExpectControlBytes( root_, retained );
    ExpectIsolationUnchanged( isolated );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Commit_Output( &root_ ),
               HIL_TRANSPORT_STATUS_NOT_READY );

    std::size_t output_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Peek_Output( &root_, nullptr, 0u, &output_size ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( output_size, 0u );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, DifferentBytes.data(),
                                                                 DifferentBytes.size() ),
               HIL_TRANSPORT_STATUS_OK );
}

TEST_F( TransportControlOutputTest, ResetRepairsEveryValidAndCorruptedLifecycle )
{
    struct StateSize
    {
        HIL_Transport_Mvp_Control_Output_State_T state;
        std::size_t                              size;
    };
    const std::array<StateSize, 9u> Cases{
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE, 0u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY, 1u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED, 20u },
        StateSize{ InvalidControlState(), 7u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE, 1u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY, 0u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED, 0u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY, 21u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED, 21u } };

    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    for ( const auto& test_case : Cases )
    {
        std::fill( std::begin( root_.control_output ), std::end( root_.control_output ), 0x7Cu );
        root_.control_output_state = test_case.state;
        root_.control_output_size  = test_case.size;
        const auto retained        = SnapshotControlBytes( root_ );
        const auto isolated        = Snapshot();
        ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
        EXPECT_EQ( root_.control_output_size, 0u );
        ExpectControlBytes( root_, retained );
        ExpectIsolationUnchanged( isolated );
        std::uint8_t pending = 1u;
        EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( &root_, &pending ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( pending, 0u );
    }
}

TEST( TransportControlOutputProfile, PublicResetClearsPrivateControlOwnership )
{
    HIL_Transport_Config_T config{};
    HIL_TRANSPORT_Default_Config( &config );
    config.session_seed       = UINT64_C( 0x1234 );
    std::size_t required_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_OK );
    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, 4096u> workspace{};
    ASSERT_LE( required_size, workspace.size() );
    HIL_Transport_Context_T context{};
    HIL_Transport_Storage_T storage{ workspace.data(), required_size };
    ASSERT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &storage ),
               HIL_TRANSPORT_STATUS_OK );
    auto* root = static_cast<HIL_Transport_Mvp_Root_T*>( context.implementation );
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( root, ControlBytes.data(),
                                                                 RepresentativeSize ),
               HIL_TRANSPORT_STATUS_OK );
    const auto retained = SnapshotControlBytes( *root );

    ASSERT_EQ( HIL_TRANSPORT_Reset( &context ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( root->control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
    EXPECT_EQ( root->control_output_size, 0u );
    ExpectControlBytes( *root, retained );
    EXPECT_EQ( root->base.session_state, HIL_TRANSPORT_SESSION_STATE_DISCONNECTED );
    EXPECT_EQ( root->base.last_failure, HIL_TRANSPORT_FAILURE_LOCAL_RESET );
}

TEST_F( TransportControlOutputTest, PendingStatusReportsOwnershipAndClearsResultOnFailure )
{
    std::uint8_t pending = 9u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( nullptr, &pending ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( pending, 0u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( &root_, nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( &root_, &pending ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending, 0u );
    Publish();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( &root_, &pending ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending, 1u );
    Peek();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( &root_, &pending ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( pending, 1u );
}

TEST_F( TransportControlOutputTest, PendingStatusRejectsEveryInvalidStateSizeCombination )
{
    struct StateSize
    {
        HIL_Transport_Mvp_Control_Output_State_T state;
        std::size_t                              size;
    };
    const std::array<StateSize, 6u> Cases{
        StateSize{ InvalidControlState(), 1u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE, 1u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY, 0u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED, 0u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY, 21u },
        StateSize{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED, 21u } };

    for ( const auto& test_case : Cases )
    {
        root_.base.session_state   = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.base.last_failure    = HIL_TRANSPORT_FAILURE_NONE;
        root_.session.state        = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.session.last_failure = HIL_TRANSPORT_FAILURE_NONE;
        root_.control_output_state = test_case.state;
        root_.control_output_size  = test_case.size;
        std::uint8_t pending       = 9u;

        EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( &root_, &pending ),
                   HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
        EXPECT_EQ( pending, 0u );
        ExpectFault();
        EXPECT_EQ( root_.control_output_state, test_case.state );
        EXPECT_EQ( root_.control_output_size, test_case.size );
        ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    }
}

TEST_F( TransportControlOutputTest, InvalidLifecycleCombinationsFaultWithoutAccessOrMutation )
{
    enum class Operation
    {
        Publish,
        Peek,
        Commit,
        Pending
    };
    struct InvalidCase
    {
        HIL_Transport_Mvp_Control_Output_State_T state;
        std::size_t                              size;
        Operation                                operation;
    };
    const std::array<InvalidCase, 6u> Cases{
        InvalidCase{ InvalidControlState(), 1u, Operation::Publish },
        InvalidCase{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE, 1u, Operation::Peek },
        InvalidCase{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY, 0u, Operation::Commit },
        InvalidCase{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED, 0u, Operation::Pending },
        InvalidCase{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_READY, 21u, Operation::Publish },
        InvalidCase{ HIL_TRANSPORT_MVP_CONTROL_OUTPUT_PEEKED, 21u, Operation::Peek } };

    for ( const auto& test_case : Cases )
    {
        ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
        root_.base.session_state   = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.session.state        = HIL_TRANSPORT_SESSION_STATE_DISCONNECTED;
        root_.base.last_failure    = HIL_TRANSPORT_FAILURE_NONE;
        root_.session.last_failure = HIL_TRANSPORT_FAILURE_NONE;
        root_.control_output_state = test_case.state;
        root_.control_output_size  = test_case.size;
        std::fill( std::begin( root_.control_output ), std::end( root_.control_output ), 0x4Du );
        const auto                    retained = SnapshotControlBytes( root_ );
        std::array<std::uint8_t, 24u> destination{};
        destination.fill( 0x6Eu );
        const auto             destination_before = destination;
        std::size_t            output_size        = 99u;
        std::uint8_t           pending            = 9u;
        HIL_Transport_Status_T status             = HIL_TRANSPORT_STATUS_OK;

        switch ( test_case.operation )
        {
            case Operation::Publish:
                status = HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded(
                    &root_, ControlBytes.data(), RepresentativeSize );
                break;
            case Operation::Peek:
                status = HIL_TRANSPORT_MVP_Control_Output_Peek_Output(
                    &root_, destination.data(), destination.size(), &output_size );
                break;
            case Operation::Commit:
                status = HIL_TRANSPORT_MVP_Control_Output_Commit_Output( &root_ );
                break;
            case Operation::Pending:
                status = HIL_TRANSPORT_MVP_Control_Output_Get_Pending_Status( &root_, &pending );
                break;
        }

        EXPECT_EQ( status, HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
        ExpectFault();
        EXPECT_EQ( root_.control_output_state, test_case.state );
        EXPECT_EQ( root_.control_output_size, test_case.size );
        ExpectControlBytes( root_, retained );
        EXPECT_EQ( destination, destination_before );
        if ( test_case.operation == Operation::Peek )
        {
            EXPECT_EQ( output_size, 0u );
        }
        if ( test_case.operation == Operation::Pending )
        {
            EXPECT_EQ( pending, 0u );
        }
        ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( root_.control_output_state, HIL_TRANSPORT_MVP_CONTROL_OUTPUT_IDLE );
        EXPECT_EQ( root_.control_output_size, 0u );
    }
}

TEST_F( TransportControlOutputTest, RepresentativeOperationsPreserveReliableAndRootIsolation )
{
    auto isolated = Snapshot();
    Publish();
    ExpectIsolationUnchanged( isolated );

    isolated = Snapshot();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, ControlBytes.data(),
                                                                 RepresentativeSize ),
               HIL_TRANSPORT_STATUS_OK );
    ExpectIsolationUnchanged( isolated );

    isolated = Snapshot();
    EXPECT_EQ( HIL_TRANSPORT_MVP_Control_Output_Publish_Encoded( &root_, DifferentBytes.data(),
                                                                 DifferentBytes.size() ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    ExpectIsolationUnchanged( isolated );

    isolated = Snapshot();
    Peek();
    ExpectIsolationUnchanged( isolated );

    isolated = Snapshot();
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Commit_Output( &root_ ), HIL_TRANSPORT_STATUS_OK );
    ExpectIsolationUnchanged( isolated );

    Publish();
    isolated = Snapshot();
    ASSERT_EQ( HIL_TRANSPORT_MVP_Control_Output_Reset( &root_ ), HIL_TRANSPORT_STATUS_OK );
    ExpectIsolationUnchanged( isolated );
}

}  // namespace
