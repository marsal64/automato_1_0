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

// global static "require setup" variable
static bool require_setup = false;
static bool last_setup = false;

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

// List of accepted messages lengths
static uint8_t mes_lengths[NUM_CHECKS_MESS_LENGTHS] = {
    MESS_LENGTH_GENERAL, MESS_LENGTH_WIFI, 7, 6, 8, 10, 21};

// message types
// plain wifi poll
char* MES_PLAINWIFIPOLL = "\0\1\0";
char* MES_PLAINWIFIPOLL_RESPONSE = "\1\0\3\0";

// Wifi reconnect constants
static int wifi_retry_count = 0;

//// S150U variables start ////////////////////

// setup variables
int8_t setup_korekceCidla;  // přichází ve formátu +127
uint8_t setup_nastaveniKamen;
uint8_t setup_typSauny;
uint8_t setup_stridaniFazi;
uint8_t setup_blokovaniSvetla;
uint8_t setup_nastavenaTemp;
uint8_t setup_dalkoveOvladani;
uint8_t setup_parniEsence;
uint8_t setup_intervalEsence;
uint8_t setup_typSpusteni;
uint32_t setup_celkovaDobaProvozu;
uint8_t setup_provozniDoba;

// standard variables
static uint8_t systemONOFF;    // 0 2 (3) 1
static uint8_t hotRUN;         // 0 sauna ceka na spusteni 1 sauna spustena
static uint8_t stavRelat;      // stav vsech relatek na hlavni desce
static uint8_t typSauny;       // 0 1 2 (s RH modulem 3 4 5 6)
static uint8_t typSpusteni;    // 0 standard, 1 denni, 2 tydenni
static uint8_t battZmerena;    // 1 - byla zmerena baterie RTC
static uint16_t battVolt;      // jeji napeti v milivoltech uint16_t
static uint8_t nastavenaTemp;  // nastavena teplota ve stupnich uint8_t
static uint32_t dobaBehu;      // doba behu sauny uint32_t v sekundach
static int16_t aktTemp;        // aktualni teplota int16 (muze byt i zaporna)
static uint8_t termState;      // stav termostatu 0 ok, 1 zkrat, 2 odpojeno
static uint8_t pojTepState;    // 1 - chyba tepelne pojistky
static uint8_t doorOpen;       // 1 - otevrene dvere
static uint16_t doorTout;      // cas otevrenych dveri v sekundach
static uint16_t
    onTout;  // timeout pro vypnuti pokud neni potvrzeno spusteni sauny
static uint8_t odlozenyNastaveno;  // 0 - neni nastaveno, 1 je nastaveno
static uint8_t runFromDO = 0;      // Spusteno z DO 0 - ne 1 - ano
static uint32_t
    p_odlozenyCas;  // hodnota odlozeneho startu v sekundach uint32_t
static uint32_t p_DO_dobaBehuSauny;  // doba behu sauny pri spusteni z dalkoveho
                                     // ovladani uint32_t

static uint8_t defDefTemp = 80;   // Vychozi teplota
static uint8_t defMinTemp = 50;   // Minimalni teplota
static uint8_t defMaxTemp = 120;  // Maximalni teplota
static uint8_t defMaxTempE;       // Max teplota - vypina pojistka

static uint8_t devActive;   // stav zarizeni bit0=1, bit1=stav extovl, bit2=stav
                            // wifi, bit3=stav RH
static uint8_t devRHvalue;  // hodnota z RH cidla
static uint8_t devRHrelay;  // hodnota pro rele v RH modulu
static uint8_t parniEsence;     // 0 vypnuto 1-10sec 2 3 4 5 6-trvale zapnuto
static uint8_t esenceState;     // Aktualni stav esence
static uint8_t intervalEsence;  // Interval esence v minutach
static uint8_t state1;          // Status změny teplota provoz 20241220
static uint8_t state2;          // Status setup 20241220
//
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

// typ sauny
enum TYP_SAUNY {
    TYP_SAUNY_STD,
    TYP_SAUNY_INFRA,
    TYP_SAUNY_PARNI,
    TYP_SAUNY_RHSUCHA,
    TYP_SAUNY_RHVLHKA,
    TYP_SAUNY_RHMOKRA,
    TYP_SAUNY_RHPARA,
    TYP_SAUNY_COUNT
};

