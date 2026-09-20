#include "hil_rig_protocol/application/application.h"
#include "hil_rig_protocol/version.h"

_Static_assert( HIL_APPLICATION_BUS_ROLE_INVALID == 0, "disabled enum value" );
_Static_assert( HIL_APPLICATION_BUS_ROLE_MASTER == 1, "wire enum value" );
_Static_assert( HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS == 1, "wire enum value" );
_Static_assert( HIL_APPLICATION_SPI_DATA_WIDTH_16_BITS == 2, "wire enum value" );
_Static_assert( HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3 == 1, "wire enum value" );
_Static_assert( HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_5V == 2, "wire enum value" );
_Static_assert( HIL_APPLICATION_UART_ELECTRICAL_MODE_RS232 == 3, "wire enum value" );
_Static_assert( HIL_APPLICATION_I2C_PULL_UP_10K == 4, "wire enum value" );
_Static_assert( HIL_APPLICATION_MIN_COMPLETE_MESSAGE_SIZE == 28u, "control minimum" );
_Static_assert( HIL_APPLICATION_STATUS_VERSION_MISMATCH == 17, "stable status value" );
_Static_assert( HIL_APPLICATION_MESSAGE_TYPE_FINALIZE_TEST_UPLOAD == 22,
                "finalize upload wire type" );
_Static_assert( HIL_APPLICATION_MAX_VARIABLE_CHUNKS_PER_TICK == 8u, "fixed v0.3.0 chunk ceiling" );

int main( void )
{
    HIL_Application_Peripheral_Config_Voltage_Level_T voltage =
        HIL_APPLICATION_PERIPHERAL_CONFIG_3V3;
    HIL_Application_Bus_Role_T             role     = HIL_APPLICATION_BUS_ROLE_MASTER;
    HIL_Application_Spi_Data_Width_T       width    = HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS;
    HIL_Application_Spi_Bit_Order_T        order    = HIL_APPLICATION_SPI_BIT_ORDER_MSB_FIRST;
    HIL_Application_Spi_Clock_Polarity_T   polarity = HIL_APPLICATION_SPI_CLOCK_POLARITY_IDLE_LOW;
    HIL_Application_Spi_Clock_Phase_T      phase    = HIL_APPLICATION_SPI_CLOCK_PHASE_FIRST_EDGE;
    HIL_Application_Uart_Electrical_Mode_T electrical =
        HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3;
    HIL_Application_Uart_Word_Length_T      word          = HIL_APPLICATION_UART_WORD_LENGTH_8_BITS;
    HIL_Application_Uart_Parity_T           parity        = HIL_APPLICATION_UART_PARITY_NONE;
    HIL_Application_Uart_Stop_Bits_T        stop          = HIL_APPLICATION_UART_STOP_BITS_1;
    HIL_Application_I2c_Voltage_Level_T     i2c_voltage   = HIL_APPLICATION_I2C_VOLTAGE_3V3;
    HIL_Application_I2c_Pull_Up_T           pull_up       = HIL_APPLICATION_I2C_PULL_UP_1K;
    HIL_Application_Digital_Input_Config_T  digital_input = { 0 };
    HIL_Application_Digital_Output_Config_T digital_output       = { 0 };
    HIL_Application_Analog_Input_Config_T   analog_input         = { 0 };
    HIL_Application_Analog_Output_Config_T  analog_output        = { 0 };
    HIL_Application_Pwm_Input_Config_T      pwm_input            = { 0 };
    HIL_Application_Pwm_Output_Config_T     pwm_output           = { 0 };
    HIL_Application_Can_Config_T            can                  = { 0 };
    HIL_Application_Spi_Config_T            spi                  = { 0 };
    HIL_Application_Uart_Config_T           uart                 = { 0 };
    HIL_Application_I2c_Config_T            i2c                  = { 0 };
    HIL_Application_Test_Configuration_T    configuration        = { 0 };
    HIL_Application_System_Info_Request_T   system_info_request  = { 0 };
    HIL_Application_System_Info_Response_T  system_info_response = { 0 };
    HIL_Application_Execution_Control_T     execution_control    = { 0 };
    HIL_Application_Global_Control_T        global_control       = { 0 };
    HIL_Application_Finalize_Test_Upload_T  finalize_upload      = { 0 };

    ( void )voltage;
    ( void )role;
    ( void )width;
    ( void )order;
    ( void )polarity;
    ( void )phase;
    ( void )electrical;
    ( void )word;
    ( void )parity;
    ( void )stop;
    ( void )i2c_voltage;
    ( void )pull_up;
    ( void )digital_input;
    ( void )digital_output;
    ( void )analog_input;
    ( void )analog_output;
    ( void )pwm_input;
    ( void )pwm_output;
    can.enabled     = 1u;
    can.bit_rate    = 500000u;
    can.filter_id   = 0x123u;
    can.filter_mask = 0x7f0u;
    ( void )can;
    ( void )spi;
    ( void )uart;
    ( void )i2c;
    ( void )configuration;
    ( void )finalize_upload;
    system_info_request.query                       = HIL_APPLICATION_SYSTEM_INFO_QUERY_BASIC;
    system_info_request.application_protocol_major  = HIL_RIG_PROTOCOL_VERSION_MAJOR;
    system_info_request.application_protocol_minor  = HIL_RIG_PROTOCOL_VERSION_MINOR;
    system_info_request.application_protocol_patch  = HIL_RIG_PROTOCOL_VERSION_PATCH;
    system_info_response.application_protocol_major = HIL_RIG_PROTOCOL_VERSION_MAJOR;
    system_info_response.application_protocol_minor = HIL_RIG_PROTOCOL_VERSION_MINOR;
    system_info_response.application_protocol_patch = HIL_RIG_PROTOCOL_VERSION_PATCH;
    execution_control.command                       = HIL_APPLICATION_CONTROL_START;
    global_control.command = HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION;
    if ( HIL_APPLICATION_Check_Protocol_Version( HIL_RIG_PROTOCOL_VERSION_MAJOR,
                                                 HIL_RIG_PROTOCOL_VERSION_MINOR,
                                                 HIL_RIG_PROTOCOL_VERSION_PATCH )
         != HIL_APPLICATION_STATUS_OK )
    {
        return 1;
    }
    return system_info_request.application_protocol_patch == HIL_RIG_PROTOCOL_VERSION_PATCH
                   && system_info_response.application_protocol_minor
                          == HIL_RIG_PROTOCOL_VERSION_MINOR
                   && execution_control.command == HIL_APPLICATION_CONTROL_START
                   && global_control.command == HIL_APPLICATION_GLOBAL_CONTROL_RESET_APPLICATION
               ? 0
               : 1;
}
