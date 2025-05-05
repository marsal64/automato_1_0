/*
 * URL handlers
 */

/* names of all three pages – indexes: 0=provoz,1=dennitrh,2=nastaveni */
static const char *page_uri[3] = {"/dennitrh", "/pravidla", "/nastaveni"};
static const char *page_label[3] = {"Denní trh", "Pravidla", "Nastavení"};

/* helper: build a <nav> bar;   active_idx = 0,1,2 */
static void nav_bar(char *buf, size_t max, int active_idx) {
    size_t n = 0;
    n += snprintf(buf + n, max - n, "<nav>");
    for (int i = 0; i < 3 && n < max; ++i) {
        n += snprintf(buf + n, max - n, "<a class=\"tab%s\" href=\"%s\">%s</a>",
                      (i == active_idx) ? " active" : "", page_uri[i],
                      t(page_label[i]));
    }
    snprintf(buf + n, max - n, "</nav>");
}

/*----------------------------------------------------------------------------
 * Root page  ("Operation" – index 0)
 *--------------------------------------------------------------------------*/
/* ----------------  MAIN PAGE  ( “/” )  ------------------------------ */

static esp_err_t root_get_handler(httpd_req_t *req)
{
    /* ---------- 1. collect runtime data ---------------------------- */

    /* a) current “YYYYMMDDHH” and hour part */
    char now_key[13] = {0};
    if (nntptime_status == 1) {
        snprintf(now_key, sizeof(now_key), "%s", r_rrrrmmddhh);   /* already built elsewhere */
    }

    /* b) gather ALL price records from NVS (“o_YYYYMMDDHH”) ---------- */
    typedef struct { char key[13]; char val[16]; } price_t;
    price_t prices[400];                /* plenty – enlarge if needed */
    size_t  n_prices = 0;

    nvs_iterator_t it = NULL;
    if (nvs_entry_find(NVS_DEFAULT_PART_NAME, "storage", NVS_TYPE_STR, &it) == ESP_OK) {
        do {
            nvs_entry_info_t info;
            nvs_entry_info(it, &info);

            if (strncmp(info.key, "o_", 2) == 0 && strlen(info.key) == 13) {
                if (n_prices < sizeof(prices)/sizeof(prices[0])) {
                    strncpy(prices[n_prices].key, info.key + 2, 12); /* strip “o_” */
                    prices[n_prices].key[12] = '\0';

                    size_t sz = sizeof(prices[n_prices].val);
                    esp_err_t e = nvs_get_str(nvs_handle_storage, info.key,
                                              prices[n_prices].val, &sz);
                    if (e == ESP_OK) ++n_prices;
                }
            }
        } while (nvs_entry_next(&it) == ESP_OK);
        nvs_release_iterator(it);
    }
    /* sort newest → oldest (string order works for YYYYMMDDHH) */
    qsort(prices, n_prices, sizeof(price_t),
          [](const void *a, const void *b){
              return strcmp(((price_t*)b)->key, ((price_t*)a)->key);
          });

    /* c) build list of last actions (already alphabetically sorted) */
    /* ------------------------------------------------------------- */

    /* ---------- 2. compose HTML ---------------------------------- */

    httpd_resp_set_type(req, "text/html; charset=UTF-8");

    /* small helper to spit string literals */
    auto OUT = [&](const char *s){ httpd_resp_send_chunk(req, s, HTTPD_RESP_USE_STRLEN); };

    OUT("<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<style>"
        "body{font-family:Helvetica,Arial,sans-serif;max-width:180ch;margin:0 auto;padding:4px;}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));grid-gap:6px;}"
        ".head{grid-column:1/-1;display:flex;align-items:center;justify-content:space-between;}"
        ".prices, .actions{border:1px solid #ccc;border-radius:4px;padding:4px;}"
        ".prices b{color:#c00;}"
        ".logo{font-weight:bold;font-size:1.2rem;}"
        "</style></head><body>");

    /* ---------- header row --------------------------------------- */
    char dt_buf[64];
    if (nntptime_status) {
        strftime(dt_buf,sizeof(dt_buf),"%Y-%m-%d&nbsp;%H:%M:%S",&timeinfo_sntp);
        size_t dw = timeinfo_sntp.tm_wday;          /* 0..6 (Sun..) */
        if (dw>6) dw = 0;
        snprintf(dt_buf+strlen(dt_buf), sizeof(dt_buf)-strlen(dt_buf),
                 "&nbsp;(%s)", translatedays[dw]);
    } else strcpy(dt_buf, "time‑not‑set");

    OUT("<div class='grid'>"
        "<div class='head'>"
          "<span class='logo'>Automato</span>"
          "<span>");
    OUT(dt_buf);
    OUT("</span>"
          "<span>");
    OUT(ipaddress);
    OUT("</span>"
          "<form action='/logout' method='post' style='margin:0'>"
          "<button>Log&nbsp;off</button>"
          "</form>"
        "</div>");

    /* ---------- left column – prices ------------------------------ */
    OUT("<div class='prices'><h3>Prices</h3><ul style='margin:0;padding:0 0 0 1em;'>");
    for (size_t i=0;i<n_prices;++i){
        bool is_now = (strcmp(prices[i].key, now_key)==0);
        char line[64];
        snprintf(line,sizeof(line),"<li>%s&nbsp;:&nbsp;%s</li>",prices[i].key,prices[i].val);
        if (is_now){
            OUT("<b>");
            OUT(line);
            OUT("</b>");
        }else OUT(line);
    }
    OUT("</ul></div>");

    /* ---------- right column – actions ---------------------------- */
    OUT("<div class='actions'><h3>Last actions</h3><ul style='margin:0;padding:0 0 0 1em;'>");
    for (int i=0;i<NUMLASTACTIONSLOG;++i){
        if (last_actions_log[i].action[0]=='\0') break;
        char line[128];
        snprintf(line,sizeof(line),"<li>%s&nbsp;→&nbsp;%s</li>",
                 last_actions_log[i].action,last_actions_log[i].timestamp);
        OUT(line);
    }
    OUT("</ul></div></div>");     /* close grid + body */
    OUT("</body></html>");

    return httpd_resp_send_chunk(req, NULL, 0);     /* end of chunks */
}

