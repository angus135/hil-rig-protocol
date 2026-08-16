/**
 * @file transport_pair_harness.cpp
 * @brief Implementation of the deterministic two-endpoint Transport harness.
 */
#include "support/transport_pair_harness.hpp"

namespace hil_rig_protocol::test {
namespace {

TransportPairOperationResult ProductionResult( const HIL_Transport_Status_T status )
{
    TransportPairOperationResult result{};
    result.transport_status = status;
    return result;
}

}  // namespace

TransportPairInitializationResult
TransportPairHarness::Initialize( const TransportTestEndpointConfig& host_config,
                                  const TransportTestEndpointConfig& rig_config )
{
    TransportPairInitializationResult result{};
    if ( host_config.role != HIL_TRANSPORT_ROLE_HOST || rig_config.role != HIL_TRANSPORT_ROLE_RIG )
    {
        result.harness_status = TransportTestHarnessStatus::InvalidPairRoles;
        return result;
    }

    link_.Clear();
    result.host_status = host_.Initialize( host_config );
    result.rig_status  = rig_.Initialize( rig_config );
    return result;
}

TransportPairInitializationResult
TransportPairHarness::InitializeConnected( const TransportTestEndpointConfig& host_config,
                                           const TransportTestEndpointConfig& rig_config,
                                           const std::uint32_t                now_ms )
{
    return InitializeConnected( host_config, rig_config, now_ms, now_ms );
}

TransportPairInitializationResult TransportPairHarness::InitializeConnected(
    const TransportTestEndpointConfig& host_config, const TransportTestEndpointConfig& rig_config,
    const std::uint32_t host_now_ms, const std::uint32_t rig_now_ms )
{
    host_now_ms_                             = host_now_ms;
    rig_now_ms_                              = rig_now_ms;
    TransportPairInitializationResult result = Initialize( host_config, rig_config );
    if ( result.harness_status != TransportTestHarnessStatus::Ok )
    {
        return result;
    }

    if ( result.host_status.has_value() && *result.host_status == HIL_TRANSPORT_STATUS_OK )
    {
        result.host_status = host_.NotifyLink( HIL_TRANSPORT_LINK_STATE_CONNECTED, host_now_ms_ );
    }
    if ( result.rig_status.has_value() && *result.rig_status == HIL_TRANSPORT_STATUS_OK )
    {
        result.rig_status = rig_.NotifyLink( HIL_TRANSPORT_LINK_STATE_CONNECTED, rig_now_ms_ );
    }
    return result;
}

TransportPairOperationResult
TransportPairHarness::EstablishCleanSession( const std::size_t max_service_steps )
{
    for ( std::size_t step = 0u; step < max_service_steps; ++step )
    {
        const TransportPairProcessResult process = ProcessBoth();
        if ( process.host_status != HIL_TRANSPORT_STATUS_OK )
        {
            return ProductionResult( process.host_status );
        }
        if ( process.rig_status != HIL_TRANSPORT_STATUS_OK )
        {
            return ProductionResult( process.rig_status );
        }

        const TransportPairOperationResult pump = PumpHealthyOutputs( 16u );
        if ( pump.harness_status != TransportTestHarnessStatus::Ok )
        {
            return pump;
        }
        if ( pump.transport_status.has_value()
             && *pump.transport_status != HIL_TRANSPORT_STATUS_OK )
        {
            return pump;
        }

        const TransportStatusResult host_status = host_.GetStatus();
        const TransportStatusResult rig_status  = rig_.GetStatus();
        if ( host_status.status != HIL_TRANSPORT_STATUS_OK )
        {
            return ProductionResult( host_status.status );
        }
        if ( rig_status.status != HIL_TRANSPORT_STATUS_OK )
        {
            return ProductionResult( rig_status.status );
        }
        if ( host_status.snapshot.session_state == HIL_TRANSPORT_SESSION_STATE_ESTABLISHED
             && rig_status.snapshot.session_state == HIL_TRANSPORT_SESSION_STATE_ESTABLISHED )
        {
            return ProductionResult( HIL_TRANSPORT_STATUS_OK );
        }

        AdvanceBothTimes();
    }

    TransportPairOperationResult result{};
    result.harness_status = TransportTestHarnessStatus::ServiceStepLimit;
    return result;
}

HIL_Transport_Status_T
TransportPairHarness::ProcessHost( const HIL_Transport_Operating_Mode_T mode )
{
    return host_.Process( host_now_ms_, mode );
}

HIL_Transport_Status_T TransportPairHarness::ProcessRig( const HIL_Transport_Operating_Mode_T mode )
{
    return rig_.Process( rig_now_ms_, mode );
}

TransportPairProcessResult
TransportPairHarness::ProcessBoth( const HIL_Transport_Operating_Mode_T mode )
{
    const HIL_Transport_Status_T host_status = ProcessHost( mode );
    const HIL_Transport_Status_T rig_status  = ProcessRig( mode );
    return TransportPairProcessResult{ host_status, rig_status };
}

TransportPairTransferResult
TransportPairHarness::TransferOneOutput( const TransportTestDirection direction )
{
    TransportTestEndpoint& sender =
        ( direction == TransportTestDirection::HostToRig ) ? host_ : rig_;
    TransportTestEndpoint& receiver =
        ( direction == TransportTestDirection::HostToRig ) ? rig_ : host_;

    TransportPairTransferResult result{};
    const std::uint32_t         sender_now =
        ( direction == TransportTestDirection::HostToRig ) ? host_now_ms_ : rig_now_ms_;
    result.accept = link_.AcceptOutput( sender, sender_now );
    if ( result.accept.harness_status != TransportTestHarnessStatus::Ok )
    {
        result.harness_status = result.accept.harness_status;
        return result;
    }
    if ( !result.accept.transport_status.has_value()
         || *result.accept.transport_status != HIL_TRANSPORT_STATUS_OK )
    {
        return result;
    }
    if ( !result.accept.handle.has_value()
         || !link_.QueueAcceptedForDelivery( *result.accept.handle ) )
    {
        result.harness_status = TransportTestHarnessStatus::AcceptedItemNotFound;
        return result;
    }

    result.delivery = link_.DeliverReady( receiver );
    if ( result.delivery.harness_status != TransportTestHarnessStatus::Ok )
    {
        result.harness_status = result.delivery.harness_status;
    }
    return result;
}

void TransportPairHarness::SetHostTime( const std::uint32_t now_ms )
{
    host_now_ms_ = now_ms;
}

void TransportPairHarness::SetRigTime( const std::uint32_t now_ms )
{
    rig_now_ms_ = now_ms;
}

void TransportPairHarness::SetBothTimes( const std::uint32_t now_ms )
{
    SetHostTime( now_ms );
    SetRigTime( now_ms );
}

void TransportPairHarness::AdvanceHostTime( const std::uint32_t delta_ms )
{
    host_now_ms_ += delta_ms;
}

void TransportPairHarness::AdvanceRigTime( const std::uint32_t delta_ms )
{
    rig_now_ms_ += delta_ms;
}

void TransportPairHarness::AdvanceBothTimes( const std::uint32_t delta_ms )
{
    AdvanceHostTime( delta_ms );
    AdvanceRigTime( delta_ms );
}

std::uint32_t TransportPairHarness::HostNow() const
{
    return host_now_ms_;
}

std::uint32_t TransportPairHarness::RigNow() const
{
    return rig_now_ms_;
}

TransportTestEndpoint& TransportPairHarness::Host()
{
    return host_;
}

TransportTestEndpoint& TransportPairHarness::Rig()
{
    return rig_;
}

TransportTestLink& TransportPairHarness::Link()
{
    return link_;
}

const TransportTestEndpoint& TransportPairHarness::Host() const
{
    return host_;
}

const TransportTestEndpoint& TransportPairHarness::Rig() const
{
    return rig_;
}

const TransportTestLink& TransportPairHarness::Link() const
{
    return link_;
}

TransportPairOperationResult
TransportPairHarness::PumpHealthyOutputs( const std::size_t max_transfers )
{
    for ( std::size_t transfer = 0u; transfer < max_transfers; ++transfer )
    {
        bool                         transferred = false;
        TransportPairOperationResult status =
            TransferPendingDirection( TransportTestDirection::HostToRig, transferred );
        if ( status.harness_status != TransportTestHarnessStatus::Ok
             || ( status.transport_status.has_value()
                  && *status.transport_status != HIL_TRANSPORT_STATUS_OK ) )
        {
            return status;
        }

        status = TransferPendingDirection( TransportTestDirection::RigToHost, transferred );
        if ( status.harness_status != TransportTestHarnessStatus::Ok
             || ( status.transport_status.has_value()
                  && *status.transport_status != HIL_TRANSPORT_STATUS_OK ) )
        {
            return status;
        }

        if ( !transferred )
        {
            return ProductionResult( HIL_TRANSPORT_STATUS_OK );
        }
    }

    TransportPairOperationResult result{};
    result.harness_status = TransportTestHarnessStatus::ServiceStepLimit;
    return result;
}

TransportPairOperationResult
TransportPairHarness::TransferPendingDirection( const TransportTestDirection direction,
                                                bool&                        transferred )
{
    TransportTestEndpoint& sender =
        ( direction == TransportTestDirection::HostToRig ) ? host_ : rig_;
    const TransportStatusResult status = sender.GetStatus();
    if ( status.status != HIL_TRANSPORT_STATUS_OK )
    {
        return ProductionResult( status.status );
    }
    if ( status.snapshot.output_pending == 0u )
    {
        return ProductionResult( HIL_TRANSPORT_STATUS_OK );
    }

    const TransportPairTransferResult transfer = TransferOneOutput( direction );
    if ( transfer.harness_status != TransportTestHarnessStatus::Ok )
    {
        TransportPairOperationResult result{};
        result.harness_status = transfer.harness_status;
        return result;
    }
    if ( !transfer.accept.transport_status.has_value() )
    {
        TransportPairOperationResult result{};
        result.harness_status = TransportTestHarnessStatus::AcceptedItemNotFound;
        return result;
    }
    if ( *transfer.accept.transport_status != HIL_TRANSPORT_STATUS_OK )
    {
        return ProductionResult( *transfer.accept.transport_status );
    }
    if ( !transfer.delivery.transport_status.has_value() )
    {
        TransportPairOperationResult result{};
        result.harness_status = TransportTestHarnessStatus::AcceptedItemNotFound;
        return result;
    }
    if ( *transfer.delivery.transport_status != HIL_TRANSPORT_STATUS_OK )
    {
        return ProductionResult( *transfer.delivery.transport_status );
    }

    transferred = true;
    return ProductionResult( HIL_TRANSPORT_STATUS_OK );
}

}  // namespace hil_rig_protocol::test
