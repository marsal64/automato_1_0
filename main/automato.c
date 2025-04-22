/*
 * s150u_wifi
 */

// avoid unused variables warnings
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

////////////////////////
#include "definitions.h"
#include "translation.h"
////////////////////////

#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_http_server.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "http_parser.h"
#include "logo_favicon.h"

// Výrobní číslo default
char vyrobnicislo[50] = "20240622-001";

// global static "provisioned" variable
static bool provisioned = false;
// "connected to wifi" status global variable
static bool device_connected = false;

// mac address
uint8_t mac[6];
char mac_string[19];

// RX Queue items
typedef struct {
    int code;
    int length;
    char message[MAX_RX_LEN];
} rx_queue_item;

static QueueHandle_t rx_queue;

// Send queue items

typedef struct {
    int length;
    char message[MAX_TX_LEN_MESSAGE];

} tx_queue_item;

static QueueHandle_t tx_queue;


// Wifi reconnect constants
static int wifi_retry_count = 0;

//// S150U variables start ////////////////////


typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t wDay;
    uint8_t date;
    uint8_t month;
    uint8_t year;
} cas_p;
cas_p btime;  // "base" time (ze sauny)

// názvy dnů
char translatedays[][10] = {"??", "ne", "po", "út", "st", "čt", "pá", "so"};
// "zpracovaná data"
uint8_t i_processed_data = 0;

// additional
static uint8_t nntptime_status;    // 0 - not OK, 1 - OK
static char sntp_string[64];       // cas z internetu
static char saunacas_string[128];  // cas ze sauny
char ipaddress[64];

// sntp time info
static time_t now_sntp = 0;
static struct tm timeinfo_sntp = {0};

// time for transfer
static struct tm timeinfo_transfer = {0};



// způsob nastavení času
enum TYP_NAST_DATCAS {
    DATCAS_NENASTAVOVAT,
    DATCAS_PEVNY,
    DATCAS_INTERNET,
    DATCAS_NUMSTATES
};
static char typy_datcas[DATCAS_NUMSTATES][100] = {
    "Neměnit čas a datum (ponechat stávající)",
    "Pevné nastavení času a data dle zadání výše (sekundy=0)",
    "Synchronizovat čas a datum z internetu (CEST)"};


// Buffer proměnné pro nastavení času
uint16_t year_s = 0;
uint8_t month_s = 0;
uint8_t date_s = 0;
uint8_t wDay_s = 0;
uint8_t hour_s = 0;
uint8_t min_s = 0;
uint8_t sec_s = 0;


// nvs flash handler
static nvs_handle_t nvs_handle_storage;


// static variables for RX receive
// static int r_len;  // received length
// static uint8_t r_buf[128];  // receving buffer

//// S150U variables end////////////////////
//

// two independent LEDs:

// (dual) led 1 states:
enum led1_states_list { LED1_STATUS_OK, LED1_STATUS_ERROR };

// (simple) led 2 states:
enum led2_states_list {
    LED2_STATUS_NO_WIFI,
    LED2_STATUS_CONNECTING_WIFI,
    LED2_STATUS_WIFI_CONNECTED,
    LED2_STATUS_WIFI_REPROVISIONING
};

// variables holding statuses for LEDs, initialization
static int led1_status = LED1_STATUS_ERROR;
static int led2_status = LED2_STATUS_NO_WIFI;

// Definice intervalů svícení nebo blikání led 1
typedef struct {
    uint8_t onoff_red_1;    // red v intervalu 1 on nebo off
    uint8_t onoff_green_1;  // green v intervalu 1 on nebo off
    int interval_ms_1;      // délka intevalu 1
    uint8_t onoff_red_2;    // red v intervalu 2 on nebo off
    uint8_t onoff_green_2;  // green v intervalu 2 on nebo off
    int interval_ms_2;      // délka intevalu 2
} onoffdual;

// Definice intervalů svícení nebo blikání led 2
typedef struct {
    uint8_t onoff_1;    // on nebo off v intervalu 1
    int interval_ms_1;  // délka intervalu 1
    uint8_t onoff_2;    // on nebo off v intervalu 2
    int interval_ms_2;  // délka intervalu 2
} onoffsimple;

onoffdual led1_blink_states[2] = {
    {0, 1, 1000, 0, 1, 1000},  //  LED1_STATUS_OK
    {1, 0, 1000, 1, 0, 1000},  //  LED1_STATUS_ERROR
};

onoffsimple led2_blink_states[4] = {
    {0, 1000, 0, 1000},  //  LED2_STATUS_NO_WIFI
    {1, 500, 0, 500},    //  LED2_STATUS_CONNECTING_WIFI
    {1, 1000, 1, 1000},  //  LED2_STATUS_WIFI_CONNECTED
    {1, 80, 0, 80}       //  LED2_STATUS_WIFI_REPROVISIONING
};