/* -------------------------------------------------------------- */


/*----------------------------------------------------------------------------
 *  Other pages would call nav_bar(...,1) or nav_bar(...,2) to highlight the
 *  respective tab so that **all three** are always visible, with the current
 *  page emphasised.
 *---------------------------------------------------------------------------*/


esp_err_t login_get_handler(httpd_req_t *req) {
    char html[RESP_SIZE];
    size_t n = snprintf(
        html, sizeof(html),
        "<!DOCTYPE html>"
        "<html lang=\"cs\">"
        "<head>"
        "  <meta charset=\"UTF-8\">"
        "  <title>%s</title>"
        "  <style>"
        "    body  { font-family:Arial,sans-serif; display:flex; "
        "justify-content:center; align-items:center; height:100vh; margin:0; }"
        "    form  { border:1px solid #ddd; padding:20px; box-shadow:2px 2px "
        "5px rgba(0,0,0,0.2); }"
        "  </style>"
        "</head><body>"
        "<form action=\"/login\" method=\"post\">"
        "  <img src=\"/logo\" alt=\"%s\" style=\"width:15%%; height:auto;\">"
        "  <h2>%s</h2>"
        "  %s:<br><input type=\"text\" name=\"username\" "
        "maxlength=\"32\"><br><br>"
        "  %s:<br><input type=\"password\" name=\"password\" "
        "maxlength=\"32\"><br><br>"
        "  <input type=\"submit\" value=\"%s\">"
        "</form></body></html>",
        t("Automato"),                      /* <title>            */
        t("Automato"),                      /* logo alt           */
        t("Automato - přihlášení obsluhy"), /* heading            */
        t("Uživatelské jméno"),             /* label – username   */
        t("Heslo"),                         /* label – password   */
        t("Přihlášení")                     /* submit             */
    );

    httpd_resp_send(req, html, n);
    return ESP_OK;
}


