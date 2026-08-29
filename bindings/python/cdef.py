"""CFFI declarations for the binding-private HIL-RIG Transport adapter.

The declarations intentionally cover only stable public Transport value types and
PR 1's opaque adapter API.  Ellipses ask CFFI's API-mode compiler step to obtain
enum values and structure layout from the real C headers rather than reproducing
ABI details in Python build code.
"""

CDEF = r"""
typedef enum
{
    HIL_PY_ADAPTER_STATUS_OK,
    HIL_PY_ADAPTER_STATUS_INVALID_ARGUMENT,
    HIL_PY_ADAPTER_STATUS_ALLOCATION_FAILED,
    HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR,
    ...
} HIL_Python_Adapter_Status_T;

typedef enum
{
    HIL_TRANSPORT_STATUS_OK,
    HIL_TRANSPORT_STATUS_INVALID_ARGUMENT,
    HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL,
    HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION,
    HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE,
    HIL_TRANSPORT_STATUS_CAPACITY_EXHAUSTED,
    HIL_TRANSPORT_STATUS_DELIVERY_FAILED,
    HIL_TRANSPORT_STATUS_TIMEOUT,
    HIL_TRANSPORT_STATUS_NOT_READY,
    HIL_TRANSPORT_STATUS_NOT_IMPLEMENTED,
    HIL_TRANSPORT_STATUS_INTERNAL_ERROR,
    ...
} HIL_Transport_Status_T;

typedef enum
{
    HIL_TRANSPORT_ROLE_HOST,
    HIL_TRANSPORT_ROLE_RIG,
    ...
} HIL_Transport_Role_T;

typedef enum
{
    HIL_TRANSPORT_LINK_STATE_DISCONNECTED,
    HIL_TRANSPORT_LINK_STATE_CONNECTED,
    ...
} HIL_Transport_Link_State_T;

typedef enum
{
    HIL_TRANSPORT_OPERATING_MODE_NORMAL,
    HIL_TRANSPORT_OPERATING_MODE_BULK_TRANSFER,
    HIL_TRANSPORT_OPERATING_MODE_QUIET_REAL_TIME,
    ...
} HIL_Transport_Operating_Mode_T;

typedef enum
{
    HIL_TRANSPORT_SESSION_STATE_DISCONNECTED,
    HIL_TRANSPORT_SESSION_STATE_CONNECTING,
    HIL_TRANSPORT_SESSION_STATE_ESTABLISHED,
    HIL_TRANSPORT_SESSION_STATE_RECOVERING,
    HIL_TRANSPORT_SESSION_STATE_FAULT,
    ...
} HIL_Transport_Session_State_T;

typedef enum
{
    HIL_TRANSPORT_FAILURE_NONE,
    HIL_TRANSPORT_FAILURE_LINK_LOST,
    HIL_TRANSPORT_FAILURE_CONNECTION_TIMEOUT,
    HIL_TRANSPORT_FAILURE_DELIVERY,
    HIL_TRANSPORT_FAILURE_PROTOCOL,
    HIL_TRANSPORT_FAILURE_CAPACITY,
    HIL_TRANSPORT_FAILURE_LOCAL_RESET,
    HIL_TRANSPORT_FAILURE_INTERNAL,
    ...
} HIL_Transport_Failure_T;

typedef enum
{
    HIL_TRANSPORT_EVENT_NONE,
    HIL_TRANSPORT_EVENT_SESSION_ESTABLISHED,
    HIL_TRANSPORT_EVENT_SESSION_RESET,
    HIL_TRANSPORT_EVENT_DELIVERY_CONFIRMED,
    HIL_TRANSPORT_EVENT_DELIVERY_FAILED,
    HIL_TRANSPORT_EVENT_PROTOCOL_ERROR,
    HIL_TRANSPORT_EVENT_CAPACITY_EXHAUSTED,
    HIL_TRANSPORT_EVENT_LINK_STATE_CHANGED,
    ...
} HIL_Transport_Event_Type_T;

typedef struct
{
    size_t max_application_message_size;
    size_t max_encoded_frame_size;
    uint64_t session_seed;
    uint16_t initial_reliable_sequence;
    uint32_t connection_timeout_ms;
    uint32_t retransmit_timeout_ms;
    uint8_t max_retries;
    ...;
} HIL_Transport_Config_T;

typedef struct
{
    HIL_Transport_Event_Type_T type;
    HIL_Transport_Status_T status;
    HIL_Transport_Failure_T failure;
    size_t required_capacity;
    ...;
} HIL_Transport_Event_T;

typedef struct
{
    HIL_Transport_Role_T role;
    HIL_Transport_Link_State_T link_state;
    HIL_Transport_Session_State_T session_state;
    HIL_Transport_Operating_Mode_T operating_mode;
    uint8_t operating_mode_valid;
    uint8_t output_pending;
    uint8_t application_message_pending;
    uint8_t event_pending;
    uint8_t reliable_delivery_pending;
    HIL_Transport_Failure_T last_failure;
    ...;
} HIL_Transport_Status_Snapshot_T;

typedef struct HIL_Python_Transport HIL_Python_Transport_T;

void HIL_PY_TRANSPORT_Default_Config(HIL_Transport_Config_T* config);

HIL_Python_Adapter_Status_T HIL_PY_TRANSPORT_Create(
    HIL_Transport_Role_T role,
    const HIL_Transport_Config_T* config,
    HIL_Python_Transport_T** out_transport,
    HIL_Transport_Status_T* out_transport_status);

void HIL_PY_TRANSPORT_Destroy(HIL_Python_Transport_T* transport);

HIL_Transport_Status_T HIL_PY_TRANSPORT_Reset(
    HIL_Python_Transport_T* transport);

HIL_Transport_Status_T HIL_PY_TRANSPORT_Notify_Link_State(
    HIL_Python_Transport_T* transport,
    HIL_Transport_Link_State_T link_state,
    uint32_t now_ms);

HIL_Transport_Status_T HIL_PY_TRANSPORT_Submit_Application_Data(
    HIL_Python_Transport_T* transport,
    const uint8_t* payload,
    size_t payload_size);

HIL_Transport_Status_T HIL_PY_TRANSPORT_Receive_Bytes(
    HIL_Python_Transport_T* transport,
    const uint8_t* data,
    size_t data_size,
    size_t* bytes_consumed);

HIL_Transport_Status_T HIL_PY_TRANSPORT_Process(
    HIL_Python_Transport_T* transport,
    uint32_t now_ms,
    HIL_Transport_Operating_Mode_T operating_mode);

HIL_Transport_Status_T HIL_PY_TRANSPORT_Peek_Output(
    HIL_Python_Transport_T* transport,
    uint8_t* output,
    size_t output_capacity,
    size_t* output_size);

HIL_Transport_Status_T HIL_PY_TRANSPORT_Commit_Output(
    HIL_Python_Transport_T* transport,
    uint32_t now_ms);

HIL_Transport_Status_T HIL_PY_TRANSPORT_Read_Application_Data(
    HIL_Python_Transport_T* transport,
    uint8_t* output,
    size_t output_capacity,
    size_t* output_size);

HIL_Transport_Status_T HIL_PY_TRANSPORT_Read_Event(
    HIL_Python_Transport_T* transport,
    HIL_Transport_Event_T* event);

HIL_Transport_Status_T HIL_PY_TRANSPORT_Get_Status(
    const HIL_Python_Transport_T* transport,
    HIL_Transport_Status_Snapshot_T* status);
"""