// RS485 communication states list
enum comm_states_list {
    COMST_OK,
    COMST_ERR,
    COMST_TIMEOUT,
};
// application state
static int8_t comm_status;

// Application states list
enum app_states_list {
    APST_NORMAL,
    APST_RS485TIMEOUT,
    APST_RS485_CRCERR,
    APST_OFF,
    APST_STARTING,
    APST_TO_CONFIRM,
    APST_ODLO,
    APST_UNKNOWN
};
// application state
static int8_t apst;

//
// WIFIAPP states list
enum WIFIAPST_states_list {
    WIFIAPST_NORMAL,
    WIFIAPST_SET_TERM,
    WIFIAPST_SET_TIMEOUT,
    WIFIAPST_SET_GENERAL,
    WIFIAPST_SET_ODLO,
    WIFIAPST_SET_RH
};
static int8_t wifiapst;

// credential vars
typedef struct {
    char username[33];
    char password[33];
} user_credentials_t;

// credential definitions
static user_credentials_t users[] = {{"sauna", INIT_PASSWORD_USER},
                                     {"servis", INIT_PASSWORD_ADMIN}};

// t_ structure
typedef struct {
    uint8_t priority;  // 0 - lowest
    char text[500];    // text to be displayed
} t_item;

// credential definitions static variable

static t_item tt[TEXTITEMS_COUNT];

// assign default textitem values

// current user
// -1 - undefined
// 0 - user
// 1 - servis
static int current_user_id = -1;

// run values list
typedef struct {
    char name[MAXLEN_NAME];
    char value[MAXLEN_VALUE];
} named_variable_t;

named_variable_t run_vars[NUMVALUES];

static const char* TAG = "S150U-WIFI";

/* Signal Wi-Fi events on this event-group */
const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

// refresh page request
static uint8_t rfp;

// refresh page request
void utf8_to_ascii(const char* utf8_str, char* ascii_str, size_t ascii_len) {
    size_t i, j;
    for (i = 0, j = 0; i < strlen(utf8_str) && j < ascii_len - 1; ++i) {
        unsigned char c = (unsigned char)utf8_str[i];
        if (c < 128) {
            // ASCII character
            ascii_str[j++] = c;
        } else {
            // Non-ASCII character, replace with '?'
            ascii_str[j++] = '?';
            // Skip continuation bytes of UTF-8 character
            while (i < strlen(utf8_str) && (utf8_str[i + 1] & 0xC0) == 0x80) {
                ++i;
            }
        }
    }
    ascii_str[j] = '\0';
}

/**
 * day_of_week:
 *    Returns the day of week for a given date in the Gregorian calendar.
 *    0 = Monday, 1 = Tuesday, ..., 6 = Sunday.
 *
 *    year, month, day must be:
 *      - year: full year, e.g. 2023
 *      - month: 1 .. 12
 *      - day: 1 .. 31 (depending on month)
 */
int day_of_week(int year, int month, int day) {
    // Sakamoto's "month offsets" for each month (Jan..Dec)
    static int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) {
        year--;
    }
    // Sakamoto’s formula produces: 0 = Sunday, 1 = Monday, ..., 6 = Saturday
    int w =
        (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
    // shift to achieve Sunday = 1
    return w + 1;
}

/**
 * @brief calculate simple crc
 */
uint8_t crc_calc(uint8_t* src, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc += src[i];
    }
    return 256 - crc;
}

// Utility function to parse cookies
bool get_cookie(httpd_req_t* req, const char* cookie_name, char* cookie_value,
                size_t max_len) {
    char buf[256];  // Cookie length
    if (httpd_req_get_hdr_value_str(req, "Cookie", buf, sizeof(buf)) ==
        ESP_OK) {
        const char* start = strstr(buf, cookie_name);
        if (start) {
            const char* end = strstr(start, ";");
            if (!end) end = buf + strlen(buf);
            size_t len = end - (start + strlen(cookie_name) + 1);
            if (len < max_len) {
                strncpy(cookie_value, start + strlen(cookie_name) + 1, len);
                cookie_value[len] = '\0';
                return true;
            }
        }
    }
    return false;
}

