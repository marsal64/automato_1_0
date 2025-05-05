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
            if (!strncmp(info.key, "o_", 2) && strlen(info.key) == 13 &&
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
          "body{font-family:Helvetica,Arial,sans-serif;margin:0;}"
          ".wrapper{max-width:800px;margin:0;border:1px solid #bbb;}"
          ".headbar{display:flex;align-items:center;justify-content:space-between;"
          "padding:6px 8px;box-sizing:border-box;}"
          ".grid{display:grid;grid-template-columns:1fr 1fr;}"
          ".prices,.actions{border-top:1px solid #bbb;padding:6px;box-sizing:border-box;}"
          ".actions{border-left:1px solid #bbb;}"
          ".prices b{color:#c00;}"
          ".logo-txt{font-weight:bold;font-size:1.2rem;margin-left:6px;}"
          "</style>"
          "<script>"
          "function logoff(){location.href='/logout';}\n"
          "function updateDom(d){\n"
          "  document.getElementById('datetime').innerHTML = d.datetime;\n"
          "  /* prices */\n"
          "  const ulp=document.getElementById('prices-list'); ulp.innerHTML='';\n"
          "  d.prices.forEach(p=>{\n"
          "     const li=document.createElement('li');\n"
          "     li.innerHTML = (p.key===d.now_key?'<b>':'') + "
          "                    p.key + '&nbsp;:&nbsp;' + p.val + "
          "                    (p.key===d.now_key?'</b>':'');\n"
          "     ulp.appendChild(li);\n"
          "  });\n"
          "  /* actions */\n"
          "  const ula=document.getElementById('actions-list'); ula.innerHTML='';\n"
          "  d.actions.forEach(a=>{\n"
          "     const li=document.createElement('li');\n"
          "     li.innerHTML = a.action + '&nbsp;&rarr;&nbsp;' + a.timestamp;\n"
          "     ula.appendChild(li);\n"
          "  });\n"
          "}\n"
          "async function fetchData(){\n"
          "  try{\n"
          "    const r=await fetch('/data');\n"
          "    if(r.ok){const j=await r.json(); updateDom(j);} }\n"
          "  catch(e){console.warn('fetch',e);} }\n"
          "window.addEventListener('load',()=>{fetchData(); setInterval(fetchData,1000);});"
          "</script>"
          "</head><body><div class='wrapper'>");

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
          "<span style='display:flex;align-items:center;'>"
          "<img src='/logo' alt='automato' style='width:15%;height:auto;'>"
          "<span class='logo-txt'>automato</span>"
          "</span>"
          "<span id='datetime'>");
    chunk(req, dt_buf);
    chunk(req,
          "</span>"
          "<span>");
    chunk(req, ipaddress);
    chunk(req,
          "</span>"
          "<form action='/setup' method='get' style='margin:0'>"
          "<button type='submit'>");
    chunk(req, t("Nastav"));
    chunk(req,
          "</button></form>"
          "</div>");

    /* ---------- grid row --------------------------------------------- */
    chunk(req, "<div class='grid'>");

    /* prices column (UL gets id) -------------------------------------- */
    chunk(req, "<div class='prices'><h3>");
    chunk(req, t("Ceny"));
    chunk(req, "</h3><ul id='prices-list' style='margin:0;padding-left:1em;'>");
    for (size_t i = 0; i < n_prices; ++i) {
        char line[64];
        snprintf(line, sizeof(line), "<li>%s&nbsp;:&nbsp;%s</li>", prices[i].key, prices[i].val);
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
    chunk(req, t("Poslední akce"));
    chunk(req, "</h3><ul id='actions-list' style='margin:0;padding-left:1em;'>");
    for (int i = 0; i < NUMLASTACTIONSLOG && last_actions_log[i].action[0]; ++i) {
        char line[512];
        snprintf(line, sizeof(line), "<li>%s&nbsp;&rarr;&nbsp;%s</li>", last_actions_log[i].action,
                 last_actions_log[i].timestamp);
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
        APP("{\"action\":\"%s\",\"timestamp\":\"%s\"}%s", last_actions_log[i].action, last_actions_log[i].timestamp,
            (i + 1 < NUMLASTACTIONSLOG && last_actions_log[i + 1].action[0] ? "," : ""));
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
          "body{font-family:Helvetica,Arial,sans-serif;margin:0;background:#fafafa}"
          ".wrapper{margin:20px 0 0 20px;max-width:860px;border:1px solid #bbb;"
          "         padding:20px;background:#fff}"
          "table{width:100%;border-collapse:collapse;margin-top:10px}"
          "th,td{border:1px solid #ccc;padding:6px;text-align:center}"
          "th{background:#eee}"
          "button{padding:4px 10px;margin:2px}"
          ".wide{width:160px}"
          /* logo bar */
          ".logo{display:flex;align-items:center;margin:20px 0 0 20px}"
          ".logo img{height:38px;width:auto}"
          ".logo span{font-weight:bold;font-size:1.4rem;margin-left:8px}"
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
    chunk(req, t("Nastavení podmínek"));
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
          "<button id='saveBtn' style='float:right;'>");
    chunk(req, t("Potvrzení"));
    chunk(req,
          "</button>"
          "<br style='clear:both'>"
          "<script>");

    /* ------- embed the JSON ------- */
    chunk(req, "const initData = ");
    chunk(req, json ? json : "[]");
    chunk(req, ";\n");

    /* ------- tiny helper JS ------- */
    /* ---- JavaScript helper block in setup_get_handler() ---------------- */
    chunk(req,
          "const tbody = document.querySelector('#condTable tbody');\n"
          "function buildRow(d){\n"
          "  const tr = document.createElement('tr');\n"
          "  tr.innerHTML = `"
          "<td><input type='checkbox' ${d.enabled ? 'checked' : ''}></td>"
          "<td><input class='wide'  value='${d.left  ?? \"\"}'></td>"
          "<td><input style='width:40px' value='${d.op    ?? \"\"}'></td>"
          "<td><input class='wide'  value='${d.right ?? \"\"}'></td>"
          "<td><input class='wide'  value='${d.action?? \"\"}'></td>"
          "<td><button class='del'>&#128465;</button></td>`;\n"
          "  tbody.appendChild(tr);\n"
          "}\n"
          "function render(){ tbody.innerHTML=''; initData.forEach(buildRow); }\n"
          "render();\n"
          "document.getElementById('addBtn').onclick = () => {\n"
          "  initData.push({ enabled: 1, left: \"\", op: \"\", right: \"\", action: \"\" });\n"
          "  render();\n"
          "};\n"
          "tbody.addEventListener('click', e => {\n"
          "  if (e.target.classList.contains('del')) {\n"
          "    const idx = [...tbody.children].indexOf(e.target.closest('tr'));\n"
          "    initData.splice(idx, 1); render();\n"
          "  }\n"
          "});\n"
          "document.getElementById('saveBtn').onclick = async () => {\n"
          "  const rows = [...tbody.children];\n"
          "  const out = rows.map(r => {\n"
          "    const inp = r.querySelectorAll('input');\n"
          "    return {\n"
          "      enabled: inp[0].checked ? 1 : 0,\n"
          "      left:    inp[1].value.trim(),\n"
          "      op:      inp[2].value.trim(),\n"
          "      right:   inp[3].value.trim(),\n"
          "      action:  inp[4].value.trim()\n"
          "    };\n"
          "  }).filter(r => r.left !== '' || r.action !== '');\n"
          "  try {\n"
          "    const resp = await fetch('/setup', {\n"
          "      method: 'POST',\n"
          "      headers: { 'Content-Type': 'application/json' },\n"
          "      body: JSON.stringify(out)\n"
          "    });\n"
          "    if (resp.ok) alert('");
    chunk(req, t("Uloženo"));
    chunk(req, "!'); else alert('");
    chunk(req, t("Chyba při ukládání"));
    chunk(req,
          "');\n"
          "  } catch (ex) { alert('");
    chunk(req, t("Chyba"));
    chunk(req,
          ": ' + ex); }\n"
          "};\n");


    chunk(req, "</script></div></body></html>");

    free(json); /* keep this line */
    return httpd_resp_send_chunk(req, NULL, 0);
}

/* ==========================  SET‑UP (POST)  ========================= *
 * Accepts JSON array like:
 *  [ {enabled:1,left:'OTEPH',op:'>',right:'0',action:'REL1ON'}, ... ]
 * Replaces the global `conditions[]`, fills trailing slot "" and zeroes
 * the rest, then (optionally) persists to NVS the way you already do.
 * Responds 204 No Content on success.
 * ------------------------------------------------------------------- */
esp_err_t setup_post_handler(httpd_req_t *req) {
    /* ----- read body --------------------------------------------------- */
    char buf[2048];
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
        esp_err_t err = nvs_set_blob(nvs_handle_storage, "conditions", /* key                     */
                                     conditions,                       /* data                    */
                                     sizeof(conditions));              /* size                    */

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

        httpd_uri_t setup_uri = {.uri = "/setup", .method = HTTP_GET, .handler = setup_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &setup_uri);

        // favicon handler
        httpd_uri_t favicon_uri = {
            .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &favicon_uri);
    }
    return server;
}
