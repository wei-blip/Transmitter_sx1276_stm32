/*
 * rf_test.c
 *
 *  Created on: May 12, 2021
 *      Author: user
 */

#include "rf-test.h"
#include "timer.h"

// #define RTC_TEST
// #define TX_TEST
#define UART_TEST

#ifdef UART_TEST
  uint8_t data_UART[] = {0x00, 0x00, 0x00, 0x00};
#define MAX_COUNT_OF_PACKETS 100
  static bool IsSend = true;
#endif
static uint32_t count = 0;

void init_rf (void);

const uint8_t PingMsg[] = "PING";
const uint8_t PongMsg[] = "PONG";

uint16_t BufferSize = BUFFER_SIZE;
uint8_t Buffer[BUFFER_SIZE];

States_t State = LOWPOWER;

int8_t RssiValue = 0;
int8_t SnrValue = 0;

/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;

/*!
 * LED GPIO pins objects
 */
extern Gpio_t Led1;
extern Gpio_t Led2;

void init_rf (void)
{
  // Target board initialization
  BoardInitMcu( );
  BoardInitPeriph( );

  // Radio initialization
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;

  Radio.Init( &RadioEvents );

  Radio.SetChannel( RF_FREQUENCY );

#if defined( USE_MODEM_LORA )

  Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                 LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                 LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                 true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

  Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                                 LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                                 LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                                 0, true, 0, 0, LORA_IQ_INVERSION_ON, true );

  Radio.SetMaxPayloadLength( MODEM_LORA, BUFFER_SIZE );

#elif defined( USE_MODEM_FSK )

  Radio.SetTxConfig( MODEM_FSK, TX_OUTPUT_POWER, FSK_FDEV, 0,
                                FSK_DATARATE, 0,
                                FSK_PREAMBLE_LENGTH, FSK_FIX_LENGTH_PAYLOAD_ON,
                                true, 0, 0, 0, 3000 );

  Radio.SetRxConfig( MODEM_FSK, FSK_BANDWIDTH, FSK_DATARATE,
                                0, FSK_AFC_BANDWIDTH, FSK_PREAMBLE_LENGTH,
                                0, FSK_FIX_LENGTH_PAYLOAD_ON, 0, true,
                                0, 0,false, true );

  Radio.SetMaxPayloadLength( MODEM_FSK, BUFFER_SIZE );
#endif

}

#ifdef RTC_TEST
TimerEvent_t rtc_tim_1 = {0};

volatile int rtc_tim_1_cnt = 0;

void rtc_tim_1_callback (void)
{
  rtc_tim_1_cnt++;
  TimerStop(&rtc_tim_1);
  TimerSetValue(&rtc_tim_1, 1000);
  TimerStart(&rtc_tim_1);
}
#endif