/* Event handler for catching system events */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    static int retries;

    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                led2_status = LED2_STATUS_WIFI_REPROVISIONING;
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t* wifi_sta_cfg =
                    (wifi_sta_config_t*)event_data;

                ESP_LOGI(TAG,
                         "Received Wi-Fi credentials"
                         "\n\tSSID     : %s\n\tPassword : %s",
                         (const char*)wifi_sta_cfg->ssid,
                         (const char*)wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t* reason =
                    (wifi_prov_sta_fail_reason_t*)event_data;
                ESP_LOGE(TAG,
                         "Provisioning failed!\n\tReason : %s"
                         "\n\tPlease reset to factory and retry provisioning",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR)
                             ? "Wi-Fi station authentication failed"
                             : "Wi-Fi access-point not found");

                led2_status = LED2_STATUS_NO_WIFI;

                retries++;
                if (retries >= PROV_MGR_MAX_RETRY_CNT) {
                    ESP_LOGI(TAG,
                             "Failed to connect with provisioned AP, reseting "
                             "provisioned credentials");
                    wifi_prov_mgr_reset_sm_state_on_failure();

                    led2_status = LED2_STATUS_NO_WIFI;

                    retries = 0;
                }

                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                led2_status = LED2_STATUS_CONNECTING_WIFI;
                wifi_prov_mgr_is_provisioned(&provisioned);

                retries = 0;

                break;
            case WIFI_PROV_END:
                /* De-initialize manager once provisioning is finished */
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:

                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:

                device_connected = false;
                led2_status = LED2_STATUS_CONNECTING_WIFI;

                if (wifi_retry_count < WIFI_RETRY_MAX_ATTEMPTS) {
                    ESP_LOGI(TAG,
                             "Disconnected. Retrying to connect to the AP...");
                    esp_wifi_connect();
                    wifi_retry_count++;
                    vTaskDelay(WIFI_RETRY_DELAY_MS / portTICK_PERIOD_MS);
                } else {
                    ESP_LOGI(TAG,
                             "Failed to connect to the AP after %d attempts. "
                             "Restarting...",
                             WIFI_RETRY_MAX_ATTEMPTS);
                    esp_restart();
                }
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_retry_count = 0;  // Reset retry count upon successful connection
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR,
                 IP2STR(&event->ip_info.ip));

        // Convert binary IP address to string and put to ipaddress static
        // variable
        snprintf(ipaddress, sizeof(ipaddress), IPSTR,
                 IP2STR(&event->ip_info.ip));

        /* Signal main application to continue execution */
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);

        led2_status = LED2_STATUS_WIFI_CONNECTED;
        device_connected = true;
    } else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        switch (event_id) {
            case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
                ESP_LOGI(TAG, "BLE transport: Connected!");
                break;
            case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
                ESP_LOGI(TAG, "BLE transport: Disconnected!");
                break;
            default:
                break;
        }
    } else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        switch (event_id) {
            case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
                ESP_LOGI(TAG, "Secured session established!");
                break;
            case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
                ESP_LOGE(
                    TAG,
                    "Received invalid security parameters for establishing "
                    "secure session!");
                break;
            case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
                ESP_LOGE(
                    TAG,
                    "Received incorrect username and/or PoP for establishing "
                    "secure session!");
                break;
            default:
                break;
        }
    }
}

static void wifi_init_sta(void) {
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void get_device_service_name(char* service_name, size_t max) {
    uint8_t eth_mac[6];
    const char* ssid_prefix = SSID_PREF;
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3],
             eth_mac[4], eth_mac[5]);
}

/**
 * @brief obsluhuje dvojbarevnou LED 1 (OK/ERROR)
 */
void wifi_led_1(void*) {
    while (1) {
        gpio_set_level(STATUS_LED_GPIO_RED,
                       led1_blink_states[led1_status].onoff_red_1);
        gpio_set_level(STATUS_LED_GPIO_GREEN,
                       led1_blink_states[led1_status].onoff_green_1);
        vTaskDelay(led1_blink_states[led1_status].interval_ms_1 /
                   portTICK_PERIOD_MS);
        gpio_set_level(STATUS_LED_GPIO_RED,
                       led1_blink_states[led1_status].onoff_red_2);
        gpio_set_level(STATUS_LED_GPIO_GREEN,
                       led1_blink_states[led1_status].onoff_green_2);
        vTaskDelay(led1_blink_states[led1_status].interval_ms_2 /
                   portTICK_PERIOD_MS);
    }
}

/**
 * @brief obsluhuje jednoduchou LED 2 (WiFi Status)
 */
void wifi_led_2(void*) {
    while (1) {
        gpio_set_level(STATUS_LED_GPIO_YELLOW,
                       led2_blink_states[led2_status].onoff_1);
        vTaskDelay(led2_blink_states[led2_status].interval_ms_1 /
                   portTICK_PERIOD_MS);
        gpio_set_level(STATUS_LED_GPIO_YELLOW,
                       led2_blink_states[led2_status].onoff_2);
        vTaskDelay(led2_blink_states[led2_status].interval_ms_2 /
                   portTICK_PERIOD_MS);
    }
}

