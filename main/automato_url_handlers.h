/*
 * URL handlers
 */

/* names of all three pages – indexes: 0=provoz,1=dennitrh,2=nastaveni */
static const char *page_uri[3]   = {"/provoz", "/dennitrh", "/nastaveni"};
static const char *page_label[3] = {"Provoz", "Denní trh", "Nastavení"};

/* helper: build a <nav> bar;   active_idx = 0,1,2 */
static void nav_bar(char *buf, size_t max, int active_idx)
{
    size_t n = 0;
    n += snprintf(buf+n, max-n, "<nav>");
    for (int i = 0; i < 3 && n < max; ++i) {
        n += snprintf(buf+n, max-n,
            "<a class=\"tab%s\" href=\"%s\">%s</a>",
            (i==active_idx)?" active":"", page_uri[i], t(page_label[i]));
    }
    snprintf(buf+n, max-n, "</nav>");
}

/*----------------------------------------------------------------------------
 * Root page  ("Operation" – index 0)
 *--------------------------------------------------------------------------*/
esp_err_t root_get_handler(httpd_req_t *req)
{
    /* 1. Authorisation -------------------------------------------------- */
    char cookie_value[128];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) ||
        strcmp(cookie_value, "1") != 0 || current_user_id == -1) {
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* 2.  date / time / weekday / ip ----------------------------------- */
    time_t now; time(&now);
    struct tm tm_info; localtime_r(&now,&tm_info);
    static const char *dow_cz[]={"Ne","Po","Út","St","Čt","Pá","So"};
    static const char *dow_en[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    const char *dow = (gst_lang==LANG_CZ)?dow_cz[tm_info.tm_wday]:dow_en[tm_info.tm_wday];
    char timestr[64]; strftime(timestr,sizeof(timestr),"%d.%m.%Y&nbsp;%H:%M:%S",&tm_info);

    char statusline[256];
    snprintf(statusline,sizeof(statusline),
             "<span class=\"date\">%s</span>&nbsp;&nbsp;&nbsp;"
             "<span class=\"weekday\">%s</span>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
             "<span class=\"ip\">%s</span>", timestr,dow,ipaddress);

    /* 3. navigation ------------------------------------------------------ */
    char navbar[256];
    nav_bar(navbar,sizeof(navbar),0);   /* 0 = Operation page */

    /* 4. Build HTML ------------------------------------------------------ */
    char html[RESP_SIZE];
        const char *html_lang = (gst_lang == LANG_CZ) ? "cs" : "en";

    size_t n = snprintf(html,sizeof(html),
        "<!DOCTYPE html><html lang=\"%s\"><head>"
        "<meta charset=\"UTF-8\"><title>%s</title>"
        "<style>body{margin:0;font-family:Arial,sans-serif;}"
        "header{display:flex;align-items:center;padding:10px 20px;box-shadow:0 2px 4px rgba(0,0,0,.1);}"
        "header img{height:48px;}"
        ".status{flex:1;text-align:center;font-size:.95rem;}"
        ".status .date{font-weight:600;} .status .weekday{margin:0 6px;color:#555;} .status .ip{font-family:monospace;color:#006;}"
        "button.logout{padding:4px 10px;font-size:.8rem;cursor:pointer;}"
        /* nav tabs */
        "nav{display:flex;justify-content:center;gap:40px;margin:30px 0;}"
        "a.tab{padding:6px 14px;text-decoration:none;color:#000;border:1px solid #bbb;border-radius:4px;font-size:.85rem;}"
        "a.tab.active{background:#e6f2ff;border-color:#339;}"
        "</style>"
        "<script>function logoff(){window.location.href='/logout';}</script></head><body>"
        "<header><img src=\"/logo\" alt=\"%s\"><span class=\"status\">%s</span><button class=\"logout\" onclick=\"logoff()\">%s</button></header>%s"
        "<!--  page content would go here  -->"
        "</body></html>",
        html_lang,             /* <html lang=...> */
        t("Automato"),             /* <title> */
        t("Automato"),             /* logo alt */
        statusline,
        t("Odhlásit"),             /* button  */
        navbar);

    httpd_resp_send(req, html, n);
    return ESP_OK;
}

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
        httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
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
    char data[4096];    // Max length of the full response (JSON)
    char buffer[1024];  // Max length of one item buffer
    char ins[256];      // helper string

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
