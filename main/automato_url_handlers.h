/*
 *  automato_url_handlers.h  – root page now uses AJAX (no meta refresh)
 */

#ifndef HTTPD_RESP_USE_STRLEN
#define HTTPD_RESP_USE_STRLEN -1 /* ESP‑IDF < v5 compatibility */
#endif

/* ----------------------------------------------------------------------
 *  Tiny helpers
 * --------------------------------------------------------------------*/
static inline void chunk(httpd_req_t *r, const char *s) { httpd_resp_send_chunk(r, s, HTTPD_RESP_USE_STRLEN); }

static int price_cmp(const void *a, const void *b) /* newest → oldest */
{
    const char *ka = ((const char (*)[13])a)[0];
    const char *kb = ((const char (*)[13])b)[0];
    return strcmp(kb, ka);
}

/* ----------------------------------------------------------------------
 *  Root page handler  (HTML → AJAX)
 * --------------------------------------------------------------------*/
esp_err_t root_get_handler(httpd_req_t *req) {
    /* ---------- auth -------------------------------------------------- */
    char cookie_value[32];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) || strcmp(cookie_value, "1") != 0 ||
        current_user_id == -1) {
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* ---------- runtime data (key of “now”) --------------------------- */
    char now_key[13] = "";
    if (nntptime_status) strncpy(now_key, r_rrrrmmddhh, 12);

    /* ---------- prices from NVS -------------------------------------- */
    typedef struct {
        char key[13];
        char val[16];
    } price_t;
    price_t prices[400];
    size_t n_prices = 0;

    nvs_iterator_t it = NULL;
    if (nvs_entry_find(NVS_DEFAULT_PART_NAME, "storage", NVS_TYPE_STR, &it) == ESP_OK) {
        do {
            nvs_entry_info_t info;
            nvs_entry_info(it, &info);
            if (!strncmp(info.key, "o_", 2) && strlen(info.key) == 12 &&
                n_prices < sizeof(prices) / sizeof(prices[0])) {
                strncpy(prices[n_prices].key, info.key + 2, 12);
                prices[n_prices].key[12] = 0;
                size_t sz = sizeof(prices[n_prices].val);
                if (nvs_get_str(nvs_handle_storage, info.key, prices[n_prices].val, &sz) == ESP_OK) ++n_prices;
            }
        } while (nvs_entry_next(&it) == ESP_OK);
        nvs_release_iterator(it);
    }
    qsort(prices, n_prices, sizeof(price_t), price_cmp);

    /* ---------- HTML start ------------------------------------------- */
    httpd_resp_set_type(req, "text/html; charset=UTF-8");
    chunk(req,
          "<!DOCTYPE html><html><head>"
          "<meta name='viewport' content='width=device-width, initial-scale=1'>"
          "<style>"
          "body{font-family:Arial,sans-serif;margin:0;background:#"
          "fafafa}"
          /* common wrapper */
          ".wrapper{"
          "    margin:20px 0 0 20px;"
          "    max-width:860px;"
          "    border:1px solid #bbb;"
          "    padding:0 20px 20px 20px;"
          "    background:#fff;"
          "}"
          /* logo bar (same as /setup) */
          ".logo{display:flex;align-items:center;margin:20px 0 0 20px}"
          ".logo img{height:38px;width:auto}"
          ".logo span{font-weight:bold;font-size:1.4rem;margin-left:8px}"
          /* rest of layout */
          ".grid{display:grid;grid-template-columns:1fr 1fr}"
          ".prices,.actions{border-top:1px solid "
          "#bbb;padding:6px;box-sizing:border-box}"
          ".actions{border-left:1px solid #bbb}"
          ".actions small{font-size:0.75em;color:#555}"
          ".actions li{margin-bottom:4px}"
          ".prices b{color:#c00}"
          /* centre both column headings */
          ".prices h3,.actions h3{margin:0 0 8px 0;text-align:center}"
          /* one unified horizontal bar (date + IP + button) */
          ".headbar{display:flex;justify-content:space-between;align-items:"
          "center;height:46px;padding:0 8px;}"
          "</style>"
          "<script>"
          "function logoff(){location.href='/logout';}\n"
          "function updateDom(d){\n"
          "  document.getElementById('datetime').innerHTML = d.datetime;\n"
          "  /* prices */\n"
          "  const ulp=document.getElementById('prices-list'); "
          "ulp.innerHTML='';\n"
          "  d.prices.forEach(p=>{\n"
          "     const li=document.createElement('li');\n"
          "const k = p.key;"
          "const fmt = "
          "`${k.slice(0,4)}-${k.slice(4,6)}-${k.slice(6,8)}&nbsp;&nbsp;&nbsp;${k."
          "slice(8,10)}`;"
          "li.innerHTML = (p.key === d.now_key ? '<b>' : '') + fmt + "
          "'&nbsp;:&nbsp;&nbsp;&nbsp;' + p.val + (p.key === "
          "d.now_key ? "
          "'</b>' : '');"
          "     ulp.appendChild(li);\n"
          "  });\n"
          "  /* actions */\n"

          "  /* actions --------------------------------------------------------- */\n"
          "  const ula = document.getElementById('actions-list');\n"
          "  ula.innerHTML = '';\n"
          "  d.actions.forEach(a => {\n"
          "      const li = document.createElement('li');\n"
          "      li.innerHTML = a.action + '&nbsp;&rarr;&nbsp;' + a.timestamp +"
          "         (a.desc ? '<br><small>' + a.desc + '</small>' : '');"



          "      ula.appendChild(li);\n"
          "  });\n"
          "}"

          "async function fetchData(){\n"
          "  try{\n"
          "    const r=await fetch('/data');\n"
          "    if(r.ok){const j=await r.json(); updateDom(j);} }\n"
          "  catch(e){console.warn('fetch',e);} }\n"
          "window.addEventListener('load',()=>{fetchData(); "
          "setInterval(fetchData,1000);});"
          "</script>"
          "</head><body>"
          /* unified header -------------------------------------------------- */
          "<div class='logo'>"
          "<img src='/logo' alt='automato'>"
          "<span>automato</span>"
          "</div>"
          "<div class='wrapper'>");

    /* ---------- head bar --------------------------------------------- */
    char dt_buf[64];
    if (nntptime_status) {
        strftime(dt_buf, sizeof(dt_buf), "%Y-%m-%d&nbsp;%H:%M:%S", &timeinfo_sntp);
        int dw = timeinfo_sntp.tm_wday;
        if (dw < 0 || dw > 6) dw = 0;
        snprintf(dt_buf + strlen(dt_buf), sizeof(dt_buf) - strlen(dt_buf), "&nbsp;(%s)", translatedays[dw]);
    } else
        strcpy(dt_buf, t("čas nenastaven"));

    chunk(req,
          "<div class='headbar'>"
          "<span id='datetime'>");
    chunk(req, dt_buf);
    chunk(req,
          "</span>"
          "<span>");
    chunk(req, ipaddress);
    chunk(req, "</span>");
    chunk(req,
          "<form action='/setup' method='get' style='margin:0'>"
          "<button type='submit'>");
    chunk(req, t("Akce"));
    chunk(req, "</button></form>");
    chunk(req,
          "<form action='/descriptions' method='get' style='margin:0'>"
          "<button type='submit'>");
    chunk(req, t("Popisy akcí"));
    chunk(req, "</button></form>");
    chunk(req, "<form action='/settings' style='margin-left:8px'><button>");
    chunk(req, t("Nastavení"));
    chunk(req,
          "</button></form>"
          "</div>");

    /* ---------- grid row --------------------------------------------- */
    chunk(req, "<div class='grid'>");

    /* prices column (UL gets id) -------------------------------------- */
    chunk(req, "<div class='prices'><h3>");
    chunk(req, t("Ceny OTE"));
    /* no bullets – but keep 1 em indent for “air” on the left */
    chunk(req,
          "</h3><ul id='prices-list'"
          " style='margin:0;padding-left:1em;list-style:none;'>");

    for (size_t i = 0; i < n_prices; ++i) {
        char line[128];
        snprintf(line, sizeof(line),
                 "<li>%.4s-%.2s-%.2s&nbsp;&nbsp;&nbsp;%.2s&nbsp;:&nbsp;&nbsp;&"
                 "nbsp;%s</li>",
                 prices[i].key,      // YYYY
                 prices[i].key + 4,  // MM
                 prices[i].key + 6,  // DD
                 prices[i].key + 8,  // HH
                 prices[i].val);
        if (!strcmp(prices[i].key, now_key)) {
            chunk(req, "<b>");
            chunk(req, line);
            chunk(req, "</b>");
        } else {
            chunk(req, line);
        }
    }
    chunk(req, "</ul></div>");

    /* actions column --------------------------------------------------- */
    chunk(req, "<div class='actions'><h3>");
    chunk(req, t("Akce&nbsp;&rarr;&nbsp;naposledy aktivováno"));
    chunk(req, "</h3><ul id='actions-list' style='margin:0;padding-left:1em;'>");

    for (int i = 0; i < NUMLASTACTIONSLOG && last_actions_log[i].action[0]; ++i) {
        const char *desc = "";
        for (int k = 0; actions[k].action_name[0]; ++k)
            if (strcmp(actions[k].action_name, last_actions_log[i].action) == 0) {
                desc = actions[k].action_desc;
                break;
            }
        char line[512];
        snprintf(line, sizeof(line),
                 desc[0] ? "<li>%s&nbsp;&rarr;&nbsp;%s<br><small>%s</small></li>" : "<li>%s&nbsp;&rarr;&nbsp;%s</li>",
                 last_actions_log[i].action, last_actions_log[i].timestamp,
                 desc); /* third arg is used only when desc[0] != '\0' */
        chunk(req, line);
    }

    /* close actions, grid, wrapper ------------------------------------ */
    chunk(req, "</ul></div></div>"); /* </ul> </div>.actions </div>.grid */
    chunk(req, "</div>");            /* </div>.wrapper                  */

    /* ---------- log‑off button --------------------------------------- */
    chunk(req, "<button onclick='logoff()' style='margin:20px 0 0 40px;'>");
    chunk(req, t("Odhlásit"));
    chunk(req, "</button>");

    /* ---------- close document --------------------------------------- */
    chunk(req, "</body></html>");
    return httpd_resp_send_chunk(req, NULL, 0);
}


