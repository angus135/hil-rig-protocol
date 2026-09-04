#include "hil_rig_protocol/application/application.h"

_Static_assert( HIL_APPLICATION_BUS_ROLE_INVALID == 0, "disabled enum value" );
_Static_assert( HIL_APPLICATION_BUS_ROLE_MASTER == 1, "wire enum value" );
_Static_assert( HIL_APPLICATION_SPI_DATA_WIDTH_8_BITS == 1, "wire enum value" );
_Static_assert( HIL_APPLICATION_SPI_DATA_WIDTH_16_BITS == 2, "wire enum value" );
_Static_assert( HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_3V3 == 1, "wire enum value" );
_Static_assert( HIL_APPLICATION_UART_ELECTRICAL_MODE_TTL_5V == 2, "wire enum value" );
_Static_assert( HIL_APPLICATION_UART_ELECTRICAL_MODE_RS232 == 3, "wire enum value" );
_Static_assert( HIL_APPLICATION_I2C_PULL_UP_10K == 4, "wire enum value" );

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
    HIL_Application_Digital_Output_Config_T digital_output = { 0 };
    HIL_Application_Analog_Input_Config_T   analog_input   = { 0 };
    HIL_Application_Analog_Output_Config_T  analog_output  = { 0 };
    HIL_Application_Pwm_Input_Config_T      pwm_input      = { 0 };
    HIL_Application_Pwm_Output_Config_T     pwm_output     = { 0 };
    HIL_Application_Can_Config_T            can            = { 0 };
    HIL_Application_Spi_Config_T            spi            = { 0 };
    HIL_Application_Uart_Config_T           uart           = { 0 };
    HIL_Application_I2c_Config_T            i2c            = { 0 };
    HIL_Application_Test_Configuration_T    configuration  = { 0 };

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
    ( void )can;
    ( void )spi;
    ( void )uart;
    ( void )i2c;
    ( void )configuration;
    return 0;
}
