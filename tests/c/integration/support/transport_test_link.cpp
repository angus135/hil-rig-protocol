/**
 * @file transport_test_link.cpp
 * @brief Implementation of the deterministic Transport integration byte link.
 */
#include "support/transport_test_link.hpp"

#include <algorithm>
#include <iterator>

namespace hil_rig_protocol::test {

TransportLinkAcceptResult TransportTestLink::AcceptOutput( TransportTestEndpoint& sender,
                                                           const std::uint32_t    now_ms )
{
    TransportLinkAcceptResult                   result{};
    const std::optional<TransportTestDirection> direction = OutputDirectionForRole( sender.Role() );
    if ( !direction.has_value() )
    {
        result.harness_status = TransportTestHarnessStatus::InvalidEndpointRole;
        return result;
    }

    const TransportPeekResult peek = sender.PeekOutput();
    result.transport_status        = peek.status;
    result.size                    = peek.required_size;
    if ( peek.status != HIL_TRANSPORT_STATUS_OK )
    {
        return result;
    }

    const HIL_Transport_Status_T commit_status = sender.CommitOutput( now_ms );
    result.transport_status                    = commit_status;
    if ( commit_status != HIL_TRANSPORT_STATUS_OK )
    {
        return result;
    }

    result.handle = TransportTestOutputHandle{ *direction, next_ordinal_++ };
    State( *direction ).accepted.push_back( TransportTestOutputItem{ *result.handle, peek.bytes } );
    return result;
}

bool TransportTestLink::TakeNextAccepted( const TransportTestDirection direction,
                                          TransportTestOutputItem&     item )
{
    DirectionState& state = State( direction );
    if ( state.accepted.empty() )
    {
        return false;
    }

    item = std::move( state.accepted.front() );
    state.accepted.pop_front();
    return true;
}

bool TransportTestLink::TakeAccepted( const TransportTestOutputHandle handle,
                                      TransportTestOutputItem&        item )
{
    DirectionState& state = State( handle.direction );
    const auto      found = std::find_if( state.accepted.begin(), state.accepted.end(),
                                          [handle]( const TransportTestOutputItem& candidate ) {
                                         return candidate.handle.ordinal == handle.ordinal;
                                     } );
    if ( found == state.accepted.end() )
    {
        return false;
    }

    item = std::move( *found );
    state.accepted.erase( found );
    return true;
}

bool TransportTestLink::DropAccepted( const TransportTestOutputHandle handle )
{
    TransportTestOutputItem item{};
    return TakeAccepted( handle, item );
}

std::optional<TransportTestOutputHandle>
TransportTestLink::DuplicateAccepted( const TransportTestOutputHandle handle )
{
    DirectionState& state = State( handle.direction );
    const auto      found = std::find_if( state.accepted.begin(), state.accepted.end(),
                                          [handle]( const TransportTestOutputItem& candidate ) {
                                         return candidate.handle.ordinal == handle.ordinal;
                                     } );
    if ( found == state.accepted.end() )
    {
        return std::nullopt;
    }

    TransportTestOutputItem duplicate = *found;
    duplicate.handle = TransportTestOutputHandle{ handle.direction, next_ordinal_++ };
    const auto                      insertion        = std::next( found );
    const TransportTestOutputHandle duplicate_handle = duplicate.handle;
    state.accepted.insert( insertion, std::move( duplicate ) );
    return duplicate_handle;
}

bool TransportTestLink::HoldAccepted( const TransportTestOutputHandle handle )
{
    TransportTestOutputItem item{};
    if ( !TakeAccepted( handle, item ) )
    {
        return false;
    }
    State( handle.direction ).held.push_back( std::move( item ) );
    return true;
}

bool TransportTestLink::ReleaseHeld( const TransportTestOutputHandle handle )
{
    DirectionState& state = State( handle.direction );
    const auto      found = std::find_if( state.held.begin(), state.held.end(),
                                          [handle]( const TransportTestOutputItem& candidate ) {
                                         return candidate.handle.ordinal == handle.ordinal;
                                     } );
    if ( found == state.held.end() )
    {
        return false;
    }

    state.accepted.push_back( std::move( *found ) );
    state.held.erase( found );
    return true;
}

bool TransportTestLink::CorruptAcceptedByte( const TransportTestOutputHandle handle,
                                             const std::size_t               byte_offset,
                                             const std::uint8_t              xor_mask )
{
    DirectionState& state = State( handle.direction );
    const auto      found = std::find_if( state.accepted.begin(), state.accepted.end(),
                                          [handle]( const TransportTestOutputItem& candidate ) {
                                         return candidate.handle.ordinal == handle.ordinal;
                                     } );
    if ( found == state.accepted.end() || byte_offset >= found->bytes.size() )
    {
        return false;
    }
    found->bytes[byte_offset] ^= xor_mask;
    return true;
}

bool TransportTestLink::DropNextAccepted( const TransportTestDirection direction )
{
    DirectionState& state = State( direction );
    if ( state.accepted.empty() )
    {
        return false;
    }
    state.accepted.pop_front();
    return true;
}

bool TransportTestLink::DuplicateNextAccepted( const TransportTestDirection direction )
{
    DirectionState& state = State( direction );
    if ( state.accepted.empty() )
    {
        return false;
    }

    return DuplicateAccepted( state.accepted.front().handle ).has_value();
}

bool TransportTestLink::HoldNextAccepted( const TransportTestDirection direction )
{
    DirectionState& state = State( direction );
    if ( state.accepted.empty() )
    {
        return false;
    }
    return HoldAccepted( state.accepted.front().handle );
}

bool TransportTestLink::ReleaseOldestHeld( const TransportTestDirection direction )
{
    DirectionState& state = State( direction );
    if ( state.held.empty() )
    {
        return false;
    }
    return ReleaseHeld( state.held.front().handle );
}

bool TransportTestLink::CorruptNextAcceptedByte( const TransportTestDirection direction,
                                                 const std::size_t            byte_offset,
                                                 const std::uint8_t           xor_mask )
{
    DirectionState& state = State( direction );
    if ( state.accepted.empty() )
    {
        return false;
    }
    return CorruptAcceptedByte( state.accepted.front().handle, byte_offset, xor_mask );
}

bool TransportTestLink::QueueNextAcceptedForDelivery( const TransportTestDirection direction )
{
    TransportTestOutputItem item{};
    if ( !TakeNextAccepted( direction, item ) )
    {
        return false;
    }
    AppendItemToReady( State( direction ), std::move( item ) );
    return true;
}

bool TransportTestLink::QueueAcceptedForDelivery( const TransportTestOutputHandle handle )
{
    TransportTestOutputItem item{};
    if ( !TakeAccepted( handle, item ) )
    {
        return false;
    }
    AppendItemToReady( State( handle.direction ), std::move( item ) );
    return true;
}

std::size_t TransportTestLink::QueueAllAcceptedForDelivery( const TransportTestDirection direction )
{
    std::size_t count = 0u;
    while ( QueueNextAcceptedForDelivery( direction ) )
    {
        ++count;
    }
    return count;
}

void TransportTestLink::InjectReadyBytes( const TransportTestDirection direction,
                                          const std::uint8_t* const bytes, const std::size_t size )
{
    if ( bytes == nullptr || size == 0u )
    {
        return;
    }
    DirectionState& state = State( direction );
    CompactReadyBytes( state );
    state.ready_bytes.insert( state.ready_bytes.end(), bytes, bytes + size );
}

void TransportTestLink::InjectReadyBytes( const TransportTestDirection     direction,
                                          const std::vector<std::uint8_t>& bytes )
{
    InjectReadyBytes( direction, bytes.data(), bytes.size() );
}

TransportLinkDeliveryResult TransportTestLink::DeliverReady( TransportTestEndpoint& receiver,
                                                             const std::size_t      max_bytes )
{
    TransportLinkDeliveryResult                 result{};
    const std::optional<TransportTestDirection> direction =
        InputDirectionForRole( receiver.Role() );
    if ( !direction.has_value() )
    {
        result.harness_status = TransportTestHarnessStatus::InvalidEndpointRole;
        return result;
    }

    DirectionState&   state     = State( *direction );
    const std::size_t available = ReadyByteCount( *direction );
    if ( available == 0u || max_bytes == 0u )
    {
        return result;
    }

    result.bytes_offered                 = std::min( available, max_bytes );
    const TransportReceiveResult receive = receiver.ReceiveBytes(
        state.ready_bytes.data() + state.ready_offset, result.bytes_offered );
    result.transport_status = receive.status;
    result.bytes_consumed   = receive.bytes_consumed;

    if ( receive.bytes_consumed > result.bytes_offered )
    {
        result.harness_status = TransportTestHarnessStatus::ReceiveContractViolation;
        return result;
    }

    state.ready_offset += receive.bytes_consumed;
    CompactReadyBytes( state );
    return result;
}

TransportLinkDeliveryResult TransportTestLink::DeliverZeroLength( TransportTestEndpoint& receiver )
{
    TransportLinkDeliveryResult  result{};
    const TransportReceiveResult receive = receiver.ReceiveBytes( nullptr, 0u );
    result.transport_status              = receive.status;
    result.bytes_offered                 = receive.bytes_offered;
    result.bytes_consumed                = receive.bytes_consumed;
    if ( receive.bytes_consumed != 0u )
    {
        result.harness_status = TransportTestHarnessStatus::ReceiveContractViolation;
    }
    return result;
}

std::size_t TransportTestLink::AcceptedItemCount( const TransportTestDirection direction ) const
{
    return State( direction ).accepted.size();
}

std::size_t TransportTestLink::HeldItemCount( const TransportTestDirection direction ) const
{
    return State( direction ).held.size();
}

std::size_t TransportTestLink::ReadyByteCount( const TransportTestDirection direction ) const
{
    const DirectionState& state = State( direction );
    if ( state.ready_offset > state.ready_bytes.size() )
    {
        return 0u;
    }
    return state.ready_bytes.size() - state.ready_offset;
}

void TransportTestLink::Clear()
{
    host_to_rig_ = {};
    rig_to_host_ = {};
}

std::optional<TransportTestDirection>
TransportTestLink::OutputDirectionForRole( const HIL_Transport_Role_T role )
{
    if ( role == HIL_TRANSPORT_ROLE_HOST )
    {
        return TransportTestDirection::HostToRig;
    }
    if ( role == HIL_TRANSPORT_ROLE_RIG )
    {
        return TransportTestDirection::RigToHost;
    }
    return std::nullopt;
}

std::optional<TransportTestDirection>
TransportTestLink::InputDirectionForRole( const HIL_Transport_Role_T role )
{
    if ( role == HIL_TRANSPORT_ROLE_HOST )
    {
        return TransportTestDirection::RigToHost;
    }
    if ( role == HIL_TRANSPORT_ROLE_RIG )
    {
        return TransportTestDirection::HostToRig;
    }
    return std::nullopt;
}

TransportTestLink::DirectionState&
TransportTestLink::State( const TransportTestDirection direction )
{
    return ( direction == TransportTestDirection::HostToRig ) ? host_to_rig_ : rig_to_host_;
}

const TransportTestLink::DirectionState&
TransportTestLink::State( const TransportTestDirection direction ) const
{
    return ( direction == TransportTestDirection::HostToRig ) ? host_to_rig_ : rig_to_host_;
}

void TransportTestLink::CompactReadyBytes( DirectionState& state )
{
    if ( state.ready_offset == 0u )
    {
        return;
    }
    if ( state.ready_offset >= state.ready_bytes.size() )
    {
        state.ready_bytes.clear();
        state.ready_offset = 0u;
        return;
    }

    state.ready_bytes.erase( state.ready_bytes.begin(),
                             state.ready_bytes.begin()
                                 + static_cast<std::ptrdiff_t>( state.ready_offset ) );
    state.ready_offset = 0u;
}

void TransportTestLink::AppendItemToReady( DirectionState& state, TransportTestOutputItem&& item )
{
    CompactReadyBytes( state );
    state.ready_bytes.insert( state.ready_bytes.end(), item.bytes.begin(), item.bytes.end() );
}

}  // namespace hil_rig_protocol::test