static char typy_saun[TYP_SAUNY_COUNT][20] = {"std", "inf", "par", "RHs",
                                              "RHv", "RHm", "RHp"};

static char typy_saun_long[TYP_SAUNY_COUNT][30] = {
    "Standardní", "Infrasauna", "Parní",    "RH - suchá",
    "RH - vlhká", "RH - mokrá", "RH - pára"};

static char typy_saun_minitext[TYP_SAUNY_COUNT][30] = {
    "Standardní", "Infrasauna", "Parní", "Suchá", "Vlhká", "Mokrá", "Pára"};


static char typy_saun_long_graph[TYP_SAUNY_COUNT][50] = {
    "Standardní", "Infrasauna", "Parní",       "RH: 💧",
    "RH: 💧💧",   "RH: 💧💧💧", "RH: 💧💧💧💧"};


// proměnná pro nastavování RH při startu
uint8_t p_rh_set = TYP_SAUNY_RHSUCHA;

// indikátor označující, že nastavení RH už proběhlo
uint8_t p_rh_set_done = 0;

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

// způsob nastavení kamen (1 nebo 3 fáze)
enum TYP_NAST_KAMEN {
    KAMNANAST_JEDNOFAZOVA,
    KAMNANAST_TROJFAZOVA,
    KAMNANAST_NUMSTATES
};
static char typy_nastaveni_kamen[KAMNANAST_NUMSTATES][50] = {"1 Fáze",
                                                             "3 Fáze"};

// parní esence
enum PARNIESENCE {
    ESENCE_VYPNUTO,
    ESENCE_10,
    ESENCE_20,
    ESENCE_30,
    ESENCE_40,
    ESENCE_50,
    ESENCE_ZAPNUTO,
    ESENCE_NUMSTATES
};
static char typy_parniesence[ESENCE_NUMSTATES][50] = {
    "Vypnuto",   "10 sekund", "20 sekund",     "30 sekund",
    "40 sekund", "50 sekund", "Trvale zapnuto"};


// dálkové ovládání
enum DALKOVEOVLADANI {
    DALKOVEOVLADANI_NE,
    DALKOVEOVLADANI_ANO,
    DALKOVEOVLADANI_NUMSTATES
};
char typy_dalkoveovladani[DALKOVEOVLADANI_NUMSTATES][10] = {"Ne", "Ano"};

// střídání fází
enum STRIDANIFAZI { STRIDANIFAZI_NE, STRIDANIFAZI_ANO, STRIDANIFAZI_NUMSTATES };
char typy_stridanifazi[STRIDANIFAZI_NUMSTATES][10] = {"Ne", "Ano"};


// blokování světla
enum BLOKOVANISVETLA {
    BLOKOVANISVETLA_NE,
    BLOKOVANISVETLA_ANO,
    BLOKOVANISVETLA_NUMSTATES
};
char typy_blokovanisvetla[BLOKOVANISVETLA_NUMSTATES][10] = {"Ne", "Ano"};

// režim sauny
enum TYPSPUSTENI_SAUNY {
    TYPSPUSTENI_SAUNY_NORMALNI,
    TYPSPUSTENI_SAUNY_DENNI,
    TYPSPUSTENI_SAUNY_TYDENNI,
    TYPSPUSTENI_SAUNY_NUMSTATES
};
static char typy_spusteni_sauny[TYPSPUSTENI_SAUNY_NUMSTATES][100] = {
    "Normální", "Denní (odložený start)", "Týdenní"};



// Buffer proměnné pro nastavení času
uint16_t year_s = 0;
uint8_t month_s = 0;
uint8_t date_s = 0;
uint8_t wDay_s = 0;
uint8_t hour_s = 0;
uint8_t min_s = 0;
uint8_t sec_s = 0;


//////// setup helper variables ////////////////
static uint8_t nastavenaTemp_set;  // nastavena teplota ve stupnich uint8_t
//////// setup helper variables ////////////////
static uint32_t p_odlozenyCas_set;  // nastavena teplota ve stupnich uint8_t

// static helper
static uint32_t dobaBehu_set;

// nvs flash handler
static nvs_handle_t nvs_handle_storage;

