/**
 * @file hil_rig_protocol_ffi.h
 * @brief Binding-private host adapter for the HIL-RIG Transport facade.
 *
 * @details This header belongs to the future Python CFFI integration. It is not
 * installed and is not part of the firmware-facing public C API. The adapter
 * owns one public Transport context and the workspace required by that context,
 * while forwarding protocol operations to the existing public Transport facade.
 */
#ifndef HIL_RIG_PROTOCOL_BINDINGS_PYTHON_HIL_RIG_PROTOCOL_FFI_H
#define HIL_RIG_PROTOCOL_BINDINGS_PYTHON_HIL_RIG_PROTOCOL_FFI_H

#include <stddef.h>
#include <stdint.h>

#include "hil_rig_protocol/transport/transport.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Opaque host-side owner of one Transport context and its workspace. */
typedef struct HIL_Python_Transport HIL_Python_Transport_T;

/**
 * @brief Result of creating a Python Transport adapter.
 *
 * @details This result is separate from HIL_Transport_Status_T because the
 * heap-free Transport core has no allocation-failure status. It is used only by
 * HIL_PY_TRANSPORT_Create(); all normal forwarded operations return the exact
 * core Transport status directly.
 */
typedef enum
{
    /** Adapter allocation and core Transport initialization completed. */
    HIL_PY_ADAPTER_STATUS_OK = 0,

    /** A required adapter output pointer was NULL. */
    HIL_PY_ADAPTER_STATUS_INVALID_ARGUMENT,

    /** The adapter handle or workspace could not be allocated. */
    HIL_PY_ADAPTER_STATUS_ALLOCATION_FAILED,

    /** A core Transport size query or initialization operation failed. */
    HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR
} HIL_Python_Adapter_Status_T;

/**
 * @brief Populate the public Transport configuration defaults.
 *
 * @details This is a direct binding-private forwarding entry point for
 * HIL_TRANSPORT_Default_Config(). NULL retains the core helper's no-op
 * behaviour.
 *
 * @param[out] config Configuration to initialize, or NULL.
 */
void HIL_PY_TRANSPORT_Default_Config( HIL_Transport_Config_T* config );

/**
 * @brief Allocate and initialize one opaque Transport adapter.
 *
 * @details Both output pointers are mandatory adapter arguments. When
 * out_transport is non-NULL it is cleared before validation or other work. When
 * out_transport_status is non-NULL it is initialized deterministically to
 * HIL_TRANSPORT_STATUS_INVALID_ARGUMENT; after adapter-output validation it is
 * set to HIL_TRANSPORT_STATUS_OK before any core operation. Therefore adapter
 * allocation failure leaves the core status as OK, while a core size-query or
 * initialization failure preserves that exact core result.
 *
 * A NULL config is deliberately treated as a core Transport configuration
 * failure. It is passed to HIL_TRANSPORT_Required_Storage_Size(), so creation
 * returns HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR and reports the core
 * HIL_TRANSPORT_STATUS_INVALID_ARGUMENT result.
 *
 * On success the adapter owns one zero-initialized HIL_Transport_Context_T and
 * one zero-initialized heap workspace of exactly the size required by the
 * selected Transport profile. The workspace allocation uses the standard C
 * allocator, whose returned storage satisfies the alignment required for
 * objects with fundamental alignment, including the current
 * HIL_TRANSPORT_WORKSPACE_ALIGNMENT requirement.
 *
 * @param[in] role Transport role fixed for the adapter lifetime.
 * @param[in] config Required public Transport configuration.
 * @param[out] out_transport Receives the created opaque handle only on success.
 * @param[out] out_transport_status Receives OK, or the exact failing core status.
 * @return Adapter-specific creation result.
 */
HIL_Python_Adapter_Status_T HIL_PY_TRANSPORT_Create( HIL_Transport_Role_T          role,
                                                     const HIL_Transport_Config_T* config,
                                                     HIL_Python_Transport_T**      out_transport,
                                                     HIL_Transport_Status_T* out_transport_status );

/**
 * @brief Release one adapter and its owned Transport workspace.
 *
 * @details NULL is a no-op. Destruction performs only local lifetime cleanup;
 * it does not call HIL_TRANSPORT_Reset() and cannot generate protocol output or
 * events. A non-NULL handle must be destroyed at most once by its owner.
 *
 * @param[in] transport Adapter to destroy, or NULL.
 */
void HIL_PY_TRANSPORT_Destroy( HIL_Python_Transport_T* transport );

/** Forward HIL_TRANSPORT_Reset() for the owned context. */
HIL_Transport_Status_T HIL_PY_TRANSPORT_Reset( HIL_Python_Transport_T* transport );

/** Forward HIL_TRANSPORT_Notify_Link_State() for the owned context. */
HIL_Transport_Status_T HIL_PY_TRANSPORT_Notify_Link_State( HIL_Python_Transport_T*    transport,
                                                           HIL_Transport_Link_State_T link_state,
                                                           uint32_t                   now_ms );

/** Forward HIL_TRANSPORT_Submit_Application_Data() for the owned context. */
HIL_Transport_Status_T HIL_PY_TRANSPORT_Submit_Application_Data( HIL_Python_Transport_T* transport,
                                                                 const uint8_t*          payload,
                                                                 size_t payload_size );

/** Forward HIL_TRANSPORT_Receive_Bytes() for the owned context. */
HIL_Transport_Status_T HIL_PY_TRANSPORT_Receive_Bytes( HIL_Python_Transport_T* transport,
                                                       const uint8_t* data, size_t data_size,
                                                       size_t* bytes_consumed );

/** Forward HIL_TRANSPORT_Process() for the owned context. */
HIL_Transport_Status_T HIL_PY_TRANSPORT_Process( HIL_Python_Transport_T* transport, uint32_t now_ms,
                                                 HIL_Transport_Operating_Mode_T operating_mode );

/** Forward HIL_TRANSPORT_Peek_Output() for the owned context. */
HIL_Transport_Status_T HIL_PY_TRANSPORT_Peek_Output( HIL_Python_Transport_T* transport,
                                                     uint8_t* output, size_t output_capacity,
                                                     size_t* output_size );

/** Forward HIL_TRANSPORT_Commit_Output() for the owned context. */
HIL_Transport_Status_T HIL_PY_TRANSPORT_Commit_Output( HIL_Python_Transport_T* transport,
                                                       uint32_t                now_ms );

/** Forward HIL_TRANSPORT_Read_Application_Data() for the owned context. */
HIL_Transport_Status_T HIL_PY_TRANSPORT_Read_Application_Data( HIL_Python_Transport_T* transport,
                                                               uint8_t*                output,
                                                               size_t  output_capacity,
                                                               size_t* output_size );

/** Forward HIL_TRANSPORT_Read_Event() for the owned context. */
HIL_Transport_Status_T HIL_PY_TRANSPORT_Read_Event( HIL_Python_Transport_T* transport,
                                                    HIL_Transport_Event_T*  event );

/** Forward HIL_TRANSPORT_Get_Status() for the owned context. */
HIL_Transport_Status_T HIL_PY_TRANSPORT_Get_Status( const HIL_Python_Transport_T*    transport,
                                                    HIL_Transport_Status_Snapshot_T* status );

#ifdef __cplusplus
}
#endif

#endif /* HIL_RIG_PROTOCOL_BINDINGS_PYTHON_HIL_RIG_PROTOCOL_FFI_H */
