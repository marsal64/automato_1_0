/*
 * automato.c
 */

// avoid unused variables warnings
// #pragma GCC diagnostic ignored "-Wunused-variable"
// #pragma GCC diagnostic ignored "-Wunused-function"

////////////////////////
#include "definitions.h"
#include "translation.h"
////////////////////////

#include <ctype.h>
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

#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/uart.h"
// #include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "http_parser.h"
#include "logo_favicon.h"
#include "mdns.h"

//// Výrobní číslo default
char vyrobnicislo[50] = "20250505-001";

//////////////////
//// Run variables
//////////////////
// Run rrrrmmhh, taken from current time incl. timezone and summer time
char r_rrrrmmddhh[100] = "-1";

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

// názvy dnů
char translatedays[][10] = {"ne", "po", "út", "st", "čt", "pá", "so"};
// "zpracovaná data"
uint8_t i_processed_data = 0;

// additional
static uint8_t nntptime_status = 0;  // 0 - not OK, 1 - OK
static char sntp_string[64];         // cas z internetu
char ipaddress[64];

// sntp time info
static time_t now_sntp = 0;
static struct tm timeinfo_sntp = {0};

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
static user_credentials_t users[] = {{"automato", INIT_PASSWORD_AUTOMATO}, {"servis", INIT_PASSWORD_SERVIS}};

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

// condition
typedef struct {
    uint8_t active;    // condition active (1) or not (0)
    char left[33];     // left side of condition
    char operator[3];  // operator (==, !=, <, >, <=, >=)
    char right[33];    // right side of condition
    char action[33];   // action to be done if condition is true

} condition_item;
condition_item conditions[MAXNUMCONDITONS] = {
    {1, "EUROTE", ">", "0", "REL1ON"},
    {1, "EUROTE", ">", "0", "REL2ON"},
    {1, "EUROTE", ">", "0", "REL3ON"},
    {1, "EUROTE", ">", "0", "REDLEDON"},
    {1, "EUROTE", "<=", "0", "REL1OFF"},
    {1, "EUROTE", "<=", "0", "REL2OFF"},
    {1, "EUROTE", "<=", "0", "REL3OFF"},
    {1, "EUROTE", "<=", "0", "REDLEDOFF"},
    {1, "", "", "", ""}  // end of conditions
};  // end of conditions list

// action_log
typedef struct {
    char action[255];    // action
    char timestamp[50];  // timestamp
} action_log_item;

action_log_item last_actions_log[NUMLASTACTIONSLOG];

// logging tag
static const char* TAG = "automato";

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

