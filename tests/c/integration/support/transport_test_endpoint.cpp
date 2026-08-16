/**
 * @file transport_test_endpoint.cpp
 * @brief Implementation of the public-API Transport integration endpoint wrapper.
 */
#include "support/transport_test_endpoint.hpp"

#include <limits>

namespace hil_rig_protocol::test {

TransportTestEndpointConfig
TransportTestEndpointConfig::Host( const std::uint64_t seed, const std::uint16_t initial_sequence,
                                   const std::uint32_t retransmit_timeout_ms,
                                   const std::uint8_t  max_retries )
{
    TransportTestEndpointConfig config{};
    config.role                      = HIL_TRANSPORT_ROLE_HOST;
    config.session_seed              = seed;
    config.initial_reliable_sequence = initial_sequence;
    config.retransmit_timeout_ms     = retransmit_timeout_ms;
    config.max_retries               = max_retries;
    return config;
}

TransportTestEndpointConfig
TransportTestEndpointConfig::Rig( const std::uint16_t initial_sequence,
                                  const std::uint32_t retransmit_timeout_ms,
                                  const std::uint8_t  max_retries )
{
    TransportTestEndpointConfig config{};
    config.role                      = HIL_TRANSPORT_ROLE_RIG;
    config.session_seed              = HIL_TRANSPORT_SESSION_SEED_INVALID;
    config.initial_reliable_sequence = initial_sequence;
    config.retransmit_timeout_ms     = retransmit_timeout_ms;
    config.max_retries               = max_retries;
    return config;
}

HIL_Transport_Status_T
TransportTestEndpoint::Initialize( const TransportTestEndpointConfig& config )
{
    if ( initialized_ )
    {
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    context_ = {};
    role_    = config.role;
    HIL_TRANSPORT_Default_Config( &config_ );
    config_.max_application_message_size = config.max_application_message_size;
    config_.max_encoded_frame_size       = config.max_encoded_frame_size;
    config_.session_seed                 = config.session_seed;
    config_.initial_reliable_sequence    = config.initial_reliable_sequence;
    config_.connection_timeout_ms        = config.connection_timeout_ms;
    config_.retransmit_timeout_ms        = config.retransmit_timeout_ms;
    config_.max_retries                  = config.max_retries;

    std::size_t                  required_size = 0u;
    const HIL_Transport_Status_T size_status =
        HIL_TRANSPORT_Required_Storage_Size( &config_, &required_size );
    if ( size_status != HIL_TRANSPORT_STATUS_OK )
    {
        workspace_.clear();
        return size_status;
    }

    if ( required_size
         > ( std::numeric_limits<std::size_t>::max() - sizeof( std::max_align_t ) + 1u ) )
    {
        workspace_.clear();
        return HIL_TRANSPORT_STATUS_INVALID_ARGUMENT;
    }

    const std::size_t aligned_elements =
        ( required_size + sizeof( std::max_align_t ) - 1u ) / sizeof( std::max_align_t );
    workspace_.assign( aligned_elements, std::max_align_t{} );

    HIL_Transport_Storage_T storage{};
    storage.workspace      = reinterpret_cast<std::uint8_t*>( workspace_.data() );
    storage.workspace_size = required_size;
    const HIL_Transport_Status_T init_status =
        HIL_TRANSPORT_Init( &context_, config.role, &config_, &storage );
    initialized_ = ( init_status == HIL_TRANSPORT_STATUS_OK );
    return init_status;
}

HIL_Transport_Status_T
TransportTestEndpoint::InitializeConnected( const TransportTestEndpointConfig& config,
                                            const std::uint32_t                now_ms )
{
    const HIL_Transport_Status_T status = Initialize( config );
    if ( status != HIL_TRANSPORT_STATUS_OK )
    {
        return status;
    }
    return NotifyLink( HIL_TRANSPORT_LINK_STATE_CONNECTED, now_ms );
}

HIL_Transport_Status_T TransportTestEndpoint::InitializeConnected(
    const HIL_Transport_Role_T role, const std::uint64_t seed, const std::uint16_t initial_sequence,
    const std::uint32_t retransmit_timeout_ms, const std::uint8_t max_retries )
{
    TransportTestEndpointConfig config{};
    config.role                      = role;
    config.session_seed              = seed;
    config.initial_reliable_sequence = initial_sequence;
    config.retransmit_timeout_ms     = retransmit_timeout_ms;
    config.max_retries               = max_retries;
    return InitializeConnected( config, 0u );
}

HIL_Transport_Status_T TransportTestEndpoint::NotifyLink( const HIL_Transport_Link_State_T state,
                                                          const std::uint32_t              now_ms )
{
    return HIL_TRANSPORT_Notify_Link_State( &context_, state, now_ms );
}

HIL_Transport_Status_T TransportTestEndpoint::Process( const std::uint32_t                  now_ms,
                                                       const HIL_Transport_Operating_Mode_T mode )
{
    return HIL_TRANSPORT_Process( &context_, now_ms, mode );
}

HIL_Transport_Status_T TransportTestEndpoint::Reset()
{
    return HIL_TRANSPORT_Reset( &context_ );
}

HIL_Transport_Status_T TransportTestEndpoint::SubmitApplication( const std::uint8_t* const data,
                                                                 const std::size_t         size )
{
    return HIL_TRANSPORT_Submit_Application_Data( &context_, data, size );
}

HIL_Transport_Status_T
TransportTestEndpoint::SubmitApplication( const std::vector<std::uint8_t>& bytes )
{
    return SubmitApplication( bytes.data(), bytes.size() );
}

TransportPeekResult TransportTestEndpoint::PeekOutput()
{
    return PeekOutput( config_.max_encoded_frame_size );
}

TransportPeekResult TransportTestEndpoint::PeekOutput( const std::size_t buffer_capacity )
{
    TransportPeekResult result{};
    result.buffer_capacity = buffer_capacity;
    result.bytes.resize( buffer_capacity );
    std::uint8_t* const destination = buffer_capacity == 0u ? nullptr : result.bytes.data();
    result.status =
        HIL_TRANSPORT_Peek_Output( &context_, destination, buffer_capacity, &result.required_size );
    if ( result.status == HIL_TRANSPORT_STATUS_OK )
    {
        result.bytes.resize( result.required_size );
    }
    else
    {
        result.bytes.clear();
    }
    return result;
}

TransportPeekResult TransportTestEndpoint::QueryOutputSize()
{
    return PeekOutput( 0u );
}

HIL_Transport_Status_T TransportTestEndpoint::CommitOutput( const std::uint32_t now_ms )
{
    return HIL_TRANSPORT_Commit_Output( &context_, now_ms );
}

TransportReceiveResult TransportTestEndpoint::ReceiveBytes( const std::uint8_t* const data,
                                                            const std::size_t         size )
{
    TransportReceiveResult result{};
    result.bytes_offered = size;
    result.status = HIL_TRANSPORT_Receive_Bytes( &context_, data, size, &result.bytes_consumed );
    return result;
}

TransportReceiveResult TransportTestEndpoint::ReceiveBytes( const std::vector<std::uint8_t>& bytes )
{
    return ReceiveBytes( bytes.data(), bytes.size() );
}

TransportApplicationReadResult TransportTestEndpoint::ReadApplication()
{
    return ReadApplication( config_.max_application_message_size );
}

TransportApplicationReadResult
TransportTestEndpoint::ReadApplication( const std::size_t buffer_capacity )
{
    TransportApplicationReadResult result{};
    result.buffer_capacity = buffer_capacity;
    result.bytes.resize( buffer_capacity );
    std::uint8_t* const destination = buffer_capacity == 0u ? nullptr : result.bytes.data();
    result.status = HIL_TRANSPORT_Read_Application_Data( &context_, destination, buffer_capacity,
                                                         &result.required_size );
    if ( result.status == HIL_TRANSPORT_STATUS_OK )
    {
        result.bytes.resize( result.required_size );
    }
    else
    {
        result.bytes.clear();
    }
    return result;
}

TransportApplicationReadResult TransportTestEndpoint::QueryApplicationSize()
{
    return ReadApplication( 0u );
}

TransportEventReadResult TransportTestEndpoint::ReadEvent()
{
    TransportEventReadResult result{};
    result.status = HIL_TRANSPORT_Read_Event( &context_, &result.event );
    return result;
}

TransportStatusResult TransportTestEndpoint::GetStatus() const
{
    TransportStatusResult result{};
    result.status = HIL_TRANSPORT_Get_Status( &context_, &result.snapshot );
    return result;
}

TransportEventDrainResult TransportTestEndpoint::DrainEvents()
{
    TransportEventDrainResult drain{};
    while ( true )
    {
        const TransportEventReadResult result = ReadEvent();
        if ( result.status != HIL_TRANSPORT_STATUS_OK )
        {
            drain.terminal_status = result.status;
            return drain;
        }
        drain.events.push_back( result.event );
    }
}

const HIL_Transport_Config_T& TransportTestEndpoint::Config() const
{
    return config_;
}

HIL_Transport_Role_T TransportTestEndpoint::Role() const
{
    return role_;
}

}  // namespace hil_rig_protocol::test