// ======================  DESCRIPTIONS (POST)  =====================
esp_err_t descriptions_post_handler(httpd_req_t *req) {
    /* ---------- auth ------------------------------------------------ */
    char cookie_value[32];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) || strcmp(cookie_value, "1") != 0 ||
        current_user_id == -1) {
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* ---------- read body ------------------------------------------ */
    char buf[1024];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    /* ---------- overwrite actions[] -------------------------------- */
    int idx = 0;
    cJSON *it = NULL;
    memset(actions, 0, sizeof(actions)); /* clear whole table */
    cJSON_ArrayForEach(it, root) {
        if (idx >= (int)(sizeof(actions) / sizeof(actions[0])) - 1) break;
        strncpy(actions[idx].action_name, cJSON_GetObjectItem(it, "action")->valuestring ?: "",
                sizeof(actions[idx].action_name) - 1);
        strncpy(actions[idx].action_desc, cJSON_GetObjectItem(it, "desc")->valuestring ?: "",
                sizeof(actions[idx].action_desc) - 1);
        ++idx;
    }
    /* mark terminator */
    actions[idx].action_name[0] = actions[idx].action_desc[0] = 0;
    cJSON_Delete(root);

    /* ---------- persist to NVS ------------------------------------ */
    esp_err_t err = nvs_set_blob(nvs_handle_storage, "actions", actions, sizeof(actions));
    if (err == ESP_OK) err = nvs_commit(nvs_handle_storage);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Saving actions to NVS failed (%d)", err);
    else
        ESP_LOGI(TAG, "Action-descriptions written to NVS (%zu B)", sizeof(actions));

    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

//======================  DESCRIPTIONS (GET)  ======================
esp_err_t descriptions_get_handler(httpd_req_t *req) {
    /* ---------- auth (same as /setup) ------------------------------ */
    char cookie_value[32];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) || strcmp(cookie_value, "1") != 0 ||
        current_user_id == -1) {
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* ---------- JSON of current actions --------------------------- */
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; actions[i].action_name[0]; ++i) {
        cJSON *it = cJSON_CreateObject();
        cJSON_AddStringToObject(it, "action", actions[i].action_name);
        cJSON_AddStringToObject(it, "desc", actions[i].action_desc);
        cJSON_AddItemToArray(root, it);
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    /* ---------- HTML ---------------------------------------------- */
    httpd_resp_set_type(req, "text/html; charset=UTF-8");
    chunk(req,
          "<!DOCTYPE html><html lang='cs'><head>"
          "<meta name='viewport' content='width=device-width,initial-scale=1'>"
          "<title>Popisy akcí</title>"
          "<style>"
          "body{font-family:Arial,sans-serif;margin:0;background:#fafafa}"
          ".wrapper{margin:20px 0 0 20px;max-width:860px;border:1px solid #bbb;"
          "padding:0 20px 20px 20px;background:#fff;}"
          "table{width:100%;border-collapse:collapse;margin-top:10px}"
          "th,td{border:1px solid #ccc;padding:6px;text-align:center}"
          "th{background:#eee}"
          "input[type=text]{width:100%;box-sizing:border-box}"
          "button{padding:4px 10px;margin:2px}"
          "button.confirm{background:#d33;color:#fff;border:1px solid #a00;border-radius:4px}"
          ".logo{display:flex;align-items:center;margin:20px 0 0 20px}"
          ".logo img{height:38px;width:auto}"
          ".logo span{font-weight:bold;font-size:1.4rem;margin-left:8px}"
          "</style></head><body>"
          "<div class='logo'><img src='/logo' alt='automato'><span>automato</span></div>"
          "<div class='wrapper'><h2>");
    chunk(req, t("Popisy akcí"));
    chunk(req,
          "</h2><table id='descTable'><thead><tr>"
          "<th>");
    chunk(req, t("Akce"));
    chunk(req, "</th><th style='width:150px;'>");
    chunk(req, t("Popis"));
    chunk(req,
          "</th></tr></thead><tbody></tbody></table>"
          "<button id='backBtn' onclick=\"location.href='/'\" "
          "style='float:right;margin-left:8px;'>");
    chunk(req, t("Zpět"));
    chunk(req,
          "</button>"
          "<button id='saveBtn' class='confirm' style='float:right;'>");
    chunk(req, t("Potvrzení"));
    chunk(req,
          "</button><br style='clear:both'>"

          "<script>"
          "const initData = ");
    chunk(req, json ? json : "[]");
    chunk(req,
          ";"
          "const tbody=document.querySelector('#descTable tbody');"
          "function buildRow(d){"
          " const tr=document.createElement('tr');"
          " tr.innerHTML=`<td>${d.action}</td>"
          " <td><input type='text' value='${d.desc.replace(/\"/g,'&quot;')}'></td>`;"
          " tbody.appendChild(tr);"
          "}"
          "function render(){tbody.innerHTML='';initData.forEach(buildRow);} render();"

          "document.getElementById('saveBtn').onclick=async()=>{"
          "  const out=[...tbody.children].map(r=>({"
          "     action : r.children[0].textContent.trim(),"
          "     desc   : r.querySelector('input').value.trim().slice(0,30) /* 30 chars */"
          "  }));"
          "  try{"
          "     const resp=await fetch('/descriptions',{method:'POST',"
          "           headers:{'Content-Type':'application/json'},"
          "           body:JSON.stringify(out)});"
          "     alert(resp.ok?'");


    chunk(req, t("Uloženo"));
    chunk(req, "':'");
    chunk(req, t("Chyba při ukládání"));
    chunk(req,
          "');"
          "  }catch(e){alert('");
    chunk(req, t("Chyba"));
    chunk(req,
          ":'+e);}"
          "};"
          "</script></div></body></html>");

    free(json);
    return httpd_resp_send_chunk(req, NULL, 0);
}