static esp_err_t load_or_init_conditions(void) {
    esp_err_t err;
    size_t sz = sizeof(conditions); /* how many bytes we expect */

    /* try to read the binary blob *************************************/
    err = nvs_get_blob(nvs_handle_storage, "conditions", conditions, &sz);

    if (err == ESP_OK && sz == sizeof(conditions)) {
        ESP_LOGI(TAG, "Conditions table loaded from NVS (size %zu B)", sz);
        return ESP_OK; /* happy path              */
    }

    /* either the key was missing OR size/version mismatch  *************/
    ESP_LOGW(TAG,
             "Conditions not found (or wrong size, err=%d). "
             "Writing factory defaults to NVS …",
             err);

    /* write the compile‑time defaults you have in `conditions[]`       */
    err = nvs_set_blob(nvs_handle_storage, "conditions", conditions, sizeof(conditions));
    if (err == ESP_OK) err = nvs_commit(nvs_handle_storage);

    if (err == ESP_OK)
        ESP_LOGI(TAG, "Factory defaults stored.");
    else
        ESP_LOGE(TAG, "Storing defaults failed (%d)", err);

    return err;
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
    int w = (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
    // shift to achieve Sunday = 1
    return w + 1;
}

static void wifi_init_sta(void) {
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
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
bool get_cookie(httpd_req_t* req, const char* cookie_name, char* cookie_value, size_t max_len) {
    char buf[256];  // Cookie length
    if (httpd_req_get_hdr_value_str(req, "Cookie", buf, sizeof(buf)) == ESP_OK) {
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
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    static int retries;

    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                led2_status = LED2_STATUS_WIFI_REPROVISIONING;
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t* wifi_sta_cfg = (wifi_sta_config_t*)event_data;

                ESP_LOGI(TAG,
                         "Received Wi-Fi credentials"
                         "\n\tSSID     : %s\n\tPassword : %s",
                         (const char*)wifi_sta_cfg->ssid, (const char*)wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t* reason = (wifi_prov_sta_fail_reason_t*)event_data;
                ESP_LOGE(TAG,
                         "Provisioning failed!\n\tReason : %s"
                         "\n\tPlease reset to factory and retry provisioning",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed"
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
                ESP_LOGI(TAG, "Provisioning done – rebooting to normal mode");

                /* 1. De-initialise the manager (closes BLE, frees heap) */
                wifi_prov_mgr_deinit();

                /* 2. Make sure any pending NVS writes are flushed       */
                nvs_commit(nvs_handle_storage);

                /* 3. Give UART time to flush log lines (optional)       */
                vTaskDelay(pdMS_TO_TICKS(200));

                /* 4. Reboot                                            */
                esp_restart();

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
                    ESP_LOGI(TAG, "Disconnected. Retrying to connect to the AP...");
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
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));

        // Convert binary IP address to string and put to ipaddress static
        // variable
        snprintf(ipaddress, sizeof(ipaddress), IPSTR, IP2STR(&event->ip_info.ip));

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
                ESP_LOGE(TAG,
                         "Received invalid security parameters for establishing "
                         "secure session!");
                break;
            case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
                ESP_LOGE(TAG,
                         "Received incorrect username and/or PoP for establishing "
                         "secure session!");
                break;
            default:
                break;
        }
    }
}

static void get_device_service_name(char* service_name, size_t max) {
    uint8_t eth_mac[6];
    const char* ssid_prefix = SSID_PREF;
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

/**
 * @brief obsluhuje LED 1 (green - OK)
 */
void wifi_led_1(void*) {
    while (1) {
        gpio_set_level(STATUS_LED_GPIO_GREEN, led1_blink_states[led1_status].onoff_green_1);
        vTaskDelay(led1_blink_states[led1_status].interval_ms_1 / portTICK_PERIOD_MS);
        gpio_set_level(STATUS_LED_GPIO_GREEN, led1_blink_states[led1_status].onoff_green_2);
        vTaskDelay(led1_blink_states[led1_status].interval_ms_2 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief obsluhuje jednoduchou LED 2 (WiFi Status)
 */
void wifi_led_2(void*) {
    while (1) {
        gpio_set_level(STATUS_LED_GPIO_YELLOW, led2_blink_states[led2_status].onoff_1);
        vTaskDelay(led2_blink_states[led2_status].interval_ms_1 / portTICK_PERIOD_MS);
        gpio_set_level(STATUS_LED_GPIO_YELLOW, led2_blink_states[led2_status].onoff_2);
        vTaskDelay(led2_blink_states[led2_status].interval_ms_2 / portTICK_PERIOD_MS);
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

// Helper - load or init actions
static esp_err_t load_or_init_actions(void) {
    esp_err_t err;
    size_t sz = sizeof(actions);

    err = nvs_get_blob(nvs_handle_storage, "actions", actions, &sz);

    if (err == ESP_OK && sz == sizeof(actions)) {
        ESP_LOGI(TAG, "Action-descriptions loaded from NVS (%zu B)", sz);
        return ESP_OK;
    }

    ESP_LOGW(TAG,
             "Action-descriptions not found (or wrong size, err=%d). "
             "Writing factory defaults to NVS …",
             err);

    err = nvs_set_blob(nvs_handle_storage, "actions", actions, sizeof(actions));
    if (err == ESP_OK) err = nvs_commit(nvs_handle_storage);

    if (err == ESP_OK)
        ESP_LOGI(TAG, "Factory defaults for actions stored.");
    else
        ESP_LOGE(TAG, "Storing defaults failed (%d)", err);

    return err;
}

/* ------------------------------------------------------------------
 *  Action‑log helper
 * ------------------------------------------------------------------*/
static void log_action(const char* action)
/* keeps last_actions_log[] alphabetically sorted and time‑stamped        */
{
    /* 1. make timestamp "YYYY‑MM‑DD HH:MM:SS" ------------------------- */
    char ts[20];
    struct tm tm_now;
    localtime_r(&now_sntp, &tm_now); /* uses your SNTP time */
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);

    /* 2. search for existing entry or free slot ---------------------- */
    int free_idx = -1, hit_idx = -1;
    for (int i = 0; i < NUMLASTACTIONSLOG; ++i) {
        if (last_actions_log[i].action[0] == 0) { /* empty row */
            if (free_idx == -1) free_idx = i;
        } else if (strcmp(last_actions_log[i].action, action) == 0) {
            hit_idx = i;
            break;
        }
    }

    /* 3. update or create ------------------------------------------- */
    int idx = (hit_idx != -1) ? hit_idx : free_idx;
    if (idx != -1) { /* have room */
        strncpy(last_actions_log[idx].action, action, sizeof(last_actions_log[idx].action) - 1);
        last_actions_log[idx].action[sizeof(last_actions_log[idx].action) - 1] = '\0';

        strncpy(last_actions_log[idx].timestamp, ts, sizeof(last_actions_log[idx].timestamp) - 1);
        last_actions_log[idx].timestamp[sizeof(last_actions_log[idx].timestamp) - 1] = '\0';
    }
    /* else: table full and action new – nothing is logged (or replace
       the oldest/last entry here if you prefer)                       */

    /* 4. simple alphabetic sort (bubble ≤10 items is fine) ----------- */
    /* 4. two-key sort:  newest timestamp first, then action A->Z -------- */
    for (int i = 0; i < NUMLASTACTIONSLOG - 1; ++i)
        for (int j = i + 1; j < NUMLASTACTIONSLOG; ++j)
            if (last_actions_log[i].action[0] && last_actions_log[j].action[0]) {
                int dt = strcmp(last_actions_log[j].timestamp, last_actions_log[i].timestamp); /* >0 ⇒ j newer */

                if (dt > 0 || /* j is newer   */
                    (dt == 0 &&
                     strcmp(last_actions_log[j].action, last_actions_log[i].action) < 0)) { /* same time → A-Z */

                    action_log_item tmp = last_actions_log[i];
                    last_actions_log[i] = last_actions_log[j];
                    last_actions_log[j] = tmp;
                }
            }
}

/**
 *  obtain_time()  ── SNTP + offline fallback
 *
 *  – Tries SNTP first (as before).
 *  – When SNTP succeeds it
 *        • stores the epoch in RAM + in NVS (“rtc_epoch”)
 *        • remembers the uptime (esp_timer)
 *        • sets nntptime_status = 1   ← “internet-accurate”
 *  – If SNTP is still down:
 *        • derives the current epoch from the stored base-time +
 *          the elapsed uptime
 *        • sets nntptime_status = 2   ← “offline-running”
 *        • (value 0 still means “invalid / not initialised”)
 *
 *  Accuracy: ±(crystal drift).  For ESP-IDF default RC-fast clock this is
 *  ≈ ±40 ppm → < 3½ s / day, usually better with an external crystal.
 */

static time_t rtc_epoch = 0;       // last good epoch (UTC seconds)
static int64_t rtc_uptime_us = 0;  // esp_timer at that moment
static const char* NVS_RTC_KEY = "rtc_epoch";

static void obtain_time(void* pv) {
    /* --- first boot: restore last known epoch from NVS --------------- */
    if (rtc_epoch == 0) {
        nvs_get_i64(nvs_handle_storage, NVS_RTC_KEY,
                    &rtc_epoch);  // ignore error
        if (rtc_epoch) {
            struct timeval tv = {.tv_sec = rtc_epoch, .tv_usec = 0};
            settimeofday(&tv, NULL);  // seed the kernel clock
            rtc_uptime_us = esp_timer_get_time();
            nntptime_status = 2;  // “offline-running”
            ESP_LOGW(TAG, "RTC fallback: time restored from NVS (%lld)", rtc_epoch);
        }
    }

    /* --- main loop --------------------------------------------------- */
    while (1) {
        /* 1. Did SNTP give us a fresh stamp? -------------------------- */
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            time(&now_sntp);  // kernel clock is already set by SNTP
            rtc_epoch = now_sntp;
            rtc_uptime_us = esp_timer_get_time();

            /* persist for the next reboot */
            nvs_set_i64(nvs_handle_storage, NVS_RTC_KEY, rtc_epoch);
            nvs_commit(nvs_handle_storage);

            nntptime_status = 1;  // internet-accurate
            ESP_LOGI(TAG, "SNTP sync OK → %lld", rtc_epoch);
        }
        /* 2. No internet – derive time from uptime -------------------- */
        else if (rtc_epoch) {
            int64_t diff_us = esp_timer_get_time() - rtc_uptime_us;
            now_sntp = rtc_epoch + (time_t)(diff_us / 1000000LL);
            nntptime_status = 2;  // offline-running
        }
        /* 3. Still nothing – leave nntptime_status = 0 ---------------- */
        else {
            now_sntp = 0;  // stays 1970-01-01
        }

        /* convert to struct tm for the rest of the program */
        localtime_r(&now_sntp, &timeinfo_sntp);

        /* ASCII version for logging / web */
        strftime(sntp_string, sizeof(sntp_string), "%Y-%m-%d %H:%M:%S", &timeinfo_sntp);

        /* debug: show the mode we’re in */
        ESP_LOGD(TAG, "time: %s  (status %d)", sntp_string, nntptime_status);

        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 s tick
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

    for (;;) {
        // Waiting for UART event.
        if (xQueueReceive(uart0_queue, (void*)&event, (TickType_t)portMAX_DELAY)) {
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
        int rec_q = xQueueReceive(rx_queue, &qi, TIMEOUT_NO_COMMUNICATION_MS / portTICK_PERIOD_MS);

        // timeout?
        if (!rec_q) {
            // ESP_LOGI(TAG, "Timeout komunikace RS485");
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
            ESP_LOGE(TAG, "Non standard event during messages receive, code: %d", qi.code);
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
            ESP_LOGE(TAG, "Incorrect control summ of message:");
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
 * @brief initialize mdns
 */
void start_mdns_service() {
    // initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "MDNS Init failed: %d\n", err);
        return;
    }

    // set hostname
    mdns_hostname_set("automato");
    // set default instance
    mdns_instance_name_set("automato");

    mdns_service_add(
        "Automato HTTP",    // service instance name
        "_http",            // service type
        "_tcp",             // protocol
        80,                 // service port
        NULL,               // TXT records (none)
        0                   // number of TXT records
    );


}

// Actions
// Parameter must be string ended by /0
static void do_action(char* action) {
    log_action(action); /* <-- NEW line, first inside the function */

    if (strcmp(action, "REL1ON") == 0) {
        ESP_LOGI(TAG, "Relay1 ON");
        gpio_set_level(RELAY1, 1);
    } else if (strcmp(action, "REL1OFF") == 0) {
        ESP_LOGI(TAG, "Relay1 OFF");
        gpio_set_level(RELAY1, 0);
    } else if (strcmp(action, "REL2ON") == 0) {
        ESP_LOGI(TAG, "Relay2 ON");
        gpio_set_level(RELAY2, 1);
    } else if (strcmp(action, "REL2OFF") == 0) {
        ESP_LOGI(TAG, "Relay2 OFF");
        gpio_set_level(RELAY2, 0);
    } else if (strcmp(action, "REL3ON") == 0) {
        ESP_LOGI(TAG, "Relay3 ON");
        gpio_set_level(RELAY3, 1);
    } else if (strcmp(action, "REL3OFF") == 0) {
        ESP_LOGI(TAG, "Relay3 OFF");
        gpio_set_level(RELAY3, 0);
    } else if (strcmp(action, "REDLEDON") == 0) {
        ESP_LOGI(TAG, "%s", "Red LED ON");
        gpio_set_level(STATUS_LED_GPIO_RED, 1);
    } else if (strcmp(action, "REDLEDOFF") == 0) {
        ESP_LOGI(TAG, "%s", "Red led OFF");
        gpio_set_level(STATUS_LED_GPIO_RED, 0);
    } else {
        ESP_LOGW(TAG, "Unknown action: %s", action);
    }
}

/* helper: replace all occurrences of TOKEN in FIELD with REPL --- */
static void substitute_token(char* field, size_t field_sz, const char* token, const char* repl) {
    char tmp[255]; /* same size as field   */
    char* dst = tmp;
    const char* src = field;
    size_t tok_len = strlen(token);
    size_t repl_len = strlen(repl);

    while (*src && (dst - tmp) < (int)field_sz - 1) {
        const char* hit = strstr(src, token);

        /* copy text before next token (or whole tail if none) */
        size_t chunk = hit ? (size_t)(hit - src) : strlen(src);
        if (chunk > field_sz - 1 - (dst - tmp)) /* space left?      */
            chunk = field_sz - 1 - (dst - tmp);
        memcpy(dst, src, chunk);
        dst += chunk;
        src += chunk;

        /* no more tokens?  copy tail done */
        if (!hit) break;

        /* copy replacement text */
        size_t copy = repl_len;
        if (copy > field_sz - 1 - (dst - tmp)) copy = field_sz - 1 - (dst - tmp);
        memcpy(dst, repl, copy);
        dst += copy;

        src += tok_len; /* skip the token        */
    }
    *dst = '\0';

    /* write back into original field */
    strncpy(field, tmp, field_sz - 1);
    field[field_sz - 1] = '\0';
}

static const char* g_cur; /* global cursor into the string */

/* --- helpers ---------------------------------------------------- */
static void skip_ws(void) {
    while (isspace((unsigned char)*g_cur)) ++g_cur;
}

static double parse_number(void) {
    skip_ws();
    char* end;
    double val = strtod(g_cur, &end);
    if (end == g_cur) { /* nothing parsed */
        fprintf(stderr, "eval: number expected near '%s'\n", g_cur);
        return 0.0;
    }
    g_cur = end;
    return val;
}

/* factor : [ + | - ] number ------------------------------------- */
static double parse_factor(void) {
    skip_ws();
    int sign = 1;
    while (*g_cur == '+' || *g_cur == '-') { /* handle repeated signs */
        if (*g_cur == '-') sign = -sign;
        ++g_cur;
        skip_ws();
    }
    return sign * parse_number();
}

/* term   : factor { (* | /) factor }* ---------------------------- */
static double parse_term(void) {
    double val = parse_factor();
    for (;;) {
        skip_ws();
        if (*g_cur == '*') {
            ++g_cur;
            val *= parse_factor();
        } else if (*g_cur == '/') {
            ++g_cur;
            double denom = parse_factor();
            if (denom == 0.0)
                fprintf(stderr, "eval: divide by zero\n");
            else
                val /= denom;
        } else
            break;
    }
    return val;
}

/* expr   : term { (+ | -) term }* -------------------------------- */
static double parse_expr_internal(void) {
    double val = parse_term();
    for (;;) {
        skip_ws();
        if (*g_cur == '+') {
            ++g_cur;
            val += parse_term();
        } else if (*g_cur == '-') {
            ++g_cur;
            val -= parse_term();
        } else
            break;
    }
    return val;
}

/* public entry: returns result; ok==0 means syntax error --------- */
double eval_expr(const char* s, int* ok) {
    g_cur = s;
    double v = parse_expr_internal();
    skip_ws();
    if (*g_cur != '\0') { /* garbage at end */
        fprintf(stderr, "eval: unexpected char '%c' near '%s'\n", *g_cur, g_cur);
        if (ok) *ok = 0;
    } else if (ok)
        *ok = 1;
    return v;
}

// Task to evaluate variables, conditions and do actions
static void evaluate_do(void* pv) {
    while (1) {
        //////// evaluate variables
        /*
                r_rrrrmmddhh - current rrrrmmddhh
                current_ote_price - price for rrrrmmddhh read from nvs
        */

        // find current rrrrmmddhh
        if (nntptime_status == 1 || nntptime_status == 2) {
            snprintf(r_rrrrmmddhh, sizeof(r_rrrrmmddhh), "%d%02d%02d%02d",
                     timeinfo_sntp.tm_year + 1900,  // RRRR
                     timeinfo_sntp.tm_mon + 1,      // MM
                     timeinfo_sntp.tm_mday,         // DD
                     timeinfo_sntp.tm_hour + 1);    // HH 1-24

            ESP_LOGI(TAG, "Current r_rrrrmmhh for evaluation:%s", r_rrrrmmddhh);

        } else {
            ESP_LOGW(TAG, "NTP time not set because of not valid nntptime_status");
        }

        //// find current price from nvs
        float current_ote_price;
        char current_ote_price_str[16] = {0};  // string for current price

        /* Build key "o_yyyymmddhh" from the already‑computed timestamp */
        char key[120];
        snprintf(key, sizeof(key), "o_%s", r_rrrrmmddhh); /* e.g. o_2025050516 */

        /* Try to fetch the price string */
        size_t sz = sizeof(current_ote_price_str);
        esp_err_t err = nvs_get_str(nvs_handle_storage, key, current_ote_price_str, &sz);

        // change price only if valid time
        if (err == ESP_OK && (nntptime_status == 1 || nntptime_status == 2)) {
            // load price to float
            current_ote_price = atof(current_ote_price_str);
            ESP_LOGI(TAG, "Current price found in NVS:  %f", current_ote_price);
        } else {
            strcpy(current_ote_price_str, "--");
            if (err == ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Price for %s not found in NVS", key);
            } else if (err == ESP_OK) {
                /* we *have* the price, just no authoritative time */
                ESP_LOGI(TAG, "Price present but time not yet authoritative");
            } else {
                ESP_LOGE(TAG, "nvs_get_str(%s) failed (%d)", key, err);
            }
        }
        // make working copy of conditions;
        condition_item w[MAXNUMCONDITONS];
        memcpy(w, conditions, sizeof(w));

        ///// tokens substitution
        for (int i = 0; i < MAXNUMCONDITONS; ++i) {
            /* terminator row? */
            if (w[i].left[0] == '\0' && w[i].right[0] == '\0') break;

            // EUROTE
            substitute_token(w[i].left, sizeof(w[i].left), "EUROTE", current_ote_price_str);
            substitute_token(w[i].right, sizeof(w[i].right), "EUROTE", current_ote_price_str);
        }

        //// conditions evaluation cycle

        if ((nntptime_status == 1 || nntptime_status == 2) && strcmp(current_ote_price_str, "--") != 0) {
            /* evaluate only if certain conditions are met:
            - nntptime_status == 1 (valid time)
            - current_ote_price_str != "--" (valid price)
            */

            for (int i = 0; w[i].left[0] || w[i].right[0]; ++i) {
                if (!w[i].active) continue;

                int ok_l, ok_r;
                double left_val = eval_expr(w[i].left, &ok_l);
                double right_val = eval_expr(w[i].right, &ok_r);
                if (!ok_l || !ok_r) {
                    ESP_LOGE(TAG, "Invalid expression: %s %s %s", w[i].left, w[i].operator, w[i].right);
                    continue; /* skip invalid lines */
                }

                bool result = false;
                const char* op = w[i].operator;

                if (strcmp(op, "=") == 0)
                    result = (left_val == right_val);
                else if (strcmp(op, "!=") == 0)
                    result = (left_val != right_val);
                else if (strcmp(op, "<") == 0)
                    result = (left_val < right_val);
                else if (strcmp(op, "<=") == 0)
                    result = (left_val <= right_val);
                else if (strcmp(op, ">") == 0)
                    result = (left_val > right_val);
                else if (strcmp(op, ">=") == 0)
                    result = (left_val >= right_val);

                if (result) {
                    do_action(w[i].action); /* your existing function */
                }
            }
        }
        //
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/* helper:  "YYYYMMDDHH" → time_t (returns 0 on error) */
static time_t ymdh_to_time(const char* s) {
    int y, m, d, h;
    if (sscanf(s, "%4d%2d%2d%2d", &y, &m, &d, &h) != 4) return 0;

    struct tm t = {0};
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_isdst = -1;   /* let mktime() figure DST */
    return mktime(&t); /* 0 if date is invalid    */
}

// Task to grab & log the OTE data every minute
static void ote_read(void* pv) {
    const char* URL = "https://www.ote-cr.cz/cs/kratkodobe-trhy/elektrina/denni-trh";

    char* chunk = heap_caps_malloc(CHUNK, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!chunk) {
        ESP_LOGE(TAG, "Memory missing when trying to allocate buffer for websraping");
        vTaskDelete(NULL);
    }

    char tail[TAIL];
    size_t tail_len = 0;

    char yyyymmdd[9] = {0};  /* result date             */
    char prices[24][16];     /* text prices             */
    bool price_ok[24] = {0}; /* flags                   */

    for (;;) {
        memset(price_ok, 0, sizeof(price_ok));
        yyyymmdd[0] = '\0';
        tail_len = 0;

        esp_http_client_config_t cfg = {.url = URL, .timeout_ms = 10000, .user_agent = "automato/grabber"};
        esp_http_client_handle_t c = esp_http_client_init(&cfg);

        // fetch headers, possibly activate unzip
        if (esp_http_client_open(c, 0) == ESP_OK) {
            // fetch header
            if (esp_http_client_fetch_headers(c) < 0) {
                ESP_LOGE(TAG, "fetch_headers failed");
                esp_http_client_close(c);
                continue;
            }

            while (1) {
                int r = esp_http_client_read(c, chunk, CHUNK);

                // ESP_LOG_BUFFER_CHAR_LEVEL(TAG, chunk, r, ESP_LOG_WARN);

                if (r == ESP_ERR_HTTP_EAGAIN) {
                    vTaskDelay(1);
                    continue;
                }
                if (r < 0) {
                    ESP_LOGE(TAG, "read %d", r);
                    break;
                }
                if (r == 0) break; /* body done */

                /* build working buffer = tail + fresh data  --------------
                 */
                static char buf[TAIL + CHUNK + 1];
                size_t n = 0;
                memcpy(buf + n, tail, tail_len);
                n += tail_len;
                memcpy(buf + n, chunk, r);
                n += r;
                buf[n] = '\0';

                /* ---------- 1. date ------------------------------------
                 */
                if (!yyyymmdd[0]) {
                    const char* anchor = strstr(buf, "Výsledky denního trhu");
                    if (anchor) {
                        const char* p = anchor;
                        while (*p && !isdigit((unsigned char)*p)) ++p;
                        if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1]) && p[2] == '.' &&
                            isdigit((unsigned char)p[3]) && isdigit((unsigned char)p[4]) && p[5] == '.' &&
                            isdigit((unsigned char)p[6]) && isdigit((unsigned char)p[7]) &&
                            isdigit((unsigned char)p[8]) && isdigit((unsigned char)p[9])) {
                            yyyymmdd[0] = p[6];
                            yyyymmdd[1] = p[7];
                            yyyymmdd[2] = p[8];
                            yyyymmdd[3] = p[9];
                            yyyymmdd[4] = p[3];
                            yyyymmdd[5] = p[4];
                            yyyymmdd[6] = p[0];
                            yyyymmdd[7] = p[1];
                            yyyymmdd[8] = '\0';
                            ESP_LOGI(TAG, "OTE date parsed: %s", yyyymmdd);
                        }
                    }
                }

                /* ---------- 2. prices ----------------------------------
                 */
                const char* scan = buf;
                while ((scan = strstr(scan, "<th")) != NULL) {
                    const char* gt = strchr(scan, '>');
                    if (!gt) break;
                    const char* endth = strstr(gt, "</th>");
                    if (!endth) break;

                    /* extract hour number */
                    char num[4] = {0};
                    size_t numlen = endth - (gt + 1);
                    if (numlen > 3) numlen = 3;
                    memcpy(num, gt + 1, numlen);
                    int hour = atoi(num);
                    if (hour < 1 || hour > 24 || price_ok[hour - 1]) {
                        scan = endth + 5;
                        continue;
                    }

                    /* find next <td …>PRICE</td> after </th> ------------
                     */
                    const char* td = strstr(endth, "<td");
                    if (!td) break;
                    const char* gt2 = strchr(td, '>');
                    if (!gt2) break;
                    const char* endtd = strstr(gt2, "</td>");
                    if (!endtd) break;

                    /* copy price, trim spaces */
                    const char* s = gt2 + 1;
                    while (s < endtd && isspace((unsigned char)*s)) ++s;
                    const char* e = endtd;
                    while (e > s && isspace((unsigned char)*(e - 1))) --e;

                    size_t plen = e - s;
                    if (plen >= sizeof(prices[0])) plen = sizeof(prices[0]) - 1;
                    memcpy(prices[hour - 1], s, plen);
                    prices[hour - 1][plen] = '\0';
                    for (char* c = prices[hour - 1]; *c; ++c)
                        if (*c == ',') *c = '.';
                    price_ok[hour - 1] = true;

                    if (yyyymmdd[0]) {
                        char ts[13];
                        snprintf(ts, sizeof(ts), "%s%02d", yyyymmdd, hour);
                        ESP_LOGI(TAG, "OTE price grabbed from web for %s: %s", ts, prices[hour - 1]);

                        // refresh/save to nvs
                        {
                            char key[16]; /* "o_yyyymmddhh"   */
                            snprintf(key, sizeof(key), "o_%s%02d", yyyymmdd, hour);

                            /* read the value, if it exists */
                            char old_val[16] = {0};
                            size_t sz = sizeof(old_val);
                            esp_err_t err = nvs_get_str(nvs_handle_storage, key, old_val, &sz);

                            /* store when key is missing OR value differs */
                            if (err == ESP_ERR_NVS_NOT_FOUND ||                      /* not in
                                                                                        NVS   */
                                (err == ESP_OK && strcmp(old_val, prices[hour - 1])) /* different */
                            ) {
                                ESP_LOGI(TAG, "NVS update %s: \"%s\" → \"%s\"", key,
                                         (err == ESP_OK) ? old_val : "<none>", prices[hour - 1]);

                                err = nvs_set_str(nvs_handle_storage, key, prices[hour - 1]);
                                if (err == ESP_OK) {
                                    nvs_commit(nvs_handle_storage); /* make it
                                                                       stick */
                                } else {
                                    ESP_LOGE(TAG, "nvs_set_str %s failed (%d)", key, err);
                                }
                            } else {
                                ESP_LOGI(TAG,
                                         "NVS %s not updated, same value "
                                         "%s found",
                                         key, old_val);
                            }
                        }
                    }

                    //
                    scan = endtd + 5;
                }

                /* keep last 512 B for next loop -------------------------
                 */
                tail_len = n >= TAIL ? TAIL : n;
                memcpy(tail, buf + n - tail_len, tail_len);
            }
            esp_http_client_close(c);
        }
        esp_http_client_cleanup(c);

        //
        for (int h = 1; h <= 24; ++h)
            if (!price_ok[h - 1]) ESP_LOGW(TAG, "Hour %d not parsed from OTE", h);

        // Tidy up old values from NVS, if present
        {
            /* 1. current timestamp from r_rrrrmmddhh ------------------- */
            time_t now_ts = ymdh_to_time(r_rrrrmmddhh);
            if (now_ts == 0) {
                ESP_LOGE(TAG, "Failed to parse r_rrrrmmddhh=\"%s\"", r_rrrrmmddhh);
            } else {
                bool erased = false;

                /* 2. walk through all string keys in namespace "storage" */
                nvs_iterator_t it = NULL;
                esp_err_t res = nvs_entry_find(NVS_DEFAULT_PART_NAME, "storage", NVS_TYPE_STR, &it);
                while (res == ESP_OK) {
                    nvs_entry_info_t info;
                    nvs_entry_info(it, &info);

                    /* keys of interest: "o_YYYYMMDDHH" (12 chars total) */
                    if (strncmp(info.key, "o_", 2) == 0 && strlen(info.key) == 12) {
                        time_t key_ts = ymdh_to_time(info.key + 2);
                        if (key_ts && difftime(now_ts, key_ts) > 48 * 3600) {
                            ESP_LOGW(TAG, "Erasing old NVS key %s", info.key);
                            nvs_erase_key(nvs_handle_storage, info.key);
                            erased = true;
                        }
                    }
                    res = nvs_entry_next(&it);
                }
                nvs_release_iterator(it);

                if (erased) nvs_commit(nvs_handle_storage); /* one flash op */
            }
        }

        // Verify delay
        int delay_ms = PERIOD_OTE_READ_MS;  // keep standard rhythm

        if (nntptime_status == 1 || nntptime_status == 2)  // time is valid
        {
            /* a fresh, thread-safe copy of the current civil time -------- */
            struct tm now_tm;
            memcpy(&now_tm, &timeinfo_sntp,
                   sizeof(now_tm));  //  <-- already updated by obtain_time()

            /* Have we NOT reached 13:01 yet? ----------------------------- */
            if (now_tm.tm_hour < 13 || (now_tm.tm_hour == 13 && now_tm.tm_min < 1)) {
                /* seconds left until 13:01:00 today ---------------------- */
                int sec_left = (12 - now_tm.tm_hour) * 3600 +   // full hours to go
                               (60 - now_tm.tm_min - 1) * 60 +  // full minutes to go
                               (60 - now_tm.tm_sec);            // seconds to go

                int ms_left = sec_left * 1000;

                /* if the normal period would overshoot 13:01, shorten it  */
                if (ms_left > 0 && ms_left < delay_ms) delay_ms = ms_left;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms)); /* <-- replaces the old line */
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

    // reset Relays
    gpio_reset_pin(RELAY1);
    gpio_set_direction(RELAY1, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY1, 0);
    gpio_reset_pin(RELAY2);
    gpio_set_direction(RELAY2, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY2, 0);
    gpio_reset_pin(RELAY3);
    gpio_set_direction(RELAY3, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY3, 0);

    // reset Wifi init button
    gpio_reset_pin(WIFI_INIT_BUTTON_GPIO);
    gpio_set_direction(WIFI_INIT_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(WIFI_INIT_BUTTON_GPIO, GPIO_PULLUP_ONLY);

    // initialize run variables
    init_variables();

    // Register WiFi led tasks
    xTaskCreatePinnedToCore(wifi_led_1, "wifi_led_1", configMINIMAL_STACK_SIZE * 5, NULL, configMAX_PRIORITIES - 4,
                            NULL, 1);
    xTaskCreatePinnedToCore(wifi_led_2, "wifi_led_2", configMINIMAL_STACK_SIZE * 5, NULL, configMAX_PRIORITIES - 4,
                            NULL, 1);

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
    ESP_ERROR_CHECK(uart_driver_install(RS485_UART_PORT, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0));
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(RS485_UART_PORT, &uart_config));
    // Set UART pins
    ESP_ERROR_CHECK(uart_set_pin(RS485_UART_PORT, RS485_TXD, RS485_RXD, RS485_RTS, RS485_CTS));
    // Set RS485 half duplex mode
    ESP_ERROR_CHECK(uart_set_mode(RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));
    // uart_set_mode(RS485_UART_PORT, UART_MODE_UART)

    // Set read timeout of UART TOUT feature
    ESP_ERROR_CHECK(uart_set_rx_timeout(RS485_UART_PORT, RS485_READ_TOUT));

    xTaskCreatePinnedToCore(uart_event_task, "uart_event_task", 8192, NULL, configMAX_PRIORITIES - 2, NULL, 1);

    // inicializuj NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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
    xTaskCreatePinnedToCore(obtain_time, "obtain_time", configMINIMAL_STACK_SIZE * 5, NULL, configMAX_PRIORITIES - 4,
                            NULL, 1);

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    /* Register our event handler for Wi-Fi, IP and Provisioning related
     * events
     */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = {.scheme = wifi_prov_scheme_ble,
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
        ESP_LOGI(TAG, "Wait for Reset button release: %d", gpio_get_level(WIFI_INIT_BUTTON_GPIO));
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
    ESP_LOGW(TAG, "button_reprovisioning: %d", button_reprovisioning);
    if (button_reprovisioning) {
        ESP_LOGW(TAG, "Erasing standard passwords for automato and servis");
        nvs_erase_key(nvs_handle_storage, "pwd_automato");
        nvs_erase_key(nvs_handle_storage, "pwd_servis");

        /*
        ESP_LOGW(TAG, "Factory reset: wiping entire NVS partition");

        nvs_flash_deinit();                  // close all open handles
        ESP_ERROR_CHECK(nvs_flash_erase());  // <-- one call erases everything
        ESP_ERROR_CHECK(nvs_flash_init());   // re-initialise for fresh use
        */
    }

    // načti aplikační hesla z nvs
    char read_nvs_value[32];
    size_t required_size = sizeof(read_nvs_value);
    //
    int err = nvs_get_str(nvs_handle_storage, "pwd_automato", read_nvs_value, &required_size);
    if (err == ESP_OK) {
        strcpy(users[0].password, read_nvs_value);
        ESP_LOGW(TAG, "Password for automato user read from nvs");
        // ESP_LOGI(TAG, "Password for automato user read from nvs, %s",
        // users[0].password);
    } else {
        ESP_LOGW(TAG, "nvs_get_str  for password automato returned %d, size %u", err, (unsigned)required_size);
        ESP_LOGW(TAG, "Standard password for the user automato loaded");
    }
    //
    err = nvs_get_str(nvs_handle_storage, "pwd_servis", read_nvs_value, &required_size);
    if (err == ESP_OK) {
        strcpy(users[1].password, read_nvs_value);
        ESP_LOGW(TAG, "Password for the servis user read from nvs");
        // ESP_LOGI(TAG, "Heslo pro servis nacteno: %s", users[1].password);
    } else {
        ESP_LOGW(TAG, "nvs_get_str for password servis returned %d, size %u", err, (unsigned)required_size);
        ESP_LOGW(TAG, "Standard password for the user servis loaded");
    }

    // výrobní číslo: pokud existuje, načti z nvs, jinak standardní
    err = nvs_get_str(nvs_handle_storage, "production_num", read_nvs_value, &required_size);
    if (err == ESP_OK) {
        strcpy(vyrobnicislo, read_nvs_value);
        ESP_LOGI(TAG, "Production number %s read from nvs", vyrobnicislo);
        // ESP_LOGI(TAG, "Heslo pro servis nacteno: %s", users[1].password);
    } else {
        ESP_LOGI(TAG, "Production number not read, standard %s", vyrobnicislo);
        // zapis standardního výrobního čísla do nvs
        err = nvs_set_str(nvs_handle_storage, "production_num", vyrobnicislo);
        if (err == ESP_OK) {
            nvs_commit(nvs_handle_storage);
        } else {
            ESP_LOGE(TAG, "Error writing the standard production number to nvs");
        }
    }
    /* If device is not yet provisioned or reprovisioning button pressed
     * long, start provisioning service */
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

        /* This step is only useful when scheme is wifi_prov_scheme_ble.
         * This will set a custom 128 bit UUID which will be included in the
         * BLE advertisement and will correspond to the primary GATT service
         * that provides provisioning endpoints as GATT characteristics.
         * Each GATT characteristic will be formed using the primary service
         * UUID as base, with different auto assigned 12th and 13th bytes
         * (assume counting starts from 0th byte). The client side
         * applications must identify the endpoints by reading the User
         * Characteristic Description descriptor (0x2901) for each
         * characteristic, which contains the endpoint name of the
         * characteristic */
        uint8_t custom_service_uuid[] = {
            /* LSB <---------------------------------------
             * ---------------------------------------> MSB */
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf, 0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);

        /* Start provisioning service */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void*)sec_params, service_name, service_key));
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
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true, portMAX_DELAY);

    led2_status = LED2_STATUS_WIFI_CONNECTED;

    // after S150u_wifi reset: request refresh of root web page
    /*
    ESP_LOGI(TAG,
             "Setting rfp to 1: // after S150u_wifi reset: request "
             "refresh of root web page ");
    */
    rfp = 1;

    // read or refreshs conditions from NVS
    ESP_ERROR_CHECK(load_or_init_conditions());

    // read or refreshs action descriptions  from NVS
    ESP_ERROR_CHECK(load_or_init_actions());

    // load lowline2 from NVS (or write default on first boot)
    size_t szl = sizeof(lowline2);
    esp_err_t e = nvs_get_str(nvs_handle_storage, "lowline2", lowline2, &szl);
    if (e == ESP_ERR_NVS_NOT_FOUND) {
        // first boot: write the compile‐time default into NVS
        strncpy(lowline2, "Kontakt: Ota Pekař, pekar@nescom.com, +420602328542", sizeof(lowline2)-1);
        nvs_set_str(nvs_handle_storage, "lowline2", lowline2);
        nvs_commit(nvs_handle_storage);
    }


    // start webserver
    start_webserver();

    // initial RX flush and clean the queue
    //  rx queue
    rx_queue_item qi;

    uart_flush(RS485_UART_PORT);
    while (xQueueReceive(rx_queue, &qi, 0) == pdPASS) {
    }

    // initialize the send task
    xTaskCreatePinnedToCore(uart_tx_task, "uart_tx_task", configMINIMAL_STACK_SIZE * 5, NULL, configMAX_PRIORITIES - 2,
                            NULL, 1);

    // initialize the send receive cycle
    xTaskCreatePinnedToCore(receive_task, "send_receive_task", configMINIMAL_STACK_SIZE * 5, NULL,
                            configMAX_PRIORITIES - 3, NULL, 1);

    // everything processed, turn status led green
    ESP_LOGI(TAG, "Inicialization finished, starting the main app cycle");

    // initi some statuses
    wifiapst = WIFIAPST_NORMAL;

    // get MAC address
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    snprintf(mac_string, sizeof(mac_string), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5]);

    // Initialize grab OTE data task
    // default httpd server priority should be 5, now giving lower: 3
    // ESP_LOGW(TAG, "free heap before OTE : %ld",
    // esp_get_free_heap_size());

    xTaskCreatePinnedToCore(ote_read, "ote_read", configMINIMAL_STACK_SIZE * 5, NULL, configMAX_PRIORITIES - 4, NULL,
                            1);

    // initiate evaluate_actions task
    xTaskCreatePinnedToCore(evaluate_do, "evaluate_do", 16384, NULL, configMAX_PRIORITIES - 5, NULL, 1);

    // ESP_LOGW(TAG, "free heap after OTE  : %ld",
    // esp_get_free_heap_size());

    // initialize mDNS
    start_mdns_service();


    // set default language

    // read language from nvs, if not found, set default
    // and write to nvs
    ESP_LOGI(TAG, "Loading language from NVS");
    size_t sz = sizeof(gst_lang);
    err = nvs_get_blob(nvs_handle_storage, "gst_lang", &gst_lang, &sz);
    if (err != ESP_OK || sz != sizeof(gst_lang) || gst_lang >= LANG_COUNT) {
        // first-boot or corrupted → fall back to default
        gst_lang = LANG_CZ;
        ESP_LOGI(TAG, "gst_lang not found or invalid, defaulting to %d", gst_lang);
        nvs_set_blob(nvs_handle_storage, "gst_lang", &gst_lang, sizeof(gst_lang));
        nvs_commit(nvs_handle_storage);
    } else {
        ESP_LOGI(TAG, "Loaded gst_lang=%d from NVS", gst_lang);
    }

    // dummy ticks cycle
    while (1) {
        // diagnostics 1: list all items in nvs "storage"

        /*

         nvs_handle_t phandle;
         esp_err_t err = nvs_open("storage", NVS_READONLY, &phandle);
         nvs_iterator_t it = NULL;
         esp_err_t res = nvs_entry_find("nvs", "storage", NVS_TYPE_ANY,
         &it); while (res == ESP_OK) { nvs_entry_info_t info;
         nvs_entry_info(it, &info); // Can omit error check if parameters
         are guaranteed to be non-NULL ESP_LOGI(TAG, "***NVS: key '%s', type
         '%d'", info.key, info.type); if (info.type == 33) { char gstr[50];
         size_t required_size = 50; ESP_ERROR_CHECK( nvs_get_str(phandle,
         info.key, gstr, &required_size)); ESP_LOGI(TAG, "***NVS string:
         '%s'", gstr);
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

        // !!! patch here: "comm_status is OK"
        comm_status = COMST_OK;
        // !!!

        if (comm_status == COMST_OK && provisioned && device_connected &&
            (nntptime_status == 1 || nntptime_status == 2)) {
            led1_status = LED1_STATUS_OK;
        } else {
            led1_status = LED1_STATUS_ERROR;
        }

        // keepalive status message
        ESP_LOGI(TAG,
                 "ip: %s, MAC: %s, "
                 "prod.num.: %s, errors: comm=%d "
                 "wifi_prov=%d "
                 "wifi_conn=%d",
                 ipaddress, mac_string, vyrobnicislo, comm_status, !provisioned, !device_connected);

        // development - helper logging

        ESP_LOGI(TAG, "===================== apst:%d  wifiapst:%d", apst, wifiapst);

        // next tick pause+
        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }
}