// "set" proměnné
// nastavují se v případě požadavku na přenos nějakého údaje do základní
// jednotky
uint8_t mainset_on = 0;
uint8_t mainset_off = 0;
uint8_t mainset_lighton = 0;
uint8_t mainset_lightoff = 0;
uint8_t mainset_hoton = 0;
uint8_t mainset_sendsetup = 0;
uint8_t mainset_temp = 0;
uint32_t mainset_dobabehu = 0;
uint32_t mainset_odlocas = 0;


// reportovaci bajty - probíhá nastavení
uint8_t mainset_report_odlocas = 0;   // odložený čas
uint8_t mainset_report_dobabehu = 0;  // doba běhu
uint8_t mainset_report_setup = 0;     // setup
uint8_t mainset_report_temp = 0;      // teplota
uint8_t mainset_report_rh = 0;        // nastavování RH

// nastavení synchronizace času
uint8_t mainset_synchrocas = 0;

// global variables used for parsing when "Confirm" pressed
uint8_t hours_ret = 0;
uint8_t minutes_ret = 0;
uint8_t date_ret = 0;
uint8_t month_ret = 0;
uint16_t year_ret = 0;
uint8_t datacas_typ_ret = 0;
uint8_t nastavenikamen_ret = 0;
uint8_t nastavenaTemp_ret;
uint32_t dobaBehu_ret;
uint8_t typSauny_ret;
uint8_t korekce_cidla_ret;
uint8_t parniEsence_ret = 0;      // 0 vypnuto 1-10sec 2 3 4 5 6-trvale zapnuto
uint8_t dalkoveOvladani_ret = 0;  // 0 - ne, 1 - ano
uint8_t intervalEsence_ret;       // Interval esence v minutach
uint8_t blokovaniSvetla_ret = 0;  // Blokování světla
uint8_t typspusteni_sauny_ret;    // 0 normální, 1 denní, 2 týdenní
uint8_t stridanifazi_ret;         // 0 normální, 1 denní, 2 týdenní
char password_sauna_ret[33] = "";
char password_servis_ret[33] = "";

// buffer variables for setup data sending
uint8_t korekce_cidla_buf = 0;
uint8_t nastavenikamen_buf = 0;
uint8_t typSauny_buf = 0;
uint8_t stridanifazi_buf = 0;
uint8_t blokovaniSvetla_buf = 0;
uint8_t dalkoveOvladani_buf = 0;
uint8_t parniEsence_buf = 0;
uint8_t intervalEsence_buf = 0;
uint8_t typspusteni_sauny_buf = 0;

// helper variables for time transfer
uint8_t hours_preps = 0;
uint8_t minutes_preps = 0;
uint8_t date_preps = 0;
uint8_t month_preps = 0;
uint16_t year_preps = 0;


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

/**
 * @brief parse "general poll"
 */