void ping_pong_rf (void)
{
//  RtcInit();
  bool isMaster = true;
  uint8_t i;
  init_rf( );
#ifdef RTC_TEST
  TimerInit( &rtc_tim_1, rtc_tim_1_callback );
  TimerStop(&rtc_tim_1);
  TimerSetValue(&rtc_tim_1, 1000);
  TimerStart(&rtc_tim_1);
  while(1)
    {

    }
#endif

#ifdef UART_TEST
  State = TX;
  while(1){
      while(IsSend){
	  switch( State )
	  	{
		  case TX:
		  *(uint32_t*)data_UART = count;
		  HAL_Delay(50);
		  Radio.Send( data_UART, sizeof(data_UART) );
		  State = LOWPOWER;
		  break;
		  case RX_TIMEOUT:
		  case TX_TIMEOUT:
		  case RX:
		  case RX_ERROR:
		  case LOWPOWER:
		  default:
		  break;
	 }
      }
      if ( count == MAX_COUNT_OF_PACKETS ) {
	  count = 0;
	  IsSend = false;
      }
  }
#endif
  HAL_GPIO_WritePin(LED_EXT_GPIO_Port, LED_EXT_Pin, RESET);
  Radio.Rx( RX_TIMEOUT_VALUE );

  while( 1 )
     {
         switch( State )
         {
         case RX:
             if( isMaster == true )
             {
                 if( BufferSize > 0 )
                 {
                     if( strncmp( ( const char* )Buffer, ( const char* )PongMsg, 4 ) == 0 )
                     {
                         // Indicates on a LED that the received frame is a PONG
                         GpioToggle( &Led1 );

                         // Send the next PING frame
                         Buffer[0] = 'P';
                         Buffer[1] = 'I';
                         Buffer[2] = 'N';
                         Buffer[3] = 'G';
                         // We fill the buffer with numbers for the payload
                         for( i = 4; i < BufferSize; i++ )
                         {
                             Buffer[i] = i - 4;
                         }
                         DelayMs( 1 );
                         Radio.Send( Buffer, BufferSize );
                     }
                     else if( strncmp( ( const char* )Buffer, ( const char* )PingMsg, 4 ) == 0 )
                     { // A master already exists then become a slave
                         isMaster = false;
                         GpioToggle( &Led2 ); // Set LED off
                         Radio.Rx( RX_TIMEOUT_VALUE );
                     }
                     else // valid reception but neither a PING or a PONG message
                     {    // Set device as master ans start again
                         isMaster = true;
                         Radio.Rx( RX_TIMEOUT_VALUE );
                     }
                 }
             }
             else
             {
                 if( BufferSize > 0 )
                 {
                     if( strncmp( ( const char* )Buffer, ( const char* )PingMsg, 4 ) == 0 )
                     {
                         // Indicates on a LED that the received frame is a PING
                         GpioToggle( &Led1 );

                         // Send the reply to the PONG string
                         Buffer[0] = 'P';
                         Buffer[1] = 'O';
                         Buffer[2] = 'N';
                         Buffer[3] = 'G';
                         // We fill the buffer with numbers for the payload
                         for( i = 4; i < BufferSize; i++ )
                         {
                             Buffer[i] = i - 4;
                         }
                         DelayMs( 1 );
                         Radio.Send( Buffer, BufferSize );
                     }
                     else // valid reception but not a PING as expected
                     {    // Set device as master and start again
                         isMaster = true;
                         Radio.Rx( RX_TIMEOUT_VALUE );
                     }
                 }
             }
             State = LOWPOWER;
             break;
         case TX:
             // Indicates on a LED that we have sent a PING [Master]
             // Indicates on a LED that we have sent a PONG [Slave]
             GpioToggle( &Led2 );
             Radio.Rx( RX_TIMEOUT_VALUE );
             State = LOWPOWER;
             break;
         case RX_TIMEOUT:
//	     State = RX;
//	     break;
         case RX_ERROR:
             if( isMaster == true )
             {
                 // Send the next PING frame
                 Buffer[0] = 'P';
                 Buffer[1] = 'I';
                 Buffer[2] = 'N';
                 Buffer[3] = 'G';
                 for( i = 4; i < BufferSize; i++ )
                 {
                     Buffer[i] = i - 4;
                 }
                 DelayMs( 1 );
                 Radio.Send( Buffer, BufferSize );
             }
             else
             {
                 Radio.Rx( RX_TIMEOUT_VALUE );
             }
             State = LOWPOWER;
             break;
         case TX_TIMEOUT:
             Radio.Rx( RX_TIMEOUT_VALUE );
             State = LOWPOWER;
             break;
         case LOWPOWER:
         default:
             // Set low power
             break;
         }

//         BoardLowPowerHandler( );
         // Process Radio IRQ
         if( Radio.IrqProcess != NULL )
         {
             Radio.IrqProcess( );
         }
     }
}

void OnTxDone( void )
{
    HAL_GPIO_TogglePin(LED_EXT_GPIO_Port, LED_EXT_Pin);
    Radio.Sleep( );
    State = TX;
#ifdef UART_TEST
    count++;
#endif
}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
    HAL_GPIO_TogglePin(LED_EXT_GPIO_Port, LED_EXT_Pin);
    HAL_Delay(1000);
    HAL_GPIO_TogglePin(LED_EXT_GPIO_Port, LED_EXT_Pin);
    Radio.Sleep( );
    BufferSize = size;
    memcpy( Buffer, payload, BufferSize );
    RssiValue = rssi;
    SnrValue = snr;
    State = RX;
#ifdef UART_TEST
    IsSend = true;
#endif
}

void OnTxTimeout( void )
{
    Radio.Sleep( );
    State = TX_TIMEOUT;
}

void OnRxTimeout( void )
{
    Radio.Sleep( );
    State = RX_TIMEOUT;
}

void OnRxError( void )
{
    Radio.Sleep( );
    State = RX_ERROR;
}




