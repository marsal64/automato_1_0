// verze firmware:
#define FW_VERSION "automato_1_0"



#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/*
 // FreeRTOSConfig.h
 #ifndef FREERTOS_CONFIG_H
 #define FREERTOS_CONFIG_H
 #define configTIMER_TASK_STACK_DEPTH 2048
 #endif
 */

 #define NUMLASTACTIONSLOG 20
 
// default passwords
#define INIT_PASSWORD_AUTOMATO "000000"
#define INIT_PASSWORD_SERVIS "111111"

//// period to read from OTE in miliseconds
#define PERIOD_OTE_READ_MS  (1000 * 60 * 30)

//// definition for setup (actions)
// define maximum number of conditions
#define MAXNUMCONDITONS 10
#define SIZERULES 33


// jednoznačné device id:
#define deviceid "automato"
// maximum number of retries when provisioning
#define PROV_MGR_MAX_RETRY_CNT 3

//
// ssid prefix pro bluetooth provisioning
#define SSID_PREF "PROV_"
// heslo pro bluetooth provisioning
#define BLE_PASSWORD	"auto1234"
// pokud je PROV_SHOW_QR definováno, používá se QR. Bez QR - řádek zakomentovat
#define PROV_SHOW_QR

//// GPIOS
// RESET_BUTTON GPIO (čtení stavu RESET)
#define WIFI_INIT_BUTTON_GPIO GPIO_NUM_9
// Status LED GPIO Red
#define STATUS_LED_GPIO_RED GPIO_NUM_5
// Status LED GPIO Green
#define STATUS_LED_GPIO_GREEN GPIO_NUM_4
// Status LED GPIO Yellow
#define STATUS_LED_GPIO_YELLOW GPIO_NUM_38
//
//Relays
#define RELAY1 GPIO_NUM_8
#define RELAY2 GPIO_NUM_6
#define RELAY3 GPIO_NUM_7

//// ote_read related definitions
// chunk to read from OTE
#define CHUNK 4096
// tail to keep and merge with next chunk
#define TAIL 512
// interval in minutes after which OTE is regrabbed
#define MINUTES_TO_GRAB_OTE 1

// How long should be pressed the Reset button (ms) to reset initiation
#define WIFI_BUTTON_PUSHMS_TO_INIT 5000

// Test signal active when uart error
#define TEST_UART_GPIO GPIO_NUM_36

// optional jumpers
#define JP1   (GPIO_NUM_40)
#define JP2   (GPIO_NUM_41)

// UART MAX485 definitions
#define RS485_TXD   (GPIO_NUM_1)
#define RS485_RXD   (GPIO_NUM_2)
// RTS for RS485 Half-Duplex Mode manages DE/~RE
#define RS485_RTS   (GPIO_NUM_42)
// CTS is not used in RS485 Half-Duplex Mode
#define RS485_CTS   (UART_PIN_NO_CHANGE)
#define BUF_SIZE        (1024)
#define BAUD_RATE       (9600)
// Read packet timeout
#define RS485_UART_PORT          (1)

//
// RX related parameters
#define MAX_RX_LEN   57		// maximum length of received message, 20241220 změna 54->56   56->57
#define MAINCYCLE_DELAY_MS 30 // used a main cycle pause
#define TIMEOUT_NO_COMMUNICATION_MS 2000  // timeout for "non-communication error"

#define RS485_READ_TOUT          2 // Flush when pause in number of "characters" sent (10 bits)
#define RX_TIMEOUT_TICKS 3 // number of ticks as a timeout value for uart_rx

#define RX_ADDRESS_GENERAL_PACKET 0x01  // address of "general status"
#define RX_ADDRESS_WIFI 0x03  // address of s150U-Wifi for RS485 communication

// deafults
#define PROV_TRANSPORT_BLE      "ble"
#define NUMVALUES 20
#define MAXLEN_NAME 20
#define MAXLEN_VALUE 20

// for language tranlsations
#define MAX_TRANS_STRING_CZ_LEN 200
#define MAX_TRANS_STRING_EN_LEN 180


#define QUEUE_TX_LENGTH 10

#define NUM_CHECKS_MESS_LENGTHS	7
#define MESS_LENGTH_GENERAL	57  // 54, 56 původně
#define MESS_LENGTH_WIFI	5

#define TX_MESSAGE_MAXLEN 128

#define MES_PLAINWIFIPOLL_RESPONSE_LEN 4

#define WIFI_RETRY_MAX_ATTEMPTS 5
#define WIFI_RETRY_DELAY_MS 2000

#define TEXTITEMS_COUNT 10  //maxinum number of text items

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

#define QUEUE_RX_LENGTH 10

#define MAX_TX_LEN_MESSAGE 128
#define MAX_TX_LEN_TYPE 64


#define MAX_ODLO_CAS (60 * 60 * 20 )  //hardcoded !!!
#define MAX_DOBA_BEHU (60 * 60 * 20 )  //hardcoded !!!

#define RESP_SIZE 16384

#define MAX_RETS_LENGTH   1000




 