int recparse_general(uint8_t* dt, int lendt) {
    int retstatus = 0;
    // ESP_LOGI(TAG, "recparse_general - Receiving/parsing general response");

    // copy just for convenience (description match)
    memcpy(dt, &dt[5], lendt - 5);
    // hexlogger(dt, lendt);
    // parse
    systemONOFF = dt[0];
    hotRUN = dt[1];
    stavRelat = dt[2];
    typSauny = dt[3];
    typSpusteni = dt[4];
    battZmerena = dt[5];
    battVolt = *(uint16_t*)&dt[6];
    nastavenaTemp = dt[8];
    dobaBehu = *(uint32_t*)&dt[9];
    aktTemp = *(uint16_t*)&dt[13];
    termState = dt[15];
    pojTepState = dt[16];
    doorOpen = dt[17];
    doorTout = *(uint16_t*)&dt[18];
    onTout = *(uint16_t*)&dt[20];
    odlozenyNastaveno = dt[22];
    runFromDO = dt[23];
    p_odlozenyCas = *(uint32_t*)&dt[24];
    p_DO_dobaBehuSauny = *(uint32_t*)&dt[28];
    defDefTemp = dt[32];
    defMinTemp = dt[33];
    defMaxTemp = dt[34];
    defMaxTempE = dt[35];
    devActive = dt[36];
    devRHvalue = dt[37];
    devRHrelay = dt[38];
    parniEsence = dt[39];
    esenceState = dt[40];
    intervalEsence = dt[41];
    btime.sec = dt[42];
    btime.min = dt[43];
    btime.hour = dt[44];
    // calculate day of week (1 - Sunday)
    btime.wDay = day_of_week(btime.year + 2000, btime.month, btime.date);
    btime.date = dt[46];
    btime.month = dt[47];
    btime.year = dt[48];
    state1 = dt[49];
    state2 = dt[50];


    // cas ze sauny
    snprintf(saunacas_string, sizeof(saunacas_string),
             "%d.%d.%d %02d:%02d:%02d (%s)", btime.date, btime.month,
             btime.year + 2000, btime.hour, btime.min, btime.sec,
             translatedays[btime.wDay]);
    // ESP_LOGI(TAG, "'%s'", saunacas_string);

    // set apst
    if (!systemONOFF) {
        apst = APST_OFF;
    } else if (systemONOFF == 2) {
        apst = APST_STARTING;
        // !!!o nenastavuje se odložený čas
    } else if (systemONOFF == 1 && !hotRUN && state2 == 0) {
        apst = APST_TO_CONFIRM;
        // !!!o nastavuje se odložený čas
    } else if (systemONOFF == 1 && !hotRUN && (state2 & 0x80)) {
        apst = APST_ODLO;
    } else if (systemONOFF == 1 && hotRUN) {
        apst = APST_NORMAL;
    } else {
        apst = APST_UNKNOWN;
    }

    // refresh _report statusůd
    mainset_report_dobabehu = (wifiapst == WIFIAPST_SET_TIMEOUT);
    mainset_report_odlocas = (wifiapst == WIFIAPST_SET_ODLO);
    mainset_report_rh = (wifiapst == WIFIAPST_SET_RH);
    mainset_report_setup = (wifiapst == WIFIAPST_SET_GENERAL);
    mainset_report_temp = (wifiapst == WIFIAPST_SET_TERM);

    // Probíhá setup na hlavní jednotce?
    if (dt[49] & 0x08) {  // bit3
        last_setup = true;
    } else {
        // no setup info - was previously?
        if (last_setup) {
            // was previously - require sending setup information
            require_setup = true;
            last_setup = false;
        }
    }

    return retstatus;
}

/**
 * @brief react to "wifi unit poll""
 */