/* ----------------------------------------------------------------------
 *  /data  – returns live data as JSON
 * --------------------------------------------------------------------*/
esp_err_t data_get_handler(httpd_req_t *req) {
    /* ---------- build date/time string (same format as root) --------- */
    char dt_buf[64];
    if (nntptime_status) {
        strftime(dt_buf, sizeof(dt_buf), "%Y-%m-%d&nbsp;%H:%M:%S", &timeinfo_sntp);
        int dw = timeinfo_sntp.tm_wday;
        if (dw < 0 || dw > 6) dw = 0;
        snprintf(dt_buf + strlen(dt_buf), sizeof(dt_buf) - strlen(dt_buf), "&nbsp;(%s)", t(translatedays[dw]));
    } else
        strcpy(dt_buf, t("čas nenastaven"));

    /* ---------- prices (same helper as in root) ---------------------- */
    typedef struct {
        char key[13];
        char val[16];
    } price_t;
    price_t prices[400];
    size_t n_prices = 0;
    nvs_iterator_t it = NULL;
    if (nvs_entry_find(NVS_DEFAULT_PART_NAME, "storage", NVS_TYPE_STR, &it) == ESP_OK) {
        do {
            nvs_entry_info_t info;
            nvs_entry_info(it, &info);
            if (!strncmp(info.key, "o_", 2) && strlen(info.key) == 12 &&
                n_prices < sizeof(prices) / sizeof(prices[0])) {
                strncpy(prices[n_prices].key, info.key + 2, 12);
                prices[n_prices].key[12] = 0;
                size_t sz = sizeof(prices[n_prices].val);
                if (nvs_get_str(nvs_handle_storage, info.key, prices[n_prices].val, &sz) == ESP_OK) ++n_prices;
            }
        } while (nvs_entry_next(&it) == ESP_OK);
        nvs_release_iterator(it);
    }
    qsort(prices, n_prices, sizeof(price_t), price_cmp);

    /* ---------- compose JSON ----------------------------------------- */
    httpd_resp_set_type(req, "application/json; charset=UTF-8");

    char buf[4096];
    char *p = buf;
    size_t rem = sizeof(buf);

#define APP(...) p += snprintf(p, rem - (p - buf), __VA_ARGS__)

    APP("{\"datetime\":\"%s\",\"now_key\":\"%s\",\"prices\":[", dt_buf, r_rrrrmmddhh);
    for (size_t i = 0; i < n_prices; ++i) {
        APP("{\"key\":\"%s\",\"val\":\"%s\"}%s", prices[i].key, prices[i].val, (i + 1 < n_prices ? "," : ""));
    }
    APP("],\"actions\":[");
    for (int i = 0; i < NUMLASTACTIONSLOG && last_actions_log[i].action[0]; ++i) {
        const char *desc = "";
        for (int k = 0; actions[k].action_name[0]; ++k)
            if (strcmp(actions[k].action_name, last_actions_log[i].action) == 0) {
                desc = actions[k].action_desc;
                break;
            }

        /* ---------- compose JSON ----------------------------------------- */
        APP("{\"action\":\"%s\",\"timestamp\":\"%s\",\"desc\":\"%s\"}%s", last_actions_log[i].action, /* %s #1 */
            last_actions_log[i].timestamp,                                                            /* %s #2 */
            desc,                                                                         /* %s #3 – plain text only */
            (i + 1 < NUMLASTACTIONSLOG && last_actions_log[i + 1].action[0]) ? "," : ""); /* %s #4 */
    }
    APP("]}");

#undef APP
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t login_get_handler(httpd_req_t *req) {
    char html[RESP_SIZE];
    size_t n = snprintf(html, sizeof(html),
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
                        t("automato"),                      /* <title>            */
                        t("automato"),                      /* logo alt           */
                        t("automato - přihlášení obsluhy"), /* heading            */
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
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
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
        if (sscanf(buf, "username=%72[^&]&password=%72s", inusername, inpassword) == 2) {
            ESP_LOGI(TAG, "Uzivatelske jmeno: %s", inusername);
            // ESP_LOGI(TAG, "Password: %s", inpassword);
        } else {
            ESP_LOGE(TAG, "Error parsing string from cookie\n");
        }

        // verify if username/password matches the given users
        // user id 0 ?
        if ((strcmp(users[0].username, inusername) == 0) && (strcmp(users[0].password, inpassword) == 0)) {
            ESP_LOGI(TAG, "Uspesne overeni hesla pro uzivatele %s", users[0].username);
            current_user_id = 0;
        }
        // user id 1 ?
        else if ((strcmp(users[1].username, inusername) == 0) && (strcmp(users[1].password, inpassword) == 0)) {
            ESP_LOGI(TAG, "Uspesne overeni hesla pro uzivatele %s", users[1].username);
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
        size_t n = snprintf(resp_str, sizeof(resp_str),
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
    httpd_resp_set_type(req,
                        "image/png");  // Adjust the MIME type if your image format is different
    httpd_resp_send(req, (const char *)logo_image_data, logo_image_data_len);
    return ESP_OK;
}

// Logout handler
esp_err_t logout_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Set-Cookie", "auth=; Path=/; HttpOnly; Expires=Thu, 01 Jan 1970 00:00:00 GMT");

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

/* ==========================  SET‑UP (GET)  ========================= */
esp_err_t setup_get_handler(httpd_req_t *req) {
    /* ---------- auth -------------------------------------------------- */
    char cookie_value[32];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) || strcmp(cookie_value, "1") != 0 ||
        current_user_id == -1) {
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* ---------- build JSON of the current conditions --------------- */
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < MAXNUMCONDITONS && conditions[i].left[0]; ++i) {
        cJSON *it = cJSON_CreateObject();
        cJSON_AddNumberToObject(it, "enabled", conditions[i].active);
        cJSON_AddStringToObject(it, "left", conditions[i].left);
        cJSON_AddStringToObject(it, "op", conditions[i].operator);
        cJSON_AddStringToObject(it, "right", conditions[i].right);
        cJSON_AddStringToObject(it, "action", conditions[i].action);
        cJSON_AddItemToArray(root, it);
    }
    char *json = cJSON_PrintUnformatted(root); /* remember to free() */
    cJSON_Delete(root);

    httpd_resp_set_type(req, "text/html; charset=UTF-8");

    chunk(req,
          "<!DOCTYPE html><html lang='cs'><head>"
          "<meta name='viewport' content='width=device-width,initial-scale=1'>"
          "<title>");
    chunk(req, t("Nastavení"));
    chunk(req,
          "</title>"

          /* ---------- CSS --------------------------------------------- */
          "<style>"
          "body{font-family:Arial,sans-serif;margin:0;background:#"
          "fafafa}"
          /* common wrapper */
          ".wrapper{"
          "    margin:20px 0 0 20px;"
          "    max-width:860px;"
          "    border:1px solid #bbb;"
          "    padding:0 20px 20px 20px;"
          "    background:#fff;"
          "}"
          "table{width:100%;border-collapse:collapse;margin-top:10px}"
          "th,td{border:1px solid #ccc;padding:6px;text-align:center}"
          "th{background:#eee}"
          "button{padding:4px 10px;margin:2px}"
          "button.confirm{background:#d33;color:#fff;border:1px solid #a00;border-radius:4px}"
          ".wide{width:160px}"
          /* logo bar */
          ".logo{display:flex;align-items:center;margin:20px 0 0 20px}"
          ".logo img{height:38px;width:auto}"
          ".logo span{font-weight:bold;font-size:1.4rem;margin-left:8px}"
          ".headbar{display:flex;justify-content:space-between;align-items:"
          "center;height:46px;padding:0 8px;}"
          "</style>"
          "</head><body>"
          /* ---------- page header with logo (top‑left) ----------------- */
          "<div class='logo'>"
          "<img src='/logo' alt='automato'>"
          "<span>");
    chunk(req, t("automato"));
    chunk(req,
          "</span>"
          "</div>"
          "<div class='wrapper'>"
          "<h2>");
    chunk(req, t("Nastavení podmínek a akcí"));
    chunk(req,
          "</h2>"
          "<table id='condTable'>"
          "<thead><tr>"
          "<th>");
    chunk(req, t("Povoleno"));
    chunk(req, "</th><th>");
    chunk(req, t("Levá strana"));
    chunk(req, "</th><th>");
    chunk(req, t("Operátor"));
    chunk(req, "</th><th>");
    chunk(req, t("Pravá strana"));
    chunk(req, "</th><th>");
    chunk(req, t("Akce"));
    chunk(req, "</th><th>");
    chunk(req, t("Odstranit"));
    chunk(req,
          "</th>"
          "</tr></thead><tbody></tbody></table>"
          "<button id='addBtn'>+ ");
    chunk(req, t("Nová podmínka"));
    chunk(req,
          "</button>"
          "<button id='backBtn' onclick=\"location.href='/'\" "
          "        style='float:right;margin-left:8px;'>");
    chunk(req, t("Zpět"));
    chunk(req,
          "</button>" /* NEW */
          "<button id='saveBtn' class='confirm' style='float:right;'>");
    chunk(req, t("Potvrzení"));
    chunk(req,
          "</button>"
          "<br style='clear:both'>"
          "<script>\n");
    /* ---------- <script> ------------------------------------------------ */
    chunk(req, "const LEFTSIDES = [");
    for (int i = 0; DEF_LEFTSIDES[i][0]; ++i) {
        if (i) chunk(req, ",");
        chunk(req, "\"");
        chunk(req, DEF_LEFTSIDES[i]);
        chunk(req, "\"");
    }
    chunk(req, "];\nconst OPERATORS = [");
    for (int i = 0; DEF_OPERATORS[i][0]; ++i) {
        if (i) chunk(req, ",");
        chunk(req, "\"");
        chunk(req, DEF_OPERATORS[i]);
        chunk(req, "\"");
    }
    chunk(req, "];\nconst ACTIONS = [");
    for (int i = 0; actions[i].action_name[0]; ++i) {
        if (i) chunk(req, ",");
        chunk(req, "\"");
        chunk(req, actions[i].action_name);
        chunk(req, "\"");
    }
    chunk(req, "];\n\n");

    // helper for MAXNUMCONDITONS
    char tmp[8];
    sprintf(tmp, "%d", MAXNUMCONDITONS);
    chunk(req, "const MAX_ROWS = ");
    chunk(req, tmp); /* e.g. “10”                       */
    chunk(req, ";\n\n");

    chunk(req, "const initData = ");
    chunk(req, json ? json : "[]");
    chunk(req,
          ";\n\n"
          "/* DOM helpers ------------------------------------------------*/\n"
          "const tbody = document.querySelector('#condTable tbody');\n\n"
          "function buildRow(d){\n"
          "  const tr = document.createElement('tr');\n"
          "  tr.innerHTML = `\n"
          "    <td><input type='checkbox' ${d.enabled?\"checked\":\"\"}></td>\n"
          "    <td>\n"
          "      <select class='wide'>\n"
          "        ${LEFTSIDES.map(v=>`<option value='${v}' "
          "${v===d.left?\"selected\":\"\"}>${v}</option>`).join('')}\n"
          "      </select>\n"
          "    </td>\n"
          "    <td>\n"
          "      <select style='width:60px'>\n"
          "        ${OPERATORS.map(v=>`<option value='${v}' "
          "${v===d.op?\"selected\":\"\"}>${v}</option>`).join('')}\n"
          "      </select>\n"
          "    </td>\n"
          "    <td><input type='text' class='wide' "
          "value='${d.right??\"\"}'></td>\n"
          "    <td>\n"
          "      <select class='wide'>\n"
          "        ${ACTIONS.map(v=>`<option value='${v}' "
          "${v===d.action?\"selected\":\"\"}>${v}</option>`).join('')}\n"
          "      </select>\n"
          "    </td>\n");
    chunk(req,
          "    <td><button class='del'>&#128465;</button></td>`;\n"
          "  tbody.appendChild(tr);\n"
          "}\n\n"
          "function render(){ tbody.innerHTML=''; initData.forEach(buildRow); }\n"
          "render();\n"
          "\n"
          "tbody.addEventListener('click', e => {\n"
          "  const btn = e.target.closest('button.del');\n"
          "  if (!btn) return;                          /* not the trash-can */\n"
          "  const row = btn.closest('tr');\n"
          "  const idx = [...tbody.children].indexOf(row);\n"
          "  if (idx !== -1) { initData.splice(idx, 1); render(); }\n"
          "});\n");

    chunk(req, "render();\n\n");

    chunk(req,
          "document.getElementById('addBtn').onclick = () => {\n"
          "  if (initData.length >= MAX_ROWS) {\n"
          "      alert('");
    chunk(req, t("Maximální počet podmínek"));
    chunk(req, " '+MAX_ROWS+' ");
    chunk(req, t("byl dosažen"));
    chunk(req,
          "');\n"
          "      return;               /* refuse to add more */\n"
          "  }\n"
          "  initData.push({ enabled:1,\n"
          "                  left:   LEFTSIDES[0]||'',\n"
          "                  op:     OPERATORS[0]||'',\n"
          "                  right:  '0',\n"
          "                  action: ACTIONS[0]||'' });\n"
          "  render();\n"
          "};\n");

    chunk(req,
          "document.getElementById('saveBtn').onclick = async () => {\n"
          "  const out = [...tbody.children].map(r => {\n"
          "    const ck   = r.querySelector('input[type=checkbox]');\n"
          "    const sels = r.querySelectorAll('select');   /* 0-left 1-op "
          "2-action */\n"
          "    const txt  = r.querySelector('input[type=text]');\n"
          "    return {\n"
          "      enabled : ck.checked ? 1 : 0,\n"
          "      left    : sels[0].value,\n"
          "      op      : sels[1].value,\n"
          "      right   : txt.value.trim(),\n"
          "      action  : sels[2].value\n"
          "    };\n"
          "  }).filter(r => r.left !== '' || r.action !== '');\n"
          "\n"
          "  /* -------- client-side check of “Pravá strana” -------- */\n"
          "  const bad = out.find(r => r.right.length > ");
    chunk(req, RIGHTEVALSIZE);
    chunk(req,
          " ||\n"
          "                           !/^[0-9+\\-*/.]*$/.test(r.right));\n"
          "  if (bad) {\n"
          "    alert(bad.right.length > ");
    chunk(req, RIGHTEVALSIZE);
    chunk(req,
          "\n"
          "          ? '");
    chunk(req, t("Maximální počet znaků na pravé straně je "));
    chunk(req, RIGHTEVALSIZE);
    chunk(req,
          "'\n"
          "          : '");
    chunk(req, t("Pravá strana smí obsahovat pouze čísla a znaky + - * / ."));
    chunk(req,
          "');\n"
          "    return;\n"
          "  }\n"
          "\n"
          "  try {\n"
          "    const resp = await fetch('/setup', {\n"
          "      method  : 'POST',\n"
          "      headers : { 'Content-Type': 'application/json' },\n"
          "      body    : JSON.stringify(out)\n"
          "    });\n"
          "    alert(resp.ok ? '");
    chunk(req, t("Uloženo"));
    chunk(req, "' : '");
    chunk(req, t("Chyba při ukládání"));
    chunk(req,
          "');\n"
          "  } catch(ex) {\n"
          "    alert('");
    chunk(req, t("Chyba"));
    chunk(req,
          " : ' + ex);\n"
          "  }\n"
          "};\n");

    chunk(req, "</script>\n");

    chunk(req, "</div></body></html>");

    free(json); /* keep this line */
    return httpd_resp_send_chunk(req, NULL, 0);
}

/* ==========================  SET‑UP (POST)  ========================= *
 * Accepts JSON array like:
 *  [ {enabled:1,left:'EUROTE',op:'>',right:'0',action:'REL1ON'}, ... ]
 * Replaces the global `conditions[]`, fills trailing slot "" and zeroes
 * the rest, then (optionally) persists to NVS the way you already do.
 * Responds 204 No Content on success.
 * ------------------------------------------------------------------- */
esp_err_t setup_post_handler(httpd_req_t *req) {
    /* ---------- auth -------------------------------------------------- */
    char cookie_value[32];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) || strcmp(cookie_value, "1") != 0 ||
        current_user_id == -1) {
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* ----- read body --------------------------------------------------- */
    char buf[3000];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    memset(conditions, 0, sizeof(conditions)); /* clear whole table */

    int idx = 0;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, root) {
        if (idx >= MAXNUMCONDITONS - 1) break; /* leave last as terminator */
        conditions[idx].active = cJSON_GetObjectItem(it, "enabled")->valueint ? 1 : 0;
        strncpy(conditions[idx].left, cJSON_GetObjectItem(it, "left")->valuestring ?: "",
                sizeof(conditions[idx].left) - 1);
        strncpy(conditions[idx].operator, cJSON_GetObjectItem(it, "op")->valuestring ?: "",
                sizeof(conditions[idx].operator) - 1);
        strncpy(conditions[idx].right, cJSON_GetObjectItem(it, "right")->valuestring ?: "",
                sizeof(conditions[idx].right) - 1);
        strncpy(conditions[idx].action, cJSON_GetObjectItem(it, "action")->valuestring ?: "",
                sizeof(conditions[idx].action) - 1);
        ++idx;
    }
    /* mark the new end */
    conditions[idx].active = 1;
    conditions[idx].left[0] = conditions[idx].operator[0] = conditions[idx].right[0] = conditions[idx].action[0] = 0;

    cJSON_Delete(root);

    /* ---------- PERSIST TO NVS -------------------------------------- */
    {                                                                  /* <- keep the scope small       */
        esp_err_t err = nvs_set_blob(nvs_handle_storage, "conditions", /* key */
                                     conditions,                       /* data                    */
                                     sizeof(conditions));              /* size */

        if (err == ESP_OK) {
            err = nvs_commit(nvs_handle_storage);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Saving conditions to NVS failed (%d)", err);
        } else {
            ESP_LOGI(TAG, "Conditions written to NVS (%zu B)", sizeof(conditions));
        }
    }
    /* ---------------------------------------------------------------- */

    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t settings_get_handler(httpd_req_t *req) {
    /* ---------- auth -------------------------------------------------- */
    char cookie_value[32];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) || strcmp(cookie_value, "1") != 0 ||
        current_user_id == -1) {
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "text/html; charset=UTF-8");

    chunk(req, "<!DOCTYPE html><html><head>)");

    chunk(req,
          "<script>"
          "function logoff(){location.href='/logout';}\n"
          "</script>");

    chunk(req,
          /* ---------- CSS --------------------------------------------- */
          "<style>"
          "body{font-family:Arial,sans-serif;margin:0;background:#"
          "fafafa}"
          /* common wrapper */
          ".wrapper{"
          "    margin:20px 0 0 20px;"
          "    max-width:860px;"
          "    border:1px solid #bbb;"
          "    padding:0 20px 20px 20px;"
          "    background:#fff;"
          "}"
          "table{width:100%;border-collapse:collapse;margin-top:10px}"
          "th,td{border:1px solid #ccc;padding:6px;text-align:center}"
          "th{background:#eee}"
          "button{padding:4px 10px;margin:2px}"
          "button.confirm{background:#d33;color:#fff;border:1px solid #a00;border-radius:4px}"
          ".wide{width:160px}"
          /* logo bar */
          ".logo{display:flex;align-items:center;margin:20px 0 0 20px}"
          ".logo img{height:38px;width:auto}"
          ".logo span{font-weight:bold;font-size:1.4rem;margin-left:8px}"
          ".headbar{display:flex;justify-content:space-between;align-items:"
          "center;height:46px;padding:0 8px;}"
          "</style>");

    chunk(req, "</head><body>");

    /* ---------- page header with logo (top‑left) ----------------- */
    chunk(req,
          "<div class='logo'>"
          "<img src='/logo' alt='automato'>"
          "<span>");
    chunk(req, t("automato"));
    chunk(req,
          "</span>"
          "</div>");
    chunk(req, "<div class='wrapper'>");
    // form start
    chunk(req, "<form action='/settings' method='post'>");

    chunk(req, "<h2 align='center'>");
    chunk(req, t("Uživatelská nastavení"));
    chunk(req, "</h2>");

    // language selector
    chunk(req, t("Změny potvrďte stiskem tlačítka 'Potvrzení'"));
    chunk(req,
          "<br><br><label>Jazyk/language:&nbsp;</label>"
          "<select name='lang'>"
          "<option value='0'");
    if (gst_lang == LANG_CZ) chunk(req, "selected");
    chunk(req,
          ">Česky</option>"
          "<option value='1'");
    if (gst_lang == LANG_EN) chunk(req, "selected");
    chunk(req,
          ">English</option>"
          "</select><br><br>");

    // password fields
    chunk(req, t("Nové heslo uživatele automato:"));
    chunk(req, "&nbsp;<input type='password' name='user_pass' value=''><br><br>");
    // servis only if service user
    if (current_user_id == 1) {
        chunk(req, t("Nové heslo uživatele servis:"));
        chunk(req, "&nbsp;<input type='password' name='serv_pass' value=''><br><br>");
    } else {
        // hidden
        chunk(req, "<input type='hidden' name='serv_pass' value=''>");
    }

    // buttons
    chunk(req, "<button type='submit' class='confirm'>");
    chunk(req, t("Potvrzení"));
    chunk(req, "</button>");
    chunk(req,
          "<button id='backBtn' type='button'"
          " onclick=\"window.location.href='/'\""
          " style='float:right;margin-left:8px;'>");
    chunk(req, t("Zpět"));
    chunk(req, "</button>");
    chunk(req, "<br><br>");
    // is "servis" - allow nvs wipe out
    if (current_user_id == 1) {
        chunk(req, "<br><br>");
        chunk(req, t("Pozor, stiskem tlačítka níže změníte všechna nastavení "
                     "na výchozí "
                     "a restartujete zařízení (nutno znovu připojit na wifi)"));
        chunk(req, "<br>");
        // Make sure we give it a name and value:
        chunk(req, "<button type='submit' name='wipe' value='1'>");
        chunk(req, t("Výchozí nastavení (!)"));
        chunk(req, "</button>");
    }
    //
    chunk(req, "</form></div>");

    /* ---------- log‑off button --------------------------------------- */
    chunk(req, "<button onclick='logoff()' style='margin:20px 0 0 40px;'>");
    chunk(req, t("Odhlásit"));
    chunk(req, "</button>");

    chunk(req, "</body></html>");
    return httpd_resp_send_chunk(req, NULL, 0);
}