/////////////////////////////////////////////////// web
///////////////////////////////////////////////////
static void init_variables(void) {
    for (int i = 0; i < NUMVALUES; i++) {
        char pstring[MAXLEN_NAME];
        snprintf(pstring, MAXLEN_NAME, "Par. %d", i);
        strcpy(run_vars[i].name, pstring);
        char vstring[MAXLEN_NAME];
        snprintf(vstring, MAXLEN_NAME, "%d", i + 5);
        strcpy(run_vars[i].value, vstring);
    }
}

// All handlers in separate file
#include "automato_url_handlers.h"

int r_parse_status() {
    // ESP_LOGI(TAG, "parsing response");

    return 0;  // OK response
}

void obtain_time(void*) {
    while (1) {
        // Wait for time to be set
        int retry = 0;
        const int retry_count = 10;
        time(&now_sntp);
        localtime_r(&now_sntp, &timeinfo_sntp);
        while (timeinfo_sntp.tm_year < (2023 - 1900) && ++retry < retry_count) {
            ESP_LOGI(TAG, "Cekani na nastaveni casu z internetu... (%d/%d)",
                     retry, retry_count);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            time(&now_sntp);
            localtime_r(&now_sntp, &timeinfo_sntp);
        }

        // Parse date and time
        if (timeinfo_sntp.tm_year > (2023 - 1900)) {
            snprintf(sntp_string, sizeof(sntp_string),
                     "%d.%d.%d %02d:%02d:%02d", timeinfo_sntp.tm_mday,
                     timeinfo_sntp.tm_mon + 1, timeinfo_sntp.tm_year + 1900,
                     timeinfo_sntp.tm_hour, timeinfo_sntp.tm_min,
                     timeinfo_sntp.tm_sec);
            // ESP_LOGW(TAG, "Datum/cas z internetu: %s", sntp_string);
            nntptime_status = 1;  // OK
        } else {
            ESP_LOGI(TAG, "Datum/cas z internetu: nespravny");
            nntptime_status = 0;  // not OK
        }

        // wait to the next poll
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief function to log received buffer in hex
 */
void hexlogger(uint8_t* dt, int lendt) {
    char ldata[1024];  // buffer for logging
    snprintf(ldata, sizeof(ldata), "%d bytes: ", lendt);
    for (int i = 0; i < lendt; i++) {
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "0x%.2X ", dt[i]);
        strcat(ldata, buffer);
    }
    ESP_LOGI(TAG, "%s", ldata);
};


static QueueHandle_t uart0_queue;

static void uart_event_task(void* pvParameters) {
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*)malloc(RD_BUF_SIZE);

    // rx queue
    rx_queue_item qi;

    char mes_buf[120];

    for (;;) {
        // Waiting for UART event.
        if (xQueueReceive(uart0_queue, (void*)&event,
                          (TickType_t)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            // ESP_LOGI(TAG, "uart[%d] event:", RS485_UART_PORT);

            qi.code = event.type;

            switch (event.type) {
                // Event of UART receving data
                /*We'd better handler data event fast, there would be much more
                 data events than other types of events. If we take too much
                 time on data event, the queue might be full.*/
                case UART_DATA:
                    // ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                    // uart_read_bytes(RS485_UART_PORT, dtmp, event.size,
                    // 5/portTICK_PERIOD_MS);
                    uart_read_bytes(RS485_UART_PORT, dtmp, event.size, 2);
                    // push to queue - normal data received
                    qi.length = event.size;
                    memcpy(qi.message, dtmp, event.size);
                    // uart_flush_input(RS485_UART_PORT);
                    break;
                    // Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    // ESP_LOGI(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding
                    // flow control for your application. The ISR has already
                    // reset the rx FIFO, As an example, we directly flush the
                    // rx buffer here in order to read more data.
                    uart_flush_input(RS485_UART_PORT);
                    xQueueReset(uart0_queue);
                    strcpy(qi.message, "HW FIFO overflow, flushed UART queue");
                    qi.length = strlen(qi.message);
                    break;
                    // Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    // ESP_LOGI(TAG, "ring buffer full");
                    // If buffer full happened, you should consider increasing
                    // your buffer size As an example, we directly flush the rx
                    // buffer here in order to read more data.
                    uart_flush_input(RS485_UART_PORT);
                    xQueueReset(uart0_queue);
                    strcpy(qi.message, "UART buffer full, flushed UART queue");
                    qi.length = strlen(qi.message);
                    break;
                    // Event of UART RX break detected
                case UART_BREAK:
                    // ESP_LOGI(TAG, "uart rx break");
                    strcpy(qi.message, "UART RX break");
                    qi.length = strlen(qi.message);
                    break;
                    // Event of UART frame error
                case UART_FRAME_ERR:
                    strcpy(qi.message, "UART frame error");
                    qi.length = strlen(qi.message);
                    break;
                default:
                    // ESP_LOGI(TAG, "uart event type: %d", event.type);
                    strcpy(qi.message, "UART event of other type");
                    qi.length = strlen(qi.message);
                    break;
            }

            // send preparsed message
            if (xQueueSend(rx_queue, &qi, portMAX_DELAY) != pdPASS) {
                ESP_LOGE(TAG, "Failed to send data to the RX queue");
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief task for sending data from tx queue to uart
 */
void uart_tx_task(void* pvParameters) {
    tx_queue_item mo;
    while (1) {
        // Wait for a message from the queue
        // verify without peek
        if (xQueuePeek(tx_queue, &mo, portMAX_DELAY)) {
            // Send this type of message directly to UART
            xQueueReceive(tx_queue, &mo, portMAX_DELAY);
            uart_write_bytes(RS485_UART_PORT, mo.message, mo.length);

            // !!! log sent message
            // ESP_LOGI(TAG, "->Sent:");
            // hexlogger((uint8_t *) mo.message, mo.length);
        }
    }
}

void receive_task(void* pvParameters) {
    // rx queue
    rx_queue_item qi;

    while (1) {
        // wait for RX data (with timeout TIMEOUT_NO_COMMUNICATION_MS)
        int rec_q = xQueueReceive(
            rx_queue, &qi, TIMEOUT_NO_COMMUNICATION_MS / portTICK_PERIOD_MS);

        // timeout?
        if (!rec_q) {
            ESP_LOGE(TAG, "Timeout komunikace se zakladni jednotkou");
            comm_status = COMST_TIMEOUT;

            // pro jistotu
            uart_flush(RS485_UART_PORT);
            while (xQueueReceive(rx_queue, &qi, 0) == pdPASS) {
            }

            continue;
        }

        // !!! logging what was received
        // hexlogger((uint8_t *)qi.message, qi.length);

        // something other than data?
        if (qi.code != UART_DATA) {
            ESP_LOGE(TAG, "Nestandardni udalost pri prijimani zprav, kod: %d",
                     qi.code);
            hexlogger((uint8_t*)qi.message, qi.length);
            comm_status = COMST_ERR;

            // try to recover and continue
            uart_flush(RS485_UART_PORT);
            while (xQueueReceive(rx_queue, &qi, 0) == pdPASS) {
            }

            continue;
        }
        // Data received
        comm_status = COMST_OK;

        // verify CRC
        uint8_t crc = 0;

        for (int i = 0; i < qi.length - 1; i++) {
            crc += qi.message[i];
        }
        // add crc
        crc += qi.message[qi.length - 1];

        // crc = 0?
        if (crc) {
            // crc error
            ESP_LOGE(TAG, "Nespravny kontrolni soucet zpravy:");
            hexlogger((uint8_t*)qi.message, qi.length);
            comm_status = COMST_ERR;
            uart_flush(RS485_UART_PORT);
            while (xQueueReceive(rx_queue, &qi, 0) == pdPASS) {
            }
            continue;
        }

        // just for debug:
        // hexlogger((uint8_t*) qi.message, qi.length);
        ESP_LOG_BUFFER_HEXDUMP(TAG, qi.message, qi.length, ESP_LOG_INFO);

    }
}
/**
 * @brief main app
 */
void app_main(void) {
    // Set log level
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    // initial technical delay (service purposes only)
    vTaskDelay(1500 / portTICK_PERIOD_MS);

    // reset pins for UART
    gpio_reset_pin(RS485_TXD);
    gpio_reset_pin(RS485_RXD);

    // reset UART test pin
    gpio_reset_pin(TEST_UART_GPIO);
    gpio_set_direction(TEST_UART_GPIO, GPIO_MODE_OUTPUT);

    // reset pins for optional pads
    gpio_reset_pin(JP1);
    gpio_set_direction(JP1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(JP1, GPIO_PULLUP_ONLY);
    gpio_reset_pin(JP2);
    gpio_set_direction(JP2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(JP2, GPIO_PULLUP_ONLY);

    // reset Status leds
    gpio_reset_pin(STATUS_LED_GPIO_RED);
    gpio_set_direction(STATUS_LED_GPIO_RED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(STATUS_LED_GPIO_GREEN);
    gpio_set_direction(STATUS_LED_GPIO_GREEN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(STATUS_LED_GPIO_YELLOW);
    gpio_set_direction(STATUS_LED_GPIO_YELLOW, GPIO_MODE_OUTPUT);

    // reset Wifi init button
    gpio_reset_pin(WIFI_INIT_BUTTON_GPIO);
    gpio_set_direction(WIFI_INIT_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(WIFI_INIT_BUTTON_GPIO, GPIO_PULLUP_ONLY);

    // initialize run variables
    init_variables();

    // Register WiFi led tasks
    xTaskCreatePinnedToCore(wifi_led_1, "wifi_led_1",
                            configMINIMAL_STACK_SIZE * 5, NULL,
                            configMAX_PRIORITIES - 4, NULL, 1);
    xTaskCreatePinnedToCore(wifi_led_2, "wifi_led_2",
                            configMINIMAL_STACK_SIZE * 5, NULL,
                            configMAX_PRIORITIES - 4, NULL, 1);

    // initialize queue for the transfer of UART events
    rx_queue = xQueueCreate(QUEUE_RX_LENGTH, sizeof(rx_queue_item));

    // initialize queue for the transfer of responses
    tx_queue = xQueueCreate(QUEUE_TX_LENGTH, sizeof(tx_queue_item));

    // initialize UART
    uart_config_t uart_config = {.baud_rate = BAUD_RATE,
                                 .data_bits = UART_DATA_8_BITS,
                                 .parity = UART_PARITY_DISABLE,
                                 .stop_bits = UART_STOP_BITS_1,
                                 .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                 .rx_flow_ctrl_thresh = MAX_RX_LEN,
                                 .source_clk = UART_SCLK_DEFAULT};
    // In this example we don't even use a buffer for sending data.
    ESP_ERROR_CHECK(uart_driver_install(RS485_UART_PORT, BUF_SIZE * 2,
                                        BUF_SIZE * 2, 20, &uart0_queue, 0));
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(RS485_UART_PORT, &uart_config));
    // Set UART pins
    ESP_ERROR_CHECK(uart_set_pin(RS485_UART_PORT, RS485_TXD, RS485_RXD,
                                 RS485_RTS, RS485_CTS));
    // Set RS485 half duplex mode
    ESP_ERROR_CHECK(
        uart_set_mode(RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));
    // uart_set_mode(RS485_UART_PORT, UART_MODE_UART)

    // Set read timeout of UART TOUT feature
    ESP_ERROR_CHECK(uart_set_rx_timeout(RS485_UART_PORT, RS485_READ_TOUT));

    xTaskCreatePinnedToCore(uart_event_task, "uart_event_task", 8192, NULL,
                            configMAX_PRIORITIES - 2, NULL, 1);

    // inicializuj NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    // Set timezone to CET/CEST for Prague
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    // sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // Register obtain time task
    xTaskCreatePinnedToCore(obtain_time, "obtain_time",
                            configMINIMAL_STACK_SIZE * 5, NULL,
                            configMAX_PRIORITIES - 4, NULL, 1);

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    /* Register our event handler for Wi-Fi, IP and Provisioning related
     * events
     */
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT,
                                               ESP_EVENT_ANY_ID, &event_handler,
                                               NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &event_handler, NULL));

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM};

    /* Initialize provisioning manager with the
     * configuration parameters set above */
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    /* Let's find out if the device is provisioned */
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    // if not provisioned, goto directly to provisioning, else verify
    // provisioning button
    bool button_reprovisioning = false;
    if (provisioned) {
        // Wait for reset button release and count how long pressed
        ESP_LOGI(TAG, "Wait for Reset button release: %d",
                 gpio_get_level(WIFI_INIT_BUTTON_GPIO));
        // počet 100 ms intervalů testu
        int times = WIFI_BUTTON_PUSHMS_TO_INIT / 100;

        while (1) {
            // tlačítko není stisknuto? Break
            if (gpio_get_level(WIFI_INIT_BUTTON_GPIO)) break;
            // dekrementuj test
            if (!gpio_get_level(WIFI_INIT_BUTTON_GPIO)) {
                times--;
                // stisknuto dostatečně dlouho?
                if (times == 0) {
                    button_reprovisioning = true;
                    break;
                }
                // čekej 100 ms
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }
    }

    // otevři "storage"
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle_storage));

    // po reset vymaž existující hesla
    if (button_reprovisioning) {
        ESP_LOGI(TAG, "Vymaz stavajicich hesel uzivatelu sauna a servis");
        nvs_erase_key(nvs_handle_storage, "password_sauna");
        nvs_erase_key(nvs_handle_storage, "password_servis");
    }

    // načti aplikační hesla z nvs
    char read_nvs_value[128];
    size_t required_size = sizeof(read_nvs_value);
    //
    int err = nvs_get_str(nvs_handle_storage, "password_sauna", read_nvs_value,
                          &required_size);
    if (err == ESP_OK) {
        strcpy(users[0].password, read_nvs_value);
        ESP_LOGI(TAG, "Heslo pro uzivatele sauna nacteno z nvs");
        // ESP_LOGI(TAG, "Heslo pro sauna nacteno: %s", users[0].password);
    } else {
        ESP_LOGI(TAG, "Nacteno standardni heslo pro unizavele sauna");
    }
    //
    err = nvs_get_str(nvs_handle_storage, "password_servis", read_nvs_value,
                      &required_size);
    if (err == ESP_OK) {
        strcpy(users[1].password, read_nvs_value);
        ESP_LOGI(TAG, "Heslo pro uzivatele servis nacteno z nvs");
        // ESP_LOGI(TAG, "Heslo pro servis nacteno: %s", users[1].password);
    } else {
        ESP_LOGI(TAG, "Nacteno standardni heslo pro uzivatele servis");
    }

    // výrobní číslo: pokud existuje, načti z nvs, jinak standardní
    err = nvs_get_str(nvs_handle_storage, "production_num", read_nvs_value,
                      &required_size);
    if (err == ESP_OK) {
        strcpy(vyrobnicislo, read_nvs_value);
        ESP_LOGI(TAG, "Vyrobni cislo %s nacteno z nvs", vyrobnicislo);
        // ESP_LOGI(TAG, "Heslo pro servis nacteno: %s", users[1].password);
    } else {
        ESP_LOGI(TAG, "Vyrobni cislo nenacteno, standardni %s", vyrobnicislo);
        // zapis standardního výrobního čísla do nvs
        err = nvs_set_str(nvs_handle_storage, "production_num", vyrobnicislo);
        if (err == ESP_OK) {
        } else {
            ESP_LOGE(TAG,
                     "Chyba pri zapisu standardniho vyrobniho cisla do nvs");
        }
    }
    /* If device is not yet provisioned or reprovisioning button pressed long,
     * start provisioning service */
    if ((!provisioned) | button_reprovisioning) {
        led2_status = LED2_STATUS_WIFI_REPROVISIONING;

        // wifi provisioning
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

        ESP_LOGI(TAG, "Starting provisioning");

        /* What is the Device Service Name that we want
         * This translates to :
         *     - Wi-Fi SSID when scheme is wifi_prov_scheme_softap
         *     - device name when scheme is wifi_prov_scheme_ble
         */
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));

        // inicializované heslo
        // vyžadované neslo při přihlašování
        wifi_prov_security1_params_t* sec_params = BLE_PASSWORD;

        const char* service_key = NULL;

        /* This step is only useful when scheme is wifi_prov_scheme_ble. This
         * will set a custom 128 bit UUID which will be included in the BLE
         * advertisement and will correspond to the primary GATT service that
         * provides provisioning endpoints as GATT characteristics. Each GATT
         * characteristic will be formed using the primary service UUID as base,
         * with different auto assigned 12th and 13th bytes (assume counting
         * starts from 0th byte). The client side applications must identify the
         * endpoints by reading the User Characteristic Description descriptor
         * (0x2901) for each characteristic, which contains the endpoint name of
         * the characteristic */
        uint8_t custom_service_uuid[] = {
            /* LSB <---------------------------------------
             * ---------------------------------------> MSB */
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);

        /* Start provisioning service */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
            security, (const void*)sec_params, service_name, service_key));
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

        /* We don't need the manager as device is already provisioned,
         * so let's release it's resources */
        wifi_prov_mgr_deinit();

        /* Start Wi-Fi station */
        led2_status = LED2_STATUS_CONNECTING_WIFI;
        wifi_init_sta();
    }

    /* Wait for Wi-Fi connection */
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true,
                        portMAX_DELAY);

    led2_status = LED2_STATUS_WIFI_CONNECTED;

    // after S150u_wifi reset: request refresh of root web page
    /*
    ESP_LOGI(TAG,
             "Setting rfp to 1: // after S150u_wifi reset: request "
             "refresh of root web page ");
    */
    rfp = 1;

    // start webserver
    start_webserver();

    // initial RX flush and clean the queue
    //  rx queue
    rx_queue_item qi;

    uart_flush(RS485_UART_PORT);
    while (xQueueReceive(rx_queue, &qi, 0) == pdPASS) {
    }

    // initialize the send task
    xTaskCreatePinnedToCore(uart_tx_task, "uart_tx_task",
                            configMINIMAL_STACK_SIZE * 5, NULL,
                            configMAX_PRIORITIES - 2, NULL, 1);

    // initialize the send receive cycle
    xTaskCreatePinnedToCore(receive_task, "send_receive_task",
                            configMINIMAL_STACK_SIZE * 5, NULL,
                            configMAX_PRIORITIES - 3, NULL, 1);

    // everything processed, turn status led green
    ESP_LOGI(TAG, "Inicializace dokoncena, zahajen hlavni cyklus aplikace");

    // initi some statuses
    wifiapst = WIFIAPST_NORMAL;

    // get MAC address
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    snprintf(mac_string, sizeof(mac_string), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // dummy ticks cycle
    while (1) {
        // diagnostics 1: list all items in nvs "storage"

        /*

         nvs_handle_t phandle;
         esp_err_t err = nvs_open("storage", NVS_READONLY, &phandle);
         nvs_iterator_t it = NULL;
         esp_err_t res = nvs_entry_find("nvs", "storage", NVS_TYPE_ANY, &it);
         while (res == ESP_OK) {
         nvs_entry_info_t info;
         nvs_entry_info(it, &info); // Can omit error check if parameters are
         guaranteed to be non-NULL ESP_LOGI(TAG, "***NVS: key '%s', type '%d'",
         info.key, info.type); if (info.type == 33) { char gstr[50]; size_t
         required_size = 50; ESP_ERROR_CHECK( nvs_get_str(phandle, info.key,
         gstr, &required_size)); ESP_LOGI(TAG, "***NVS string: '%s'", gstr);
         }
         res = nvs_entry_next(&it);
         }
         nvs_release_iterator(it);

         */

        //
        // diagnostics 2: list all namespaces in the NVS partition
        /*
         nvs_iterator_t it2 = NULL;
         nvs_entry_info_t info2;
         // Iterate through all NVS entries
         nvs_entry_find(NVS_DEFAULT_PART_NAME, NULL, NVS_TYPE_ANY, &it2);

         ESP_LOGI(TAG,"\n\n***nvs entries:");
         while (it2 != NULL) {
         nvs_entry_info(it2, &info2);
         ESP_LOGI(TAG,"namespace_name/key/type: %s/%s/%d",
         info2.namespace_name,info2.key, info2.type); nvs_entry_next(&it2);
         }
         nvs_release_iterator(it2);

         */

        // construct  LED1 status
        // !!! all conditions must be met here for LED1_STATUS_OK:
        if (comm_status == COMST_OK && provisioned && device_connected) {
            led1_status = LED1_STATUS_OK;
        } else {
            led1_status = LED1_STATUS_ERROR;
        }

        // keepalive status message
        ESP_LOGI(TAG,"ip: %s, MAC: %s, "
                 "vyr.c.: %s, chyby: comm=%d "
                 "wifi_prov=%d "
                 "wifi_conn=%d",
                 ipaddress, mac_string, vyrobnicislo, comm_status, !provisioned,
                 !device_connected);


        // development - helper logging

        /*
         (apst != APST_SET_GENERAL && apst != APST_SET_TERM
         && apst != APST_SET_TIMEOUT) {
         */
        ESP_LOGI(TAG, "===================== apst:%d  wifiapst:%d", apst,
                 wifiapst);

        /*
      ESP_LOGI(TAG, "stavRelat 0x%X", stavRelat);
      */
        /*
         ESP_LOGI(TAG, "typSauny %d", typSauny);
         */
        /*
        ESP_LOGI(TAG, "battZmerena %d", battZmerena);
        ESP_LOGI(TAG, "battVolt %d", battVolt);
        */

        /*
        ESP_LOGI(TAG, "setup_korekceCidla: %d", setup_korekceCidla);
        ESP_LOGI(TAG, "setup_nastaveniKamen: %d", setup_nastaveniKamen);
        ESP_LOGI(TAG, "setup_typSauny: %d", setup_typSauny);
        ESP_LOGI(TAG, "setup_stridaniFazi: %d", setup_stridaniFazi);
        ESP_LOGI(TAG, "setup_blokovaniSvetla: %d", setup_blokovaniSvetla);
        ESP_LOGI(TAG, "setup_nastavenaTemp: %d", setup_nastavenaTemp);
        ESP_LOGI(TAG, "setup_dalkoveOvladani: %d", setup_dalkoveOvladani);
        ESP_LOGI(TAG, "setup_parniEsence: %d", setup_parniEsence);
        ESP_LOGI(TAG, "setup_intervalEsence: %d", setup_intervalEsence);
        ESP_LOGI(TAG, "setup_typSpusteni: %d", setup_typSpusteni);
        ESP_LOGI(TAG, "setup_celkovaDobaProvozu: %lu",
                 setup_celkovaDobaProvozu);
        ESP_LOGI(TAG, "setup_provozniDoba: %d", setup_provozniDoba);
*/

        // next tick pause+
        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }
}