int react_to_wifi_rx_message(char* message, int len) {
    // Expect OK
    int retstatus = 0;
    // newly constructed output message:
    tx_queue_item mo;

    if (message[3] == 0) {  // jde o "standardní" info
        // plain WiFi unit poll, start to construct the response
        // setup to send needed?
        if (require_setup) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 0x20, 0, 0}, 6);
            mo.length = 6;  // length without crc
        } else if (mainset_report_rh) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 4, 1, 0x08}, 6);
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_report_dobabehu) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 4, 1, 0x40}, 6);
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_report_temp) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 4, 1, 0x80}, 6);
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_report_setup) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 4, 1, 0x20}, 6);
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_report_odlocas) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 4, 1, 0x10}, 6);
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_odlocas) {
            // second parameter - data length + 1
            memcpy(mo.message, (uint8_t[]){1, 5, 3, 0x14, 1}, 5);
            memcpy(&mo.message[5], &mainset_odlocas, 4);
            mainset_odlocas = 0;  // nebude se potvrzovat, vynuluj (předpoklad
                                  // úspěšného odeslání)
            mo.length = 6 + 4 - 1;
        } else if (mainset_synchrocas) {
            // second parameter - data length + 1
            memcpy(mo.message, (uint8_t[]){1, 8, 3, 0x13, 1}, 5);
            mo.message[5] = year_preps - 2000;
            mo.message[6] = month_preps;
            mo.message[7] = date_preps;
            mo.message[8] = day_of_week(year_preps, month_preps, date_preps);
            mo.message[9] = hours_preps;
            mo.message[10] = minutes_preps;
            mo.message[11] = 0;      // seconds to zero
            mainset_synchrocas = 0;  // nebude se potvrzovat, vynuluj
                                     // (předpoklad úspěšného odeslání)
            mo.length = 5 + 7;

        } else if (mainset_hoton) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 2, 1, 1}, 6);
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_lighton) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 3, 1, 1}, 6);
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_lightoff) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 3, 1, 0}, 6);
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_on) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 1, 1, 2}, 6);
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_off) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 1, 1, 0}, 6);
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_dobabehu) {
            memcpy(mo.message, (uint8_t[]){1, 5, 3, 0x12, 1}, 5);
            memcpy(&mo.message[5], &mainset_dobabehu, 4);
            mo.length = 6 + 4 - 1;  // length: without crc, with data
        } else if (mainset_temp) {
            memcpy(mo.message, (uint8_t[]){1, 2, 3, 0x11, 1}, 5);
            mo.message[5] = mainset_temp;
            mo.length = 6;  // length: without crc, with data
        } else if (mainset_sendsetup) {
            // !!! construct "setup" message to be sent
            // second parameter - data length + 1
            memcpy(mo.message, (uint8_t[]){1, 16, 3, 0x20, 1}, 5);
            mo.message[5] = korekce_cidla_buf + 127;
            mo.message[6] = nastavenikamen_buf;
            mo.message[7] = typSauny_buf;
            mo.message[8] = stridanifazi_buf;
            mo.message[9] = blokovaniSvetla_buf;
            mo.message[10] = nastavenaTemp;
            mo.message[11] = dalkoveOvladani_buf;
            mo.message[12] = parniEsence_buf;
            mo.message[13] = intervalEsence_buf;
            mo.message[14] = typspusteni_sauny_buf;
            memcpy(&mo.message[16], &setup_celkovaDobaProvozu, 4);
            mo.message[20] = setup_provozniDoba;  // dle typu sauny
            mo.length = 5 + 15;                   // length without CRC
            // no subsequent confirmation, expect to be sent correctly

            mainset_sendsetup = 0;

        } else {
            // plain response only, no changes requested
            memcpy(mo.message, MES_PLAINWIFIPOLL_RESPONSE,
                   MES_PLAINWIFIPOLL_RESPONSE_LEN);
            mo.length = MES_PLAINWIFIPOLL_RESPONSE_LEN;
        }
    } else {
        // setup?
        // sekce "očekáváme setup informaci"
        if (require_setup && message[3] == 0x20) {
            // přišla setup informace
            // ESP_LOGI(TAG, "Prijem setup informace");
            // 0x03 0x10 0x01 0x20 0x00 0x7F 0x01 0x00 0x01 0x00 0x50 0x00 0x00
            // 0x05 0x00 0x95 0x55 0x00 0x00 0x04 0x08 přepiš
            //
            // setup_variables held here for consistency, duplicate variables
            // then used:
            setup_korekceCidla = message[5] - 127;
            setup_nastaveniKamen = message[6];
            setup_typSauny = message[7];
            setup_stridaniFazi = message[8];
            setup_blokovaniSvetla = message[9];
            setup_nastavenaTemp = message[10];
            setup_dalkoveOvladani = message[11];
            setup_parniEsence = message[12];
            setup_intervalEsence = message[13];
            setup_typSpusteni = message[14];
            memcpy(&setup_celkovaDobaProvozu, &message[15], sizeof(uint32_t));
            setup_provozniDoba = message[19];

            // parsed, now may cleanse "require setup" flag
            require_setup = 0;
        }
        // něco jiného než 0, zde předpokládáme potvrzení příkazu

        // sekce "správné a očekávané potvrzení"
        else if (mainset_hoton && (message[3] == 2)) {
            mainset_hoton = 0;
            ESP_LOGI(TAG, "Potvrzeni hoton z hlavni jednotky OK");
        } else if (mainset_lighton && (message[3] == 3)) {
            ESP_LOGI(TAG, "Potvrzeni lighton z hlavni jednotky OK");
            mainset_lighton = 0;
        } else if (mainset_lightoff && (message[3] == 3)) {
            ESP_LOGI(TAG, "Potvrzeni lightoff z hlavni jednotky OK");
            mainset_lightoff = 0;
        } else if (mainset_on && (message[3] == 1)) {
            ESP_LOGI(TAG, "Potvrzeni on z hlavni jednotky OK");
            mainset_on = 0;
        } else if (mainset_off && (message[3] == 1)) {
            ESP_LOGI(TAG, "Potvrzeni off z hlavni jednotky OK");
            mainset_off = 0;
        } else if ((mainset_dobabehu > 0) && (message[3] == 0x12)) {
            ESP_LOGI(TAG, "Potvrzeni dobabehu z hlavni jednotky OK");
            mainset_dobabehu = 0;
        } else if (mainset_temp && (message[3] == 0x11)) {
            ESP_LOGI(TAG, "Potvrzeni teploty z hlavni jednotky OK");
            mainset_temp = 0;
        } else {
            // Nekonzistentní potvrzení
            ESP_LOGW(TAG, "Nekonzistentni potvzrzeni z hlavni jednotky: %d",
                     message[3]);
            hexlogger((uint8_t*)message, len);
        }

        // zasílá "potvrzení potvrzení"
        memcpy(mo.message, (uint8_t[]){1, 0, 3, 0}, 4);
        mo.length = 4;
    }

    // !!! zruší vše, co bylo případně nastaveno ke změně
    /*
    mainset_hoton = 0;
    mainset_lighton = 0;
    mainset_lightoff = 0;
    mainset_on = 0;
    mainset_off = 0;
    mainset_dobabehu = 0;
    mainset_temp = 0;
  */

    // send the message
    uint8_t crc = crc_calc((uint8_t*)mo.message, mo.length);
    mo.message[mo.length] = crc;
    mo.length = mo.length + 1;  // add crc

    // send message
    if (xQueueSend(tx_queue, &mo, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to send data to the rx_internal queue");
        retstatus = -1;
    } else {
        // message sent log (anything but standard response)
        if (mo.message[1]) {
            ESP_LOGI(TAG, "Message sent to tx, len %d:", mo.length);
            hexlogger((uint8_t*)mo.message, mo.length);
        }
    }
    return retstatus;
}

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

        // Verify if message length in allowed list
        uint8_t meslenghtOK = 0;  // assume length not correct
        for (int i = 0; i < NUM_CHECKS_MESS_LENGTHS; i++)
            if (mes_lengths[i] == qi.length) {
                meslenghtOK = 1;
                break;
            }
        // If not correct, try to recover and wait
        // try to recover and continue
        if (!meslenghtOK) {
            ESP_LOGI(TAG, "Zprava s delkou %d ignorovana", qi.length);
            hexlogger((uint8_t*)qi.message, qi.length);

            uart_flush(RS485_UART_PORT);
            while (xQueueReceive(rx_queue, &qi, 0) == pdPASS) {
            }
            continue;
        }

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
        // ESP_LOG_BUFFER_HEXDUMP(TAG, qi.message, qi.length, ESP_LOG_INFO);

        // What message?
        int retm;
        switch (qi.message[0]) {
                // WiFi poll packet
            case RX_ADDRESS_WIFI:
                // poll for S150U-WiFi or confirmation of set
                retm = react_to_wifi_rx_message(qi.message, qi.length);
                if (retm) {
                    ESP_LOGE(TAG, "Chyba pri zpracovani zpravy pro WiFi");
                    // snaha o restart
                    uart_flush(RS485_UART_PORT);
                    while (xQueueReceive(rx_queue, &qi, 0) == pdPASS) {
                    }
                    continue;
                }
                // uspesne zpracovano
                break;
                // General packet
            case RX_ADDRESS_GENERAL_PACKET:
                // only if correct message length
                if (qi.length != MESS_LENGTH_GENERAL) {
                    ESP_LOGI(TAG,
                             "Obecny paket s nespravnou delkou %d, ignorovano",
                             qi.length);

                    // short diagnostics pulse at the status pin
                    // gpio_set_level(TEST_UART_GPIO, 1);
                    // ESP_LOGI(TAG,"///// UART Diag Pulse on");
                    // gpio_set_level(TEST_UART_GPIO, 0);
                    // ESP_LOGI(TAG,"///// UART Diag Pulse off");

                    // ESP_LOG_BUFFER_HEXDUMP(TAG, qi.message,
                    // qi.length,ESP_LOG_INFO);
                    uart_flush(RS485_UART_PORT);
                    while (xQueueReceive(rx_queue, &qi, 0) == pdPASS) {
                    }
                    continue;
                }
                // parsuj
                retm = recparse_general((uint8_t*)qi.message, qi.length);
                if (retm) {
                    ESP_LOGE(TAG, "Chyba pri zpracovani vseobecne zpravy");
                    // snaha o restart
                    uart_flush(RS485_UART_PORT);
                    while (xQueueReceive(rx_queue, &qi, 0) == pdPASS) {
                    }
                    continue;
                } else {
                    // mark data processing
                    i_processed_data = 1;
                }
                break;
            default:
                // something else, ignore
                break;
        }
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

        // force status update periodically
        require_setup = true;

        // keepalive status message
        ESP_LOGI(TAG,
                 "S150U-WiFi typ: %s, typ spusteni: %s, ip: %s, MAC: %s, "
                 "vyr.c.: %s, chyby: comm=%d "
                 "wifi_prov=%d "
                 "wifi_conn=%d",
                 typy_saun[typSauny], typSpusteni == 1 ? "norm." : "odl.",
                 ipaddress, mac_string, vyrobnicislo, comm_status, !provisioned,
                 !device_connected);


        // development - helper logging

        /*
         (apst != APST_SET_GENERAL && apst != APST_SET_TERM
         && apst != APST_SET_TIMEOUT) {
         */
        ESP_LOGI(TAG, "===================== apst:%d  wifiapst:%d", apst,
                 wifiapst);

        ESP_LOGI(TAG, "systemONOFF %d", systemONOFF);
        ESP_LOGI(TAG, "hotRUN %d", hotRUN);
        /*
      ESP_LOGI(TAG, "stavRelat 0x%X", stavRelat);
      */
        /*
         ESP_LOGI(TAG, "typSauny %d", typSauny);
         */
        ESP_LOGI(TAG, "typSpusteni %d", typSpusteni);
        /*
        ESP_LOGI(TAG, "battZmerena %d", battZmerena);
        ESP_LOGI(TAG, "battVolt %d", battVolt);
        */
        /*
       ESP_LOGI(TAG, "nastavenaTemp %d", nastavenaTemp);
       ESP_LOGI(TAG, "dobaBehu %ld", dobaBehu);
      */
        /*
         ESP_LOGI(TAG, "aktTemp %d", aktTemp);

         ESP_LOGI(TAG, "termState %d", termState);
         ESP_LOGI(TAG, "pojTepState %d", pojTepState);
         ESP_LOGI(TAG, "doorOpen %d", doorOpen);
         ESP_LOGI(TAG, "doorTout %d", doorTout);
         ESP_LOGI(TAG, "onTout %d", onTout);
         */
        ESP_LOGI(TAG, "odlozenyNastaveno %d", odlozenyNastaveno);
        /*
         ESP_LOGI(TAG, "runFromDO %d", runFromDO);
         */
        // ESP_LOGI(TAG, "p_odlozenyCas %ld", p_odlozenyCas);
        // ESP_LOGI(TAG, "p_DO_dobaBehuSauny %lud", p_DO_dobaBehuSauny);
        /*
        ESP_LOGI(TAG, "defDefTemp %d", defDefTemp);

         ESP_LOGI(TAG, "defMinTemp %d", defMinTemp);
         ESP_LOGI(TAG, "defMaxTemp %d", defMaxTemp);
         ESP_LOGI(TAG, "defMaxTempE %d", defMaxTempE);
         ESP_LOGI(TAG, "devActive %d", devActive);
         ESP_LOGI(TAG, "devRHvalue %d", devRHvalue);
         ESP_LOGI(TAG, "devRHrelay %d", devRHrelay);
         ESP_LOGI(TAG, "parniEsence %d", parniEsence);
         ESP_LOGI(TAG, "esenceState %d", esenceState);
         ESP_LOGI(TAG, "intervalEsence %d", intervalEsence);
         */

        /*
       ESP_LOGI(TAG, "sec %d", btime.sec);
       ESP_LOGI(TAG, "min %d", btime.min);
       ESP_LOGI(TAG, "hour %d", btime.hour);
       ESP_LOGI(TAG, "wDAy %d", btime.wDay);
       ESP_LOGI(TAG, "date %d", btime.date);
       ESP_LOGI(TAG, "month %d", btime.month);
       ESP_LOGI(TAG, "year %d", btime.year);
    */
        ESP_LOGI(TAG, "state1 0x%02x", state1);
        ESP_LOGI(TAG, "state2 0x%02x", state2);

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