// Login processing handler
esp_err_t login_post_handler(httpd_req_t *req) {
    char buf[256];  // Input buffer for cookie contents parsing
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        // Read the data for the request
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <=
            0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                // Handle timeout
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }

        // ESP_LOGI(TAG, "Processing login buffer (len: %d), %s", ret, buf);

        char inusername[33];
        char inpassword[33];

        // cut the buffer for the correct length
        buf[ret] = 0;

        remaining -= ret;  // Decrement the remaining length

        // expect two fixed users

        // Use sscanf to parse the string
        if (sscanf(buf, "username=%72[^&]&password=%72s", inusername,
                   inpassword) == 2) {
            ESP_LOGI(TAG, "Uzivatelske jmeno: %s", inusername);
            // ESP_LOGI(TAG, "Password: %s", inpassword);
        } else {
            ESP_LOGE(TAG, "Error parsing string from cookie\n");
        }

        // verify if username/password matches the given users
        // user id 0 ?
        if ((strcmp(users[0].username, inusername) == 0) &&
            (strcmp(users[0].password, inpassword) == 0)) {
            ESP_LOGI(TAG, "Uspesne overeni hesla pro uzivatele %s",
                     users[0].username);
            current_user_id = 0;
        }
        // user id 1 ?
        else if ((strcmp(users[1].username, inusername) == 0) &&
                 (strcmp(users[1].password, inpassword) == 0)) {
            ESP_LOGI(TAG, "Uspesne overeni hesla pro uzivatele %s",
                     users[1].username);
            current_user_id = 1;
        } else {
            // no verification, logoff
            ESP_LOGI(TAG, "Overeni hesla neuspesne");
            current_user_id = -1;
        }
    }

    // Prepare HTML content with meta refresh tag for redirection
    if (current_user_id != -1) {
        // Login success

        // Set cookie
        httpd_resp_set_hdr(req, "Set-Cookie", "auth=1; Path=/; HttpOnly");

        // Redirect to root page after successful login
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, NULL, 0);  // Send empty response for redirection
    } else {
        // Unsuccessfull login
        char resp_str[512];
        size_t n =
            snprintf(resp_str, sizeof(resp_str),
                     "<!DOCTYPE html>"
                     "<html lang=\"cs\">"
                     "<head>"
                     "<title>Automato</title>"
                     "<meta charset=\"UTF-8\">"
                     "<meta http-equiv=\"refresh\" content=\"2;url=/login\">"
                     "<title>Automato redir</title>"
                     "</head>"
                     "<style>"
                     "body {"
                     "  font-family: Arial, sans-serif;"
                     "}"
                     "</style>"
                     "<body><h2>Automato</h2>%s"
                     "</body>"
                     "</html>",
                     t("Neúspěšné přihlášení"));

        // Send response
        httpd_resp_send(req, resp_str, n);
    }

    return ESP_OK;
}

// handler for catching nets logo
esp_err_t logo_image_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(
        req,
        "image/png");  // Adjust the MIME type if your image format is different
    httpd_resp_send(req, (const char *)logo_image_data, logo_image_data_len);
    return ESP_OK;
}

// Handler for fetching data
// Before each data fetch, construct the displayed strings
esp_err_t data_get_handler(httpd_req_t *req) {
    char data[4096];  // Max length of the full response (JSON)

    // Tell the browser we’re returning JSON:
    httpd_resp_set_type(req, "application/json; charset=UTF-8");

    // JSON start
    strcpy(data, "{");
    strcat(data, "}");

    httpd_resp_send(req, data, strlen(data));

    // helper - log data sent
    // char helpchar[5000];
    // utf8_to_ascii(data, helpchar, strlen(data));
    // ESP_LOGI(TAG, "%s", helpchar);
    // ESP_LOGI(TAG, "%s", data);
    return ESP_OK;
}

// Logout handler
esp_err_t logout_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(
        req, "Set-Cookie",
        "auth=; Path=/; HttpOnly; Expires=Thu, 01 Jan 1970 00:00:00 GMT");

    wifiapst = WIFIAPST_NORMAL;

    // Redirect to login
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/login");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// favicon handler
esp_err_t favicon_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico, sizeof(favicon_ico));
    return ESP_OK;
}

// Function to start the web server
httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // increase httpd stack
    config.stack_size = 50000;
    config.max_uri_handlers = 30;
    config.task_priority = configMAX_PRIORITIES - 1;  // Set to desired priority

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {.uri = "/",
                                .method = HTTP_GET,
                                .handler = root_get_handler,
                                .user_ctx = NULL};
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t data_uri = {.uri = "/data",
                                .method = HTTP_GET,
                                .handler = data_get_handler,
                                .user_ctx = NULL};
        httpd_register_uri_handler(server, &data_uri);

        httpd_uri_t login_uri = {.uri = "/login",
                                 .method = HTTP_GET,
                                 .handler = login_get_handler,
                                 .user_ctx = NULL};
        httpd_register_uri_handler(server, &login_uri);

        httpd_uri_t login_post_uri = {.uri = "/login",
                                      .method = HTTP_POST,
                                      .handler = login_post_handler,
                                      .user_ctx = NULL};
        httpd_register_uri_handler(server, &login_post_uri);

        httpd_uri_t logout_uri = {.uri = "/logout",
                                  .method = HTTP_GET,
                                  .handler = logout_handler,
                                  .user_ctx = NULL};
        httpd_register_uri_handler(server, &logout_uri);


        httpd_uri_t image_uri = {.uri = "/logo",
                                 .method = HTTP_GET,
                                 .handler = logo_image_get_handler,
                                 .user_ctx = NULL};
        httpd_register_uri_handler(server, &image_uri);


        // favicon handler
        httpd_uri_t favicon_uri = {.uri = "/favicon.ico",
                                   .method = HTTP_GET,
                                   .handler = favicon_get_handler,
                                   .user_ctx = NULL};
        httpd_register_uri_handler(server, &favicon_uri);
    }
    return server;
}