esp_err_t settings_post_handler(httpd_req_t *req) {
    /* ---------- auth -------------------------------------------------- */
    char cookie_value[32];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) || strcmp(cookie_value, "1") != 0 ||
        current_user_id == -1) {
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // Read the form body
    char buf[256];  // Input buffer for form data

    // 1) Read the body
    int len = req->content_len;
    if (len <= 0 || len >= sizeof(buf)) return ESP_FAIL;
    int r = httpd_req_recv(req, buf, len);
    if (r <= 0) return ESP_FAIL;
    buf[r] = '\0';

    // 2) Parse key/value pairs
    int lang = 0, wipe = 0;
    char userp[33] = {0}, servp[33] = {0};
    for (char *p = buf; p; /* */) {
        char *pair = p;
        char *amp = strchr(pair, '&');
        if (amp) *amp = '\0';
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = '\0';
            if (!strcmp(pair, "lang"))
                lang = atoi(eq + 1);
            else if (!strcmp(pair, "user_pass"))
                strncpy(userp, eq + 1, sizeof(userp) - 1);
            else if (!strcmp(pair, "serv_pass"))
                strncpy(servp, eq + 1, sizeof(servp) - 1);
            else if (!strcmp(pair, "wipe"))
                wipe = atoi(eq + 1);
        }
        p = amp ? amp + 1 : NULL;
    }

    // 3) If they hit the wipe-factory-defaults button, do that *first*
    if (wipe) {
        httpd_resp_set_type(req, "text/html; charset=UTF-8");
        httpd_resp_send(req,
                        t("<html><body>Výchozí nastavení, restartuji zařízení "
                          "(po restartu připojte na WiFi)</body></html>"),
                        HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGW(TAG, "Factory reset: wiping entire NVS partition");

        nvs_flash_deinit();                  // close all open handles
        ESP_ERROR_CHECK(nvs_flash_erase());  // <-- one call erases everything
        esp_restart();

        return ESP_OK;
    }

    // 4) Otherwise apply language & password changes
    gst_lang = lang;
    nvs_set_blob(nvs_handle_storage, "gst_lang", &gst_lang, sizeof(gst_lang));
    int err = nvs_commit(nvs_handle_storage);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed for gst_lang (%d)", err);
    } else {
        ESP_LOGI(TAG, "Saved new gst_lang to NVS");
    }

    if (userp[0]) {
        strncpy(users[0].password, userp, sizeof(users[0].password) - 1);
        users[0].password[sizeof(users[0].password) - 1] = '\0';
        nvs_set_str(nvs_handle_storage, "pwd_automato", userp);
        int err = nvs_commit(nvs_handle_storage);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "nvs_commit failed for pwd_automato (%d)", err);
        } else {
            ESP_LOGI(TAG, "Saved new automato password to NVS");
        }
    }
    if (servp[0]) {
        strncpy(users[1].password, servp, sizeof(users[1].password) - 1);
        users[1].password[sizeof(users[1].password) - 1] = '\0';
        nvs_set_str(nvs_handle_storage, "pwd_servis", servp);
        int err = nvs_commit(nvs_handle_storage);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "nvs_commit failed for pwd_servis (%d)", err);
        } else {
            ESP_LOGI(TAG, "Saved new servis password to NVS");
        }
    }

    // 5) Redirect back
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/settings");
    httpd_resp_send(req, NULL, 0);
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
        httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &root_uri);

        /* existing GET handler keeps same URI */
        httpd_uri_t setup_uri_get = {
            .uri = "/setup", .method = HTTP_GET, .handler = setup_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &setup_uri_get);

        /* NEW: POST handler that saves the rules */
        httpd_uri_t setup_uri_post = {
            .uri = "/setup", .method = HTTP_POST, .handler = setup_post_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &setup_uri_post);

        httpd_uri_t data_uri = {.uri = "/data", .method = HTTP_GET, .handler = data_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &data_uri);

        httpd_uri_t login_uri = {.uri = "/login", .method = HTTP_GET, .handler = login_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &login_uri);

        httpd_uri_t login_post_uri = {
            .uri = "/login", .method = HTTP_POST, .handler = login_post_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &login_post_uri);

        httpd_uri_t logout_uri = {.uri = "/logout", .method = HTTP_GET, .handler = logout_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &logout_uri);

        httpd_uri_t image_uri = {
            .uri = "/logo", .method = HTTP_GET, .handler = logo_image_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &image_uri);

        // favicon handler
        httpd_uri_t favicon_uri = {
            .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &favicon_uri);

        // after registering settings
        httpd_uri_t settings_uri_get = {
            .uri = "/settings", .method = HTTP_GET, .handler = settings_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &settings_uri_get);

        httpd_uri_t settings_uri_post = {
            .uri = "/settings", .method = HTTP_POST, .handler = settings_post_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &settings_uri_post);

        httpd_uri_t desc_get = {
            .uri = "/descriptions", .method = HTTP_GET, .handler = descriptions_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &desc_get);

        httpd_uri_t desc_post = {
            .uri = "/descriptions", .method = HTTP_POST, .handler = descriptions_post_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &desc_post);
    }
    return server;
}
