"""CFFI declarations for the private shared Transport and Application boundary.

Transport uses its ownership adapter; Application calls the public C API directly.
API-mode CFFI checks declarations against the real C headers. Ellipses resolve
enum values and partial layouts without reproducing ABI details in Python.
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


/* Application: compiler-resolved size/alignment, including MSVC max_align_t. */
typedef struct { ...; } max_align_t;

/* application_status.h */

typedef enum
{
    HIL_APPLICATION_STATUS_OK,
    HIL_APPLICATION_STATUS_INVALID_ARGUMENT,
    HIL_APPLICATION_STATUS_UNINITIALIZED,
    HIL_APPLICATION_STATUS_BUFFER_TOO_SMALL,
    HIL_APPLICATION_STATUS_INVALID_MESSAGE_TYPE,
    HIL_APPLICATION_STATUS_INVALID_SUBTYPE,
    HIL_APPLICATION_STATUS_MALFORMED_MESSAGE,
    HIL_APPLICATION_STATUS_TRUNCATED_MESSAGE,
    HIL_APPLICATION_STATUS_INVALID_LENGTH,
    HIL_APPLICATION_STATUS_INVALID_COUNT,
    HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE,
    HIL_APPLICATION_STATUS_INCONSISTENT_TEST_ID,
    HIL_APPLICATION_STATUS_INCONSISTENT_TICK,
    HIL_APPLICATION_STATUS_INCOMPLETE_DATA,
    HIL_APPLICATION_STATUS_VALIDATION_FAILED,
    HIL_APPLICATION_STATUS_NOT_IMPLEMENTED,
    HIL_APPLICATION_STATUS_INTERNAL_ERROR,
    ...
} HIL_Application_Status_T;

/* Public value layouts are checked against C by API-mode CFFI. CFFI 1.17
 * cannot nest partial structs in the public unnamed body union. These value
 * records therefore use complete fields and literal array extents (verified
 * against compiled constants in tests); no padding is declared manually.
 * Context and the outer message remain compiler-resolved partial structs.
 */
/* application_types.h */

typedef struct
{
    uint8_t bytes[16];
} HIL_Application_Test_Id_T;

typedef struct
{
    const uint8_t* data;
    uint8_t size;
} HIL_Application_Byte_Span_T;

typedef enum
{
    HIL_APPLICATION_PERIPHERAL_INVALID,
    HIL_APPLICATION_PERIPHERAL_DIGITAL_INPUT,
    HIL_APPLICATION_PERIPHERAL_DIGITAL_OUTPUT,
    HIL_APPLICATION_PERIPHERAL_ANALOG_INPUT,
    HIL_APPLICATION_PERIPHERAL_ANALOG_OUTPUT,
    HIL_APPLICATION_PERIPHERAL_PWM_INPUT,
    HIL_APPLICATION_PERIPHERAL_PWM_OUTPUT,
    HIL_APPLICATION_PERIPHERAL_UART,
    HIL_APPLICATION_PERIPHERAL_SPI,
    HIL_APPLICATION_PERIPHERAL_I2C,
    HIL_APPLICATION_PERIPHERAL_CAN,
    HIL_APPLICATION_PERIPHERAL_RESERVED,
    ...
} HIL_Application_Peripheral_Type_T;

typedef struct
{
    HIL_Application_Peripheral_Type_T peripheral;
    uint16_t channel;
} HIL_Application_Channel_Id_T;

typedef struct
{
    uint32_t microseconds;
} HIL_Application_Tick_Duration_T;

typedef struct
{
    uint8_t high;
} HIL_Application_Digital_Output_Value_T;

typedef struct
{
    uint8_t high;
} HIL_Application_Digital_Input_Value_T;

typedef struct
{
    uint32_t microvolts;
} HIL_Application_Analog_Output_Value_T;

typedef struct
{
    uint32_t microvolts;
} HIL_Application_Analog_Input_Value_T;

typedef struct
{
    uint32_t period_nanoseconds;
    uint16_t duty_cycle_permyriad;
} HIL_Application_Pwm_Output_Value_T;

typedef struct
{
    uint32_t period_nanoseconds;
    uint16_t duty_cycle_permyriad;
} HIL_Application_Pwm_Input_Value_T;

typedef struct
{
    size_t max_encoded_message_size;
    size_t max_variable_data_size;
    size_t max_variable_transfers_per_tick;
    uint32_t max_expected_tick_count;
    ...;
} HIL_Application_Config_T;

typedef struct
{
    ...;
} HIL_Application_Context_T;

#define HIL_APPLICATION_TEST_ID_SIZE ...
#define HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT ...
#define HIL_APPLICATION_DIGITAL_OUTPUT_CHANNEL_COUNT ...
#define HIL_APPLICATION_DIGITAL_INPUT_CHANNEL_COUNT ...
#define HIL_APPLICATION_ANALOG_OUTPUT_CHANNEL_COUNT ...
#define HIL_APPLICATION_ANALOG_INPUT_CHANNEL_COUNT ...
#define HIL_APPLICATION_PWM_OUTPUT_CHANNEL_COUNT ...
#define HIL_APPLICATION_PWM_INPUT_CHANNEL_COUNT ...
#define HIL_APPLICATION_CAN_CHANNEL_COUNT ...
#define HIL_APPLICATION_UART_CHANNEL_COUNT ...
#define HIL_APPLICATION_SPI_CHANNEL_COUNT ...
#define HIL_APPLICATION_I2C_CHANNEL_COUNT ...

/* application_test_config.h */

typedef enum
{
    HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_INVALID,
    HIL_APPLICATION_PERIPHERAL_CONFIG_3V3,
    HIL_APPLICATION_PERIPHERAL_CONFIG_5V,
    HIL_APPLICATION_PERIPHERAL_CONFIG_12V,
    HIL_APPLICATION_PERIPHERAL_CONFIG_24V,
    HIL_APPLICATION_PERIPHERAL_CONFIG_VOLTAGE_RESERVED,
    ...
} HIL_Application_Peripheral_Config_Voltage_Level_T;

typedef enum
{
    HIL_APPLICATION_BUS_ROLE_INVALID,
    HIL_APPLICATION_BUS_ROLE_MASTER,
    HIL_APPLICATION_BUS_ROLE_SLAVE,
    HIL_APPLICATION_BUS_ROLE_RESERVED,
    ...
} HIL_Application_Bus_Role_T;

typedef enum
{
    HIL_APPLICATION_SPI_DATA_WIDTH_INVALID,
    HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS,
    HIL_APPLICATION_SPI_DATA_WIDTH_16_BITS,
    HIL_APPLICATION_SPI_DATA_WIDTH_RESERVED,
    ...
} HIL_Application_Spi_Data_Width_T;

typedef enum
{
    HIL_APPLICATION_SPI_BIT_ORDER_INVALID,
    HIL_APPLICATION_SPI_BIT_ORDER_MSB_FIRST,
    HIL_APPLICATION_SPI_BIT_ORDER_LSB_FIRST,
    HIL_APPLICATION_SPI_BIT_ORDER_RESERVED,
    ...
} HIL_Application_Spi_Bit_Order_T;

typedef enum
{
    HIL_APPLICATION_SPI_CLOCK_POLARITY_INVALID,
    HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_LOW,
    HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_HIGH,
    HIL_APPLICATION_SPI_CLOCK_POLARITY_RESERVED,
    ...
} HIL_Application_Spi_Clock_Polarity_T;

typedef enum
{
    HIL_APPLICATION_SPI_CLOCK_PHASE_INVALID,
    HIL_APPLICATION_SPI_CLOCK_PHASE_FIRST_EDGE,
    HIL_APPLICATION_SPI_CLOCK_PHASE_SECOND_EDGE,
    HIL_APPLICATION_SPI_CLOCK_PHASE_RESERVED,
    ...
} HIL_Application_Spi_Clock_Phase_T;

typedef enum
{
    HIL_APPLICATION_UART_ELECTRICAL_MODE_INVALID,
    HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3,
    HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_5V,
    HIL_APPLICATION_UART_ELECTRICAL_MODE_RS232,
    HIL_APPLICATION_UART_ELECTRICAL_MODE_RESERVED,
    ...
} HIL_Application_Uart_Electrical_Mode_T;

typedef enum
{
    HIL_APPLICATION_UART_WORD_LENGTH_INVALID,
    HIL_APPLICATION_UART_WORD_LENGTH_8_BITS,
    HIL_APPLICATION_UART_WORD_LENGTH_9_BITS,
    HIL_APPLICATION_UART_WORD_LENGTH_RESERVED,
    ...
} HIL_Application_Uart_Word_Length_T;

typedef enum
{
    HIL_APPLICATION_UART_PARITY_INVALID,
    HIL_APPLICATION_UART_PARITY_NONE,
    HIL_APPLICATION_UART_PARITY_EVEN,
    HIL_APPLICATION_UART_PARITY_ODD,
    HIL_APPLICATION_UART_PARITY_RESERVED,
    ...
} HIL_Application_Uart_Parity_T;

typedef enum
{
    HIL_APPLICATION_UART_STOP_BITS_INVALID,
    HIL_APPLICATION_UART_STOP_BITS_1,
    HIL_APPLICATION_UART_STOP_BITS_2,
    HIL_APPLICATION_UART_STOP_BITS_RESERVED,
    ...
} HIL_Application_Uart_Stop_Bits_T;

typedef enum
{
    HIL_APPLICATION_I2C_VOLTAGE_INVALID,
    HIL_APPLICATION_I2C_VOLTAGE_3V3,
    HIL_APPLICATION_I2C_VOLTAGE_5V,
    HIL_APPLICATION_I2C_VOLTAGE_RESERVED,
    ...
} HIL_Application_I2c_Voltage_Level_T;

typedef enum
{
    HIL_APPLICATION_I2C_PULL_UP_INVALID,
    HIL_APPLICATION_I2C_PULL_UP_1K,
    HIL_APPLICATION_I2C_PULL_UP_2K2,
    HIL_APPLICATION_I2C_PULL_UP_4K7,
    HIL_APPLICATION_I2C_PULL_UP_10K,
    HIL_APPLICATION_I2C_PULL_UP_RESERVED,
    ...
} HIL_Application_I2c_Pull_Up_T;

typedef struct
{
    uint8_t enabled;
    HIL_Application_Peripheral_Config_Voltage_Level_T voltage_level;
} HIL_Application_Digital_Input_Config_T;

typedef struct
{
    uint8_t enabled;
    HIL_Application_Peripheral_Config_Voltage_Level_T voltage_level;
    uint8_t initial_high;
} HIL_Application_Digital_Output_Config_T;

typedef struct
{
    uint8_t enabled;
} HIL_Application_Analog_Input_Config_T;

typedef struct
{
    uint8_t enabled;
} HIL_Application_Analog_Output_Config_T;

typedef struct
{
    uint8_t enabled;
    HIL_Application_Peripheral_Config_Voltage_Level_T voltage_level;
} HIL_Application_Pwm_Input_Config_T;

typedef struct
{
    uint8_t enabled;
    HIL_Application_Peripheral_Config_Voltage_Level_T voltage_level;
    uint32_t initial_period_nanoseconds;
    uint16_t initial_duty_cycle_permyriad;
} HIL_Application_Pwm_Output_Config_T;

typedef struct
{
    uint8_t enabled;
    uint32_t bit_rate;
    uint8_t termination_enabled;
    uint32_t capture_limit_bytes;
} HIL_Application_Can_Config_T;

typedef struct
{
    uint8_t enabled;
    uint32_t bit_rate;
    HIL_Application_Bus_Role_T role;
    HIL_Application_Spi_Data_Width_T data_width;
    HIL_Application_Spi_Bit_Order_T bit_order;
    HIL_Application_Spi_Clock_Polarity_T clock_polarity;
    HIL_Application_Spi_Clock_Phase_T clock_phase;
    uint32_t capture_limit_bytes;
} HIL_Application_Spi_Config_T;

typedef struct
{
    uint8_t enabled;
    uint32_t baud_rate;
    HIL_Application_Uart_Electrical_Mode_T electrical_mode;
    HIL_Application_Uart_Word_Length_T word_length;
    HIL_Application_Uart_Parity_T parity;
    HIL_Application_Uart_Stop_Bits_T stop_bits;
    uint8_t rx_enabled;
    uint8_t tx_enabled;
    uint32_t capture_limit_bytes;
} HIL_Application_Uart_Config_T;

typedef struct
{
    uint8_t enabled;
    uint32_t bit_rate;
    HIL_Application_Bus_Role_T role;
    uint16_t own_address_7bit;
    HIL_Application_I2c_Voltage_Level_T voltage_level;
    HIL_Application_I2c_Pull_Up_T pull_up;
    uint32_t capture_limit_bytes;
} HIL_Application_I2c_Config_T;

typedef struct
{
    HIL_Application_Tick_Duration_T tick_duration_us;
    uint32_t expected_tick_count;
    uint32_t flags;
    HIL_Application_Digital_Input_Config_T digital_in[10];
    HIL_Application_Digital_Output_Config_T digital_out[10];
    HIL_Application_Analog_Input_Config_T analog_in[2];
    HIL_Application_Analog_Output_Config_T analog_out[6];
    HIL_Application_Pwm_Input_Config_T pwm_in[2];
    HIL_Application_Pwm_Output_Config_T pwm_out[2];
    HIL_Application_Can_Config_T can[2];
    HIL_Application_Spi_Config_T spi[2];
    HIL_Application_Uart_Config_T uart[2];
    HIL_Application_I2c_Config_T i2c[2];
    HIL_Application_Byte_Span_T extension_data;
} HIL_Application_Test_Configuration_T;

/* application_instruction.h */

typedef struct
{
    uint32_t tick_number;
    HIL_Application_Digital_Output_Value_T digital_outputs[10];
    HIL_Application_Analog_Output_Value_T analog_outputs[6];
    HIL_Application_Pwm_Output_Value_T pwm_outputs[2];
} HIL_Application_Test_Instruction_T;

/* application_result.h */

typedef enum
{
    HIL_APPLICATION_RESULT_CONDITION_OK,
    HIL_APPLICATION_RESULT_CONDITION_PARTIAL,
    HIL_APPLICATION_RESULT_CONDITION_EXECUTION_PROBLEM,
    HIL_APPLICATION_RESULT_CONDITION_RESERVED,
    ...
} HIL_Application_Result_Condition_T;

typedef struct
{
    uint32_t tick_number;
    HIL_Application_Digital_Input_Value_T digital_inputs[10];
    HIL_Application_Analog_Input_Value_T analog_inputs[2];
    HIL_Application_Pwm_Input_Value_T pwm_inputs[2];
    HIL_Application_Result_Condition_T condition;
    uint32_t problem_detail;
} HIL_Application_Test_Result_T;

/* application_message.h */

typedef enum
{
    HIL_APPLICATION_MESSAGE_TYPE_INVALID,
    HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_REQUEST,
    HIL_APPLICATION_MESSAGE_TYPE_SYSTEM_INFO_RESPONSE,
    HIL_APPLICATION_MESSAGE_TYPE_TEST_CONFIGURATION,
    HIL_APPLICATION_MESSAGE_TYPE_TEST_INSTRUCTION,
    HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_INSTRUCTION_DATA,
    HIL_APPLICATION_MESSAGE_TYPE_EXECUTION_CONTROL,
    HIL_APPLICATION_MESSAGE_TYPE_GLOBAL_CONTROL,
    HIL_APPLICATION_MESSAGE_TYPE_TEST_RESULT,
    HIL_APPLICATION_MESSAGE_TYPE_VARIABLE_RESULT_DATA,
    HIL_APPLICATION_MESSAGE_TYPE_RESPONSE,
    HIL_APPLICATION_MESSAGE_TYPE_ERROR,
    HIL_APPLICATION_MESSAGE_TYPE_RESERVED,
    ...
} HIL_Application_Message_Type_T;

typedef enum
{
    HIL_APPLICATION_MESSAGE_SUBTYPE_NONE,
    HIL_APPLICATION_MESSAGE_SUBTYPE_BASIC,
    HIL_APPLICATION_MESSAGE_SUBTYPE_RESERVED,
    ...
} HIL_Application_Message_Subtype_T;

typedef struct
{
    HIL_Application_Message_Type_T type;
    HIL_Application_Message_Subtype_T subtype;
    uint8_t has_test_id;
    HIL_Application_Test_Id_T test_id;
    union {
        HIL_Application_Test_Configuration_T test_configuration;
        HIL_Application_Test_Instruction_T test_instruction;
        HIL_Application_Test_Result_T test_result;
    } body;
    ...;
} HIL_Application_Message_T;

#define HIL_APPLICATION_ABSOLUTE_BYTE_SPAN_SIZE ...
#define HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_SIZE ...
#define HIL_APPLICATION_ABSOLUTE_MAX_VARIABLE_DATA_COUNT_PTICK ...
#define HIL_APPLICATION_ABSOLUTE_MAX_TICK_COUNT ...
#define HIL_APPLICATION_DEFAULT_MAX_MESSAGE_SIZE ...
#define HIL_APPLICATION_PROTOCOL_MAJOR_SIZE_BYTES ...
#define HIL_APPLICATION_PROTOCOL_MINOR_SIZE_BYTES ...
#define HIL_APPLICATION_MESSAGE_HAS_ID_SIZE_BYTES ...
#define HIL_APPLICATION_MESSAGE_TYPE_SIZE_BYTES ...
#define HIL_APPLICATION_MESSAGE_SUB_TYPE_SIZE_BYTES ...
#define HIL_APPLICATION_HEADER_PAYLOAD_SIZE_BYTES ...
#define HIL_APPLICATION_HEADER_SIZE_BYTES ...
#define HIL_APPLICATION_ABSOLUTE_MAX_MESSAGE_SIZE ...
#define HIL_APPLICATION_MIN_COMPLETE_MESSAGE_SIZE ...

/* Direct public Application entry points; no binding forwarding codec. */

HIL_Application_Status_T HIL_APPLICATION_Default_Config(
    HIL_Application_Config_T* config);

HIL_Application_Status_T HIL_APPLICATION_Init(
    HIL_Application_Context_T* context,
    const HIL_Application_Config_T* config);

HIL_Application_Status_T HIL_APPLICATION_Encoded_Size(
    const HIL_Application_Context_T* context,
    const HIL_Application_Message_T* message,
    size_t* encoded_size);

HIL_Application_Status_T HIL_APPLICATION_Encode_Message(
    const HIL_Application_Context_T* context,
    const HIL_Application_Message_T* message,
    uint8_t* out_buffer,
    size_t out_buffer_size,
    size_t* output_size);

HIL_Application_Status_T HIL_APPLICATION_Decode_Storage_Size(
    const HIL_Application_Context_T* context,
    const uint8_t* encoded_message,
    size_t encoded_message_size,
    size_t* required_storage_size);

HIL_Application_Status_T HIL_APPLICATION_Decode_Message(
    const HIL_Application_Context_T* context,
    const uint8_t* encoded_message,
    size_t encoded_message_size,
    HIL_Application_Message_T* out_message,
    uint8_t* decoded_data,
    size_t max_decoded_data_size,
    size_t* used_decoded_size);

HIL_Application_Status_T HIL_APPLICATION_Validate_Message(
    const HIL_Application_Context_T* context,
    const HIL_Application_Message_T* message);

HIL_Application_Status_T HIL_APPLICATION_Validate_Encoded_Message(
    const HIL_Application_Context_T* context,
    const uint8_t* encoded_message,
    size_t encoded_message_size,
    size_t* required_decode_storage);
"""
