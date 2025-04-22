/*
 * s150u_wifi URL handlers
 */

// Root handler
esp_err_t root_get_handler(httpd_req_t *req) {
    // debug
    // ESP_LOGI(TAG, "apst:%d", apst);
    // ESP_LOGI(TAG, "current_user_id:%d  %s ",current_user_id,
    // users[current_user_id].username);

    // authentication verification part /////////
    char cookie_value[128];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) ||
        strcmp(cookie_value, "1") != 0 || current_user_id == -1) {
        // Redirect to login if not authenticated
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // construct response string
    char resp_str[RESP_SIZE];
    strcpy(
        resp_str,

        "<!DOCTYPE html>"
        "<html lang=\"cs\">"
        "<head>"
        "<title>S150U-WIFI</title>"
        "<style>"

        "body {"
        "  font-family: Arial, sans-serif;"
        //"  background-color: #E0E0E0;"
        "}"

        ".grid-container {"
        "  display: grid;"
        "  grid-template-areas:"
        "    'g1 g2 g12 g12 g13 g3'"
        "    'g4 g14 g5 g5 g15 g6'"
        "    'g4 g7 g7 g7 g7 g6'"
        "    'g4 g8 g9 g10 g11 g6';"
        "  grid-template-columns: 1fr 1fr 1fr 1fr 1fr 1fr;"
        // "grid-template-columns: 100px 100px 100px 100px 100px 100px;"
        "  gap: 0px;"
        "  max-width: 800px;"
        "  background-color: #75AA9D;"
        "  box-shadow: 10px 10px 10px grey;"
        "  border-radius: 20px;"
        "  align-items: center;"
        "  justify-items: center;"
        "  font-size: 16px;"
        "  margin-left: 5px;"
        "  margin-top: 10px;"
        "  margin-bottom: 20px;"
        "  padding-bottom: 15px;"
        "  padding-top: 7px;"
        "  box-sizing: border-box;"
        "}"

        ".grid-container button {"
        "    background-color: #483C32;"
        "    color: white;"
        "    border-radius: 5px;"
        "    aspect-ratio: 1 / 1;"
        "    width: 60px;"
        "    height: 60px;"
        "    font-size: 18px;"
        "    margin: 0;"
        "    padding: 0;"
        "}"

        ".item1 { grid-area: g1; }"  // logo
        ".item2 { grid-area: g2; justify-self: start; text-align: left; color: "
        "#483C32; font-weight:bold}"  // typ sauny
        ".item3 { grid-area: g3; justify-self: end; text-align: center; "
        "padding-right: 20px; color: #483C32; font-size: 14px;}"  // login
        ".item4 { grid-area: g4; }"  // tlačitko "nahoru"
        ".item5 { grid-area: g5; margin-top:10px; padding-top:12px; "
        "padding-bottom: 0px; margin-bottom: -10px; background-color:#483C32; "
        "color:#FFFFFF; width: 100%; height: 100%; text-align: center; "
        "font-size: 460%;}"  // "big" uprostřed - teplota
        ".item14 { grid-area: g14; margin-top: 10px; padding-top:12px; "
        "padding-bottom: 0px; margin-bottom: -10px; padding-left: 10px; "
        "margin-left: 10px; border-radius: 10px 0 0 0; "
        "background-color:#483C32; "
        "color:#FFFFFF; display: flex; align-items: center; justify-content: "
        "flex-start; "
        "text-align: center; width: 100%; height: 100%; font-size: 150%;}" /* "big"
                                                                              vlevo
                                                                            */
        ".item15 { grid-area: g15; margin-top: 10px; padding-top:12px; "
        "padding-bottom: 0px; margin-bottom: -10px; padding-right: 10px; "
        "margin-right: 10px; border-radius: 0 10px 0 0; "
        "background-color:#483C32; color:#FFFFFF; display: flex; align-items: "
        "center; justify-content: flex-start; text-align: center; width: 100%; "
        "height: "
        "100%; font-size: 150%;}" /* "big" vpravo */
        ".item7 { grid-area: g7; border-radius: 0 0 10px 10px; padding-top: "
        "15px; background-color:#483C32; color:#FFFFFF; width: 100%; "
        "font-size: "
        "90%}"                                           // souhrnná hlášení
        ".item6 { grid-area: g6; }"                      // tlačítko "dolů"
        ".item8 { grid-area: g8; padding-top: 20px;}"    // tlačítko "světlo"
        ".item9 { grid-area: g9; padding-top: 20px;}"    // tlačítko "SET"
        ".item10 { grid-area: g10; padding-top: 20px;}"  // tlačítko "ON"
        ".item11 { grid-area: g11; padding-top: 20px;}"  // tlačítko "OFF"
        ".item12 { grid-area: g12; color: #483C32; font-size: 14px;}"  // datum
                                                                       // a čas
        ".item13 { grid-area: g13; justify-self: end; text-align: right; "
        "padding-right: 5px; color: #483C32; font-size: 14px;}"  // IP adresa

        // formatting the setup container incl. sliders contained here
        ".form-container {"
        "  background-color: white;"
        "  border: 2px solid black;"
        "  border-radius: 15px;"
        "  box-shadow: 5px 5px 10px grey;"
        "  padding: 20px;"
        "  margin-top: 20px;"
        "  max-width: 800px;"
        "  margin-left: 5px;"
        "  margin-right: auto;"
        "  box-sizing: border-box;"
        "}"

        ".form-container input[type='range'] {"
        "  -webkit-appearance: none;"
        "  width: 75%;"
        "  height: 8px;"
        "  background: #ff9800;"
        "  border-radius: 5px;"
        "  outline: none;"
        "}"

        ".form-container input[type='range']::-webkit-slider-thumb {"
        "  -webkit-appearance: none;"
        "  appearance: none;"
        "  width: 30px;"
        "  height: 30px;"
        "  background: #ff9800;"
        "  border-radius: 50%;"
        "  cursor: pointer;"
        "}"

        ".form-container input[type='range']::-moz-range-thumb {"
        "  width: 30px;"
        "  height: 30px;"
        "  background: #ff9800;"
        "  border-radius: 50%;"
        "  cursor: pointer;"
        "}"

        "</style>"

        "<meta charset=\"UTF-8\"><script>"

        "function getData(){"
        "fetch('/data')"
        ".then(response => response.json())"
        ".then(data => {"

        // příprava datových položek
        "   var displayText = data.datetime;"
        "   document.getElementById('datetime').innerHTML = displayText;"
        "   displayText = data.typsauny; "
        "document.getElementById('typsauny').innerHTML = displayText;"
        "   displayText = data.big_line; "
        "document.getElementById('big_line').innerHTML = displayText;"
        "   displayText = data.big_line_left; "
        "document.getElementById('big_line_left').innerHTML = displayText;"
        "   displayText = data.big_line_right; "
        "document.getElementById('big_line_right').innerHTML = displayText;"
        "   displayText = data.ipaddress; "
        "document.getElementById('ipaddress').innerHTML = displayText;"
        "   displayText = data.current_user; "
        "document.getElementById('current_user').innerHTML = "
        "'login:<br>'+displayText;"
        // here used manually TEXTITEMS_COUNT :-/
        "   displayText = data.t_1 + '<br>'+ data.t_2 + '<br>' + data.t_3 + "
        "'<br>' + data.t_4 + '<br>' + data.t_5 + '<br>' + data.t_6 + '<br>' + "
        "data.t_7 + '<br>' + data.t_8 + data.t_9 + data.t_10;"
        "   document.getElementById('t_').innerHTML = displayText;"
        // Full page refresh if rfp
        "   if (data.rfp === 1) {"
        "	   fetch('/resetrfp')"  // reset rfp
        ".then(()=>{"
        "window.location.reload();"
        "})"
        "    }"
        "});"
        "}"

        "function updateValue_temp_set(val) {"
        "    document.getElementById('temp_set_display').innerText = '  '+ val "
        "+'°C';"
        "}"

        "function updateValue_doba_behu_set(val) {"
        // round to 5 minutes
        "    val = Math.round(val / (5 * 60)) * (5 * 60);"
        "    var hours = Math.floor(val / 3600);"
        "    var minutes = Math.floor((val % 3600) / 60);"
        "    var hoursStr = hours.toString().padStart(2, '0');"
        "    var minutesStr = minutes.toString().padStart(2, '0');"
        "    var valstr = hoursStr + ':' + minutesStr;"
        "    document.getElementById('doba_behu_set_display').innerText = '  ' "
        "+ "
        "valstr;"
        "}"

        "function updateValue_odlo_cas_set(val) {"
        // round to 15 minutes
        "    val = Math.round(val / (15 * 60)) * (15 * 60);"
        "    var hours = Math.floor(val / 3600);"
        "    var minutes = Math.floor((val % 3600) / 60);"
        "    var hoursStr = hours.toString().padStart(2, '0');"
        "    var minutesStr = minutes.toString().padStart(2, '0');"
        "    var valstr = hoursStr + ':' + minutesStr;"
        "    document.getElementById('odlo_cas_set_display').innerText = '  ' "
        "+ "
        "valstr;"
        "}"

        // definition of refresh interval
        "setInterval(function() {"
        "getData();"
        "}, 500);"

        "function logoff() {"
        "     window.location.href = '/logout';"
        "}"

        "function button_up() {"
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '/button_up', true);"
        //"xhr.onreadystatechange = function () {"
        //"    if (xhr.readyState === 4 && xhr.status === 200) {"
        //"        console.log('Request successful');"
        //"    }"
        //"};"
        "xhr.send();"
        "return false;"  // Prevent default action
        "}"

        "function button_down() {"
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '/button_down', true);"
        //"xhr.onreadystatechange = function () {"
        //"    if (xhr.readyState === 4 && xhr.status === 200) {"
        //"        console.log('Request successful');"
        //"    }"
        //"};"
        "xhr.send();"
        "return false;"  // Prevent default action
        "}"

        "function button_light() {"
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '/button_light', true);"
        //"xhr.onreadystatechange = function () {"
        //"    if (xhr.readyState === 4 && xhr.status === 200) {"
        //"        console.log('Request successful');"
        //"    }"
        //"};"
        "xhr.send();"
        "return false;"  // Prevent default action
        "}"

        "function button_set() {"
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '/button_set', true);"
        "xhr.onreadystatechange = function () {"
        "    if (xhr.readyState === 4 && xhr.status === 200) {"
        "        console.log('SET pressed');"
        "    }"
        "};"
        "xhr.send();"
        "return false;"  // Prevent default action
        "}"

        "function button_on() {"
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '/button_on', true);"
        //"xhr.onreadystatechange = function () {"
        //"    if (xhr.readyState === 4 && xhr.status === 200) {"
        //"        console.log('Request successful');"
        //"    }"
        //"};"
        "xhr.send();"
        "return false;"  // Prevent default action
        "}"

        "function button_off() {"
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '/button_off', true);"
        //"xhr.onreadystatechange = function () {"
        //"    if (xhr.readyState === 4 && xhr.status === 200) {"
        //"        console.log('Request successful');"
        //"    }"
        //"};"
        "xhr.send();"
        "return false;"  // Prevent default action
        "}"

        "</script>"

        "</head>"

        "<body>"
        "<div class='grid-container'>"

        "<div class='item1'><img src='/logo', style='width:40%; "
        "height:auto; display: flex; align-items: center; justify-content: "
        "flex-start;'></div>"

        "<div class='item2' id='typsauny'></div>"

        "<div class='item3' id='current_user'></div>"

        "<div class='item13' id='ipaddress'></div>"

        "<div class='item12' id='datetime'></div>"

        "<div class='item4'> <button onclick='button_up()'>▲</button></div>"

        "<div class='item5' id='big_line'></div>"

        "<div class='item6'><button onclick='button_down()'>▼</button></div>"

        "<div class='item7' id='t_'></div>"

        "<div class='item8'><button onclick='button_light()'>💡</button></div>"

        "<div class='item9'><button onclick='button_set()'>SET</button></div>"

        "<div class='item10'><button onclick='button_on()'>ON</button></div>"

        "<div class='item11'><button onclick='button_off()'>OFF</button></div>"

        "<div class='item14' id='big_line_left'></div>"

        "<div class='item15' id='big_line_right'></div>"

        "</div>"

        "<br>"
        "<button onclick='logoff()' style='margin-left:40px;' "
        ">Odhlásit</button>"
        "<br>"
        "<br>");

    if (wifiapst == WIFIAPST_SET_GENERAL) {
        char buf[2048];
        strcat(resp_str,
               "<div class='form-container'>"
               "<span style='font-weight:bold; "
               "font-size:150%'>S150U-WIFI nastavení hodnot</span>"
               "<br>"
               "<br>"
               "Změnu údajů potvrďte tlačítkem 'Potvrdit změny', stiskem "
               "'SET' opustíte režim nastavení beze změn"
               "<br>"
               "<br>"
               "<form action='/set_values' method='post'>"

               // nastavení teploty
               "<label for='temp_set'>"
               "<b>Cílová teplota (");
        snprintf(buf, sizeof(buf),
                 "%d .. %d°C):</b></label><br>"
                 "<input type='range' id='temp_set' name='temp_set' min='%d' "
                 "max='%d' "
                 "value='%d' oninput='updateValue_temp_set(temp_set.value)'>"
                 "<span id='temp_set_display' style='font-weight:bold; "
                 "font-size:25px;'>&nbsp;&nbsp;%d°C</span>",
                 defMinTemp, defMaxTemp, defMinTemp, defMaxTemp, nastavenaTemp,
                 nastavenaTemp);
        strcat(resp_str, buf);

        // nastavení doby běhu
        strcat(resp_str,
               "<br>"
               "<label for='doba_behu_set'>"
               "<br><br><b>Doba běhu (hh:mm ");
        char dobaBehu_disp[20];  // hh:mm, but compiler error ;-/
        int hours = dobaBehu / 3600;
        int minutes = (dobaBehu % 3600) / 60;
        snprintf(dobaBehu_disp, sizeof(dobaBehu_disp), "%02d:%02d", hours,
                 minutes);

        snprintf(buf, sizeof(buf),
                 "%s .. %s):</b></label><br>"
                 "<input type='range' id='doba_behu_set' name='doba_behu_set' "
                 "min='%d' max='%d' value='%ld' "
                 "oninput='updateValue_doba_behu_set(doba_behu_set.value)'>"
                 "<span id='doba_behu_set_display' style='font-weight:bold; "
                 "font-size:25px;'>&nbsp;&nbsp;%s</span>",
                 MINDOBABEHU_STR, MAXDOBABEHU_STR, MINDOBABEHU, MAXDOBABEHU,
                 dobaBehu, dobaBehu_disp);
        strcat(resp_str, buf);


        // potvrzovací tlačítko
        strcat(resp_str,
               "<br>"
               "<br>"
               "<br>"
               "<input type='submit' value='Potvrdit změny'>"
               "<br>");

        // čas a datum
        snprintf(
            buf, sizeof(buf),
            "<br><br><b>Pevné nastavení času (h:m):</b><br>"
            "<input type='number' id='hodina_set' name='hodina_set' "
            "value='%d' min='0' max='23' size='2' maxlength='2' style='width: "
            "40px; text-align: center;'>"
            ":"
            "<input type='number' id='minuta_set' name='minuta_set' "
            "value='%d' min='0' max='59' size='2' maxlength='2' style='width: "
            "40px; text-align: center;'><br>"
            "<b>Pevné nastavení data (d.m.rok):</b><br>"
            "<input type='number' id='den_set' name='den_set' value='%d' "
            "size='2' min='1' max='31' maxlength='2'  style='width: 40px; "
            "text-align: center;'> "
            "."
            "<input type='number' id='mesic_set' name='mesic_set' value='%d' "
            "size='2' min='1' max='12' maxlength='2' style='width: 40px; "
            "text-align: center;'>"
            "."
            "<input type='number' id='rok_set' name='rok_set' value='%d' "
            "size='4' min='2024' maxlength='4' style='width: 60px; "
            "text-align: center;'><br>",
            btime.hour, btime.min, btime.date, btime.month, btime.year + 2000);
        strcat(resp_str, buf);
        //
        // nastavení času
        strcat(resp_str,
               "<br><b>Jednorázová korekce data a času sauny:</b><br>");
        for (int i = DATCAS_NENASTAVOVAT; i < DATCAS_NUMSTATES; i++) {
            snprintf(buf, sizeof(buf),
                     "<input type='radio' id='datcas_typ_%d' "
                     "name='datcas_typ_set' value='%d' %s>"
                     "<label for='datcas_typ_%d'>%s</label><br>",
                     i, i, i == DATCAS_NENASTAVOVAT ? "checked" : "", i,
                     typy_datcas[i]);
            strcat(resp_str, buf);
        }


        // korekce čidla
        strcat(resp_str, "<br><b>Korekce čidla (-8 .. +8°C):</b><br>");
        snprintf(buf, sizeof(buf),
                 "<input type='number' id='korekce_c_set' name='korekce_c_set' "
                 "min='-8' max='8' style='width: 36px;' value='%d'>",
                 setup_korekceCidla);
        strcat(resp_str, buf);

        // parní esence - pouze parní sauna
        if (typSauny == TYP_SAUNY_PARNI) {
            strcat(resp_str, "<br><br><b>Parní esence:</b><br>");
            for (int i = 0; i < ESENCE_NUMSTATES; i++) {
                snprintf(buf, sizeof(buf),
                         "<input type='radio' id='parniesence_%d' "
                         "name='parniesence_set' value='%d' %s>"
                         "<label for='parniesence_%d'>%s</label><br>",
                         i, i, setup_parniEsence == i ? "checked" : "", i,
                         typy_parniesence[i]);
                strcat(resp_str, buf);
            }

            // interval esence
            snprintf(buf, sizeof(buf),
                     "<br><b>Interval esence (1 .. 15 minut):</b><br>"
                     "<input type='number' id='interval_esence_set' "
                     "name='interval_esence_set' min='1' max='15' "
                     "style='width: "
                     "40px;' value='%d'>",
                     setup_intervalEsence);
            strcat(resp_str, buf);
        } else {
            // Just ensure the value is still posted, but hide it
            snprintf(buf, sizeof(buf),
                     "<input type='hidden' name='parniesence_set' value='%d'>",
                     setup_parniEsence);
            strcat(resp_str, buf);
            snprintf(
                buf, sizeof(buf),
                "<input type='hidden' name='interval_esence_set' value='%d'>",
                setup_intervalEsence);
            strcat(resp_str, buf);
        }

        // dálkové ovládání
        strcat(resp_str, "<br><br><b>Dálkové ovládání:</b><br>");
        for (int i = 0; i < DALKOVEOVLADANI_NUMSTATES; i++) {
            snprintf(buf, sizeof(buf),
                     "<input type='radio' id='dalkoveovladani_%d' "
                     "name='dalkoveovladani_set' value='%d' %s>"
                     "<label for='dalkoveovladani_%d'>%s</label><br>",
                     i, i, setup_dalkoveOvladani == i ? "checked" : "", i,
                     typy_dalkoveovladani[i]);
            strcat(resp_str, buf);
        }

        // změna hesla uživatele sauna
        snprintf(buf, sizeof(buf),
                 "<br>"
                 "<br>"
                 "<b><label for='pwd_0'>Nové heslo uživatele sauna: "
                 "</label></b><br>"
                 "  <input type='password' id='pwd_0' name='pwd_0'>");
        strcat(resp_str, buf);

        //// část pro uživatele servis

        // záhlaví
        if (current_user_id == 1) {
            // potvrzovací tlačítko (jen pro uživatele servis)
            strcat(resp_str,
                   "<br>"
                   "<br>"
                   "<input type='submit' value='Potvrdit změny'>");

            // servisní část
            strcat(resp_str,
                   "<br><br><br>==================== Servisní část "
                   "====================<br>");
        }

        // typ sauny
        if (current_user_id == 1) {
            // typ sauny
            strcat(resp_str, "<br><b>Typ sauny:</b><br>");
            for (int i = TYP_SAUNY_STD; i < TYP_SAUNY_COUNT; i++) {
                snprintf(buf, sizeof(buf),
                         "<input type='radio' id='set_typ_sauny_%d' "
                         "name='typ_sauny_set' value='%d' %s>"
                         "<label for='set_typ_sauny_%d'>%s</label><br>",
                         i, i, setup_typSauny == i ? "checked" : "", i,
                         typy_saun_long[i]);
                strcat(resp_str, buf);
            }
        } else {
            // Just ensure the value is still posted, but hide it
            snprintf(buf, sizeof(buf),
                     "<input type='hidden' name='typ_sauny_set' value='%d'>",
                     setup_typSauny);
            strcat(resp_str, buf);
        }

        // typ spuštění sauny
        if (current_user_id == 1) {
            // typ spuštění sauny
            strcat(resp_str, "<br><b>Typ spuštění sauny:</b><br>");
            for (int i = TYPSPUSTENI_SAUNY_NORMALNI;
                 i < TYPSPUSTENI_SAUNY_NUMSTATES; i++) {
                snprintf(buf, sizeof(buf),
                         "<input type='radio' id='set_typspusteni_sauny_%d' "
                         "name='typspusteni_sauny_set' value='%d' %s>"
                         "<label "
                         "for='set_typspusteni_sauny_%d'>%s</label><br>",
                         i, i, setup_typSpusteni == i ? "checked" : "", i,
                         typy_spusteni_sauny[i]);
                strcat(resp_str, buf);
            }
        } else {
            // Just ensure the value is still posted, but hide it
            snprintf(
                buf, sizeof(buf),
                "<input type='hidden' name='typspusteni_sauny_set' value='%d'>",
                setup_typSpusteni);
            strcat(resp_str, buf);
        }

        // nastavení kamen - jen standardní
        if (current_user_id == 1 && typSauny == TYP_SAUNY_STD) {
            strcat(resp_str, "<br><b>Nastavení kamen:</b><br>");
            for (int i = KAMNANAST_JEDNOFAZOVA; i < KAMNANAST_NUMSTATES; i++) {
                snprintf(buf, sizeof(buf),
                         "<input type='radio' id='nastavenikamen_%d' "
                         "name='nastavenikamen_set' value='%d' %s>"
                         "<label for='nastavenikamen_%d'>%s</label><br>",
                         i, i, setup_nastaveniKamen == i ? "checked" : "", i,
                         typy_nastaveni_kamen[i]);
                strcat(resp_str, buf);
            }
        } else {
            // Just ensure the value is still posted, but hide it
            snprintf(
                buf, sizeof(buf),
                "<input type='hidden' name='nastavenikamen_set' value='%d'>",
                setup_nastaveniKamen);
            strcat(resp_str, buf);
        }

        // střídání fází
        if (current_user_id == 1) {
            // střídání fází - pouze standardní sauna
            if (typSauny == TYP_SAUNY_STD) {
                strcat(resp_str, "<br><b>Střídání fází:</b><br>");
                for (int i = STRIDANIFAZI_NE; i < STRIDANIFAZI_NUMSTATES; i++) {
                    snprintf(buf, sizeof(buf),
                             "<input type='radio' id='set_stridanifazi_%d' "
                             "name='stridanifazi_set' value='%d' %s>"
                             "<label "
                             "for='stridanifazi_%d'>%s</label><br>",
                             i, i, setup_stridaniFazi == i ? "checked" : "", i,
                             typy_stridanifazi[i]);
                    strcat(resp_str, buf);
                }
            }
        } else {
            // Just ensure the value is still posted, but hide it
            snprintf(buf, sizeof(buf),
                     "<input type='hidden' name='stridanifazi_set' value='%d'>",
                     setup_stridaniFazi);
            strcat(resp_str, buf);
        }

        // blokování světla
        if (current_user_id == 1) {
            // blokování světla
            strcat(resp_str, "<br><b>Blokování světla:</b><br>");
            for (int i = BLOKOVANISVETLA_NE; i < BLOKOVANISVETLA_NUMSTATES;
                 i++) {
                snprintf(buf, sizeof(buf),
                         "<input type='radio' id='set_blokovani_svetla_%d' "
                         "name='blokovani_svetla_set' value='%d' %s>"
                         "<label "
                         "for='set_blokovani_svetla_%d'>%s</label><br>",
                         i, i, setup_blokovaniSvetla == i ? "checked" : "", i,
                         typy_blokovanisvetla[i]);
                strcat(resp_str, buf);
            }
        } else {
            // Just ensure the value is still posted, but hide it
            snprintf(
                buf, sizeof(buf),
                "<input type='hidden' name='blokovani_svetla_set' value='%d'>",
                setup_blokovaniSvetla);
            strcat(resp_str, buf);
        }

        if (current_user_id == 1) {
            // hesla
            strcat(resp_str,
                   "<br>"
                   "<br>"
                   "<b>  <label for='pwd_1'>Nové heslo uživatele servis: "
                   "</label></b><br>"
                   "  <input type='password' id='pwd_1' name='pwd_1'>");
        }

        // potvrzovací tlačítko
        strcat(resp_str,
               "<br>"
               "<br>"
               "<input type='submit' value='Potvrdit změny'>");

        // ukončení form
        strcat(resp_str,
               "</form>"
               "</div>"  // of class container¨
               "</br>"
               "</br>");


        //// Provozní informace
        snprintf(buf, sizeof(buf), "<table>");
        strcat(resp_str, buf);

        // Výrobní číslo
        snprintf(buf, sizeof(buf),
                 "<tr><td style='text-align:right'>Výrobní číslo "
                 "S150U-WIFI:</td><td>%s</td></tr>",
                 vyrobnicislo);
        strcat(resp_str, buf);

        // Firmware
        snprintf(buf, sizeof(buf),
                 "<tr><td style='text-align:right'>Firmware "
                 "S150U-WIFI:</td><td>%s</td></tr>",
                 FW_VERSION);
        strcat(resp_str, buf);

        // MAC
        uint8_t m[6];
        esp_wifi_get_mac(WIFI_IF_STA, m);
        snprintf(buf, sizeof(buf),
                 "<tr><td "
                 "style='text-align:right'>MAC S150U-WIFI:</"
                 "td><td>%02X:%02X:%02X:%02X:%02X:%02X</td></tr>",
                 m[0], m[1], m[2], m[3], m[4], m[5]);
        strcat(resp_str, buf);

        // Počet provozních hodin
        snprintf(buf, sizeof(buf),
                 "<tr><td "
                 "style='text-align:right'>Provozní hodiny S150U:</"
                 "td><td>%d</td></tr>",
                 (int)(setup_celkovaDobaProvozu / 60 / 60));
        strcat(resp_str, buf);

        // Baterie
        snprintf(buf, sizeof(buf),
                 "<tr><td "
                 "style='text-align:right'>Baterie S150U:</"
                 "td><td>%.3fV</td></tr>",
                 ((float)battVolt) / 1000);
        strcat(resp_str, buf);

        // Provozní doba
        snprintf(buf, sizeof(buf),
                 "<tr><td "
                 "style='text-align:right'>Provozní doba:</"
                 "td><td>%d</td></tr>",
                 setup_provozniDoba);
        strcat(resp_str, buf);


        snprintf(buf, sizeof(buf), "</table>");
        strcat(resp_str, buf);

    }  // of setup

    strcat(resp_str,
           "</body>"
           "</html>");

    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Login form handler
esp_err_t login_get_handler(httpd_req_t *req) {
    const char *login_form =
        "<!DOCTYPE html>"
        "<html lang=\"cs\">"
        "<head>"
        "<title>S150U-WIFI</title>"
        "<meta charset=\"UTF-8\">"
        "<style>"
        "body {"
        "  font-family: Arial, sans-serif;"
        //					"  background-color: #E0E0E0;"
        "  display: flex;"
        "  justify-content: center;"
        "  align-items: center;"
        "  height: 100vh;"
        "  margin: 0;"
        "}"
        "form {"
        "  border: 1px solid #ddd;"
        "  padding: 20px;"
        "  box-shadow: 2px 2px 5px rgba(0,0,0,0.2);"
        "}"
        "</style>"
        "</head>"
        "<body>"
        "<form action=\"/login\" method=\"post\">"
        "<img src='/logo' alt='automato' style='width:15%; height:auto;'>"
        "<h2>Automato - přihlášení obsluhy</h2>"
        "Uživatelské jméno:<br> <input type=\"text\" name=\"username\" "
        "maxlength=\"32\" ><br>"
        "<br>"
        "Heslo:<br> <input type=\"password\" name=\"password\" "
        "maxlength=\"32\"><br>"
        "<br>"
        "<input type=\"submit\" value=\"Přihlášení\">"
        "</form>"
        "</body></html>";
    httpd_resp_send(req, login_form, HTTPD_RESP_USE_STRLEN);
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
        const char *resp_str =
            "<!DOCTYPE html>"
            "<html lang=\"cs\">"
            "<head>"
            "<title>S150U-WIFI</title>"
            "<meta charset=\"UTF-8\">"
            "<meta http-equiv=\"refresh\" content=\"2;url=/login\">"
            "<title>S150U-WIFI redir</title>"
            "</head>"
            "<style>"
            "body {"
            "  font-family: Arial, sans-serif;"
            //						"  background-color:
            // #E0E0E0;"
            "}"
            "</style>"
            "<body><h2>Sauna S150U-WIFI</h2>"
            "Neúspěšné přihlášení"
            "</body>"
            "</html>";

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
    httpd_resp_send(req, (const char *)logo_image_data,
                    logo_image_data_len);
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

    // data items:

    // čas z internetu/sauny
    // if (nntptime_status)
    // else
    //	strcpy(ins, "Datum/čas nedostupný");
    snprintf(buffer, sizeof(buffer), "\"datetime\":\"%s\"", saunacas_string);
    strcat(data, buffer);
    strcat(data, ",");

    // ip adresa
    snprintf(buffer, sizeof(buffer), "\"ipaddress\":\"%s\"", ipaddress);
    strcat(data, buffer);
    strcat(data, ",");

    // curent user
    snprintf(buffer, sizeof(buffer), "\"current_user\":\"%s\"",
             users[current_user_id].username);
    strcat(data, buffer);
    strcat(data, ",");

    // typ sauny
    strcpy(ins, "S150U&nbsp;");
    // typ sauny
    strcat(ins, typy_saun[typSauny]);
    // odložený start?
    if (typSpusteni == 1) {
        strcat(ins, "/odl.");
    }
    snprintf(buffer, sizeof(buffer), "\"typsauny\":\"%s\"", ins);
    strcat(data, buffer);
    strcat(data, ",");

    // text aktuální teplota / nastavení / rh
    if (comm_status == COMST_ERR || comm_status == COMST_TIMEOUT ||
        apst == APST_OFF || apst == APST_STARTING) {
        // do not display temperature if communication error
        strcat(data, "\"big_line\":\"--°C\",");
    } else if (wifiapst == WIFIAPST_SET_RH) {
        snprintf(
            buffer, sizeof(buffer),
            "\"big_line\":\"<span "
            "style='font-size:14px;'>&UpArrowDownArrow;&nbsp;</"
            "span><span style='font-size:32px;'>💧"
            "%s%s%s</span><span "
            "style='font-size:14px;'>&nbsp;&UpArrowDownArrow;&nbsp;&nbsp;%s</"
            "span>\"",  // ⇅
            p_rh_set > TYP_SAUNY_RHSUCHA ? "💧" : "",
            p_rh_set > TYP_SAUNY_RHVLHKA ? "💧" : "",
            p_rh_set > TYP_SAUNY_RHMOKRA ? "💧" : "",
            typy_saun_minitext[p_rh_set]);
        strcat(data, buffer);
        strcat(data, ",");
    } else if (wifiapst == WIFIAPST_SET_TERM) {
        snprintf(
            buffer, sizeof(buffer),
            "\"big_line\":\"<span "
            "style='font-size:14px;'>tep.&UpArrowDownArrow;&nbsp;</"
            "span>%d°C<span "
            "style='font-size:14px;'>&nbsp;&UpArrowDownArrow;</span>\"",  // ⇅
            nastavenaTemp_set);
        strcat(data, buffer);
        strcat(data, ",");
    } else if (wifiapst == WIFIAPST_SET_TIMEOUT) {
        int hours_set = dobaBehu_set / 3600;
        int minutes_set = (dobaBehu_set % 3600) / 60;
        // int secs = dobaBehu_set % 60;
        snprintf(
            buffer, sizeof(buffer),
            "\"big_line\":\"<span "
            "style='font-size:14px;'>běh&UpArrowDownArrow;&nbsp;</"
            "span>%02d:%02d<span "
            "style='font-size:14px;'>&nbsp;&UpArrowDownArrow;</span>\"",  // ⇅
            hours_set, minutes_set);
        strcat(data, buffer);
        strcat(data, ",");
    } else if (wifiapst == WIFIAPST_SET_ODLO) {
        // dummy

        int hours_set = p_odlozenyCas_set / 3600;
        int minutes_set = (p_odlozenyCas_set % 3600) / 60;
        // int secs = dobaBehu_set % 60;
        snprintf(
            buffer, sizeof(buffer),
            "\"big_line\":\"<span "
            "style='font-size:14px;'>odl.&UpArrowDownArrow;&nbsp;</"
            "span>%02d:%02d<span "
            "style='font-size:14px;'>&nbsp;&UpArrowDownArrow;</span>\"",  // ⇅
            hours_set, minutes_set);
        strcat(data, buffer);
        strcat(data, ",");
    } else {
        snprintf(buffer, sizeof(buffer), "\"big_line\":\"%d°C\"", aktTemp);
        strcat(data, buffer);
        strcat(data, ",");
    }

    // big_line_left
    strcpy(ins, "&nbsp");
    snprintf(buffer, sizeof(buffer), "\"big_line_left\":\"%s\"", ins);
    strcat(data, buffer);
    strcat(data, ",");

    // big_line_right
    // svítí - nesvítí
    if (stavRelat & 0b00000100) {
        strcpy(ins, "💡");
    } else {
        strcpy(ins, "&nbsp");
    }
    snprintf(buffer, sizeof(buffer), "\"big_line_right\":\"%s\"", ins);
    strcat(data, buffer);
    strcat(data, ",");

    // t položky - nulování
    for (int i = 0; i < TEXTITEMS_COUNT; i++) {
        tt[i].priority = 0;  // 0 = do not display (default)
        strcpy(tt[i].text, "&nbsp;");
    }

    // index pro t položky
    uint8_t t_ind = 0;

    // něco probíhá ve základní jednotce?
    if ((state2 & 0x0F) != 0) {
        strcpy(
            tt[t_ind].text,
            "<span style='color:orange'>&nbsp;&nbsp;⚒&nbsp;"
            "Detekováno nastavování v jiné jednotce než je S150U-WiFi</span>");
        tt[t_ind].priority = 9;
        t_ind += 1;
    }

    // t_header default "blocking"
    if (comm_status == COMST_TIMEOUT) {
        strcpy(tt[t_ind].text,
               "<span style='color:red'>&nbsp;&nbsp;⚠&nbsp;Není "
               "komunikace se základní jednotkou sauny</span>");
        tt[t_ind].priority = 10;  // default 10 - do not display anything else
    } else if (comm_status == COMST_ERR) {
        strcpy(tt[t_ind].text,
               "<span style='color:red'>&nbsp;&nbsp;⚠&nbsp;Chyba "
               "komunikace se základní jednotkou sauny</span>");
        tt[t_ind].priority = 10;  // default 10 - do not display anything else
    } else if (apst == APST_OFF) {
        strcpy(
            tt[t_ind].text,
            "<span style='color:white'>&nbsp;&nbsp;◯</span>&nbsp;&nbsp;Sauna "
            "vypnuta (zapněte tlačítkem ON)");
        tt[t_ind].priority = 10;  // default 10 - do not display anything else
    } else if (apst == APST_STARTING) {
        strcpy(tt[t_ind].text,
               "<span style='color:orange'>&nbsp;&nbsp;↗&nbsp;&nbsp;Probíhá "
               "inicializace sauny");
        tt[t_ind].priority = 10;  // default 10 - do not display anything else
    } else if (apst == APST_TO_CONFIRM && odlozenyNastaveno && !hotRUN) {
        // čeká se na potvrzení za stavu nastaveného odloženého času
        int hours = p_odlozenyCas / 3600;
        int minutes = (p_odlozenyCas % 3600) / 60;
        int secs = p_odlozenyCas % 60;
        // Convert the current time to total seconds
        int seconds_start =
            btime.hour * 3600 + btime.min * 60 + btime.sec + p_odlozenyCas;
        // přes půlnoc? - korekce
        if (seconds_start > 24 * 60 * 60) {
            seconds_start = seconds_start - 24 * 60 * 60;
        }
        int hours_start = seconds_start / 3600;
        int minutes_start = (seconds_start % 3600) / 60;
        int secs_start = seconds_start % 60;
        snprintf(
            buffer, sizeof(buffer),
            "<span style='color:orange'>&nbsp;&nbsp;🕑&nbsp;Odložený start "
            "sauny v %02d:%02d:%02d (za %dh %dm %ds)&nbsp;&nbsp;ON - "
            "start,&nbsp;OFF - vypnout</span>",
            hours_start, minutes_start, secs_start, hours, minutes, secs);
        strcpy(tt[t_ind].text, buffer);
        tt[t_ind].priority = 10;  // default 10 - do not display anything else
    } else if (wifiapst == WIFIAPST_SET_ODLO) {
        // probíhá režim nastavení odloženého času ve WiFi
        int hours = p_odlozenyCas_set / 3600;
        int minutes = (p_odlozenyCas_set % 3600) / 60;
        int secs = p_odlozenyCas_set % 60;

        // Convert the current time to total seconds
        int seconds_start =
            btime.hour * 3600 + btime.min * 60 + btime.sec + p_odlozenyCas_set;
        // přes půlnoc - korekce?
        if (seconds_start > 24 * 60 * 60) {
            seconds_start = seconds_start - 24 * 60 * 60;
        }
        int hours_start = seconds_start / 3600;
        int minutes_start = (seconds_start % 3600) / 60;
        int secs_start = seconds_start % 60;
        snprintf(
            buffer, sizeof(buffer),
            "<span style='color:orange'>&nbsp;&nbsp;🕑&nbsp;Nastavte odložený "
            "start (▲ a ▼, nyní v %02d:%02d:%02d), potvrďte pomocí ON </span>",
            hours_start, minutes_start, secs_start);
        strcpy(tt[t_ind].text, buffer);
        tt[t_ind].priority = 10;  // default 10 - do not display anything else
    } else if (apst == APST_TO_CONFIRM && wifiapst == WIFIAPST_SET_RH) {
        // čeká se na potvrzení bez odložehého času
        strcpy(tt[t_ind].text,
               "<span style='color:orange'>&nbsp;&nbsp;⚠&nbsp;Nastavit typ RH "
               "sauny (▲ a ▼) a potvrdit tlačítkem ON (OFF - vypnout)</span>");
        tt[t_ind].priority = 10;  // !!! Může být ještě odložený start...
    } else if (apst == APST_TO_CONFIRM && !odlozenyNastaveno) {
        // čeká se na potvrzení bez odložehého času
        strcpy(tt[t_ind].text,
               "<span style='color:orange'>&nbsp;&nbsp;⚠&nbsp;Potvrdit start "
               "sauny "
               "tlačítkem ON (OFF - vypnout)</span>");
        tt[t_ind].priority = 10;  // !!! Může být ještě odložený start...
    } else if (apst == APST_ODLO) {
        // na hlavní jednotce probíhá nastavení odloženého času
        snprintf(buffer, sizeof(buffer),
                 "<span style='color:orange'>&nbsp;&nbsp;🕑&nbsp;Na hlavní "
                 "jednotce probíhá nastavení odloženého startu"
                 "</span>");
        strcpy(tt[t_ind].text, buffer);
        tt[t_ind].priority = 10;  // default 10 - do not display anything else
    } else if (apst == APST_TO_CONFIRM && !odlozenyNastaveno &&
               typSpusteni == 1 && wifiapst != WIFIAPST_SET_ODLO) {
        // na hlavní jednotce nebylo nastavení odloženého času, ale měl by se
        // nastavit na WiFi jednotce -> vstup do režimu nastavení odloženého
        // času ve WiFi

        // inicializuj režim odloženého času
        wifiapst = WIFIAPST_SET_ODLO;
        p_odlozenyCas_set = 0;
        //
    } else if (wifiapst == WIFIAPST_SET_TERM) {
        strcpy(
            tt[t_ind].text,
            "<span style='color:orange'>&nbsp;&nbsp;⚙&nbsp;▲ a ▼ - nastavení "
            "teploty, ON - potvrzení, OFF - zrušení</span>");
        tt[t_ind].priority = 9;  // allow to display other items
    } else if (wifiapst == WIFIAPST_SET_ODLO) {
        strcpy(
            tt[t_ind].text,
            "<span style='color:orange'>&nbsp;&nbsp;⚙&nbsp;▲ a ▼ - nastavení "
            "odloženého startu, ON - potvrzení, OFF - zrušení</span>");
        tt[t_ind].priority = 9;  // allow to display other items
    } else if (wifiapst == WIFIAPST_SET_RH) {
        strcpy(
            tt[t_ind].text,
            "<span style='color:orange'>&nbsp;&nbsp;⚙&nbsp;▲ a ▼ - nastavení "
            "typu RH sauny, ON - potvrzení, OFF - zrušení</span>");
        tt[t_ind].priority = 9;  // allow to display other items
    } else if (wifiapst == WIFIAPST_SET_TIMEOUT) {
        strcpy(
            tt[t_ind].text,
            "<span style='color:orange'>&nbsp;&nbsp;⚙&nbsp;▲ a ▼ - nastavení "
            "intervalu, ON - potvrzení, OFF - zrušení</span>");
        tt[t_ind].priority = 9;  // allow to display other items
    } else if (wifiapst == WIFIAPST_SET_GENERAL) {
        strcpy(tt[t_ind].text,
               "<span style='color:orange'>&nbsp;&nbsp;⚙&nbsp;&nbsp;Nastavení "
               "hodnot, beze změn e tlačítem SET</span>");
        tt[t_ind].priority = 9;  // allow to display other items
    }

    t_ind += 1;

    // doba běhu
    tt[t_ind].priority = 9;  // 9 - zobrazují se další údaje
    int hours = dobaBehu / 3600;
    int minutes = (dobaBehu % 3600) / 60;
    int secs = dobaBehu % 60;
    strcpy(tt[t_ind].text, "&nbsp;🔥&nbsp;&nbsp;");

    /*
     if (odlozenyNastaveno) {
     int hours_o = p_odlozenyCas / 3600;
     int minutes_o = (p_odlozenyCas % 3600) / 60;
     int secs_o = p_odlozenyCas % 60;
     snprintf(buffer, sizeof(buffer),
     "Odložený start: %dh %dm %ds, poté doba běhu: %dh %dm %ds",
     hours, minutes, secs, hours_o, minutes_o, secs_o);
     strcat(t[t_ind].text, buffer);
     } else {
     */

    // Convert the current time to total seconds
    int seconds_to = btime.hour * 3600 + btime.min * 60 + btime.sec + dobaBehu;
    // přes půlnoc?
    if (seconds_to > 24 * 60 * 60) {
        seconds_to = seconds_to - 24 * 60 * 60;
    }
    int hours_to = seconds_to / 3600;
    int minutes_to = (seconds_to % 3600) / 60;
    int secs_to = seconds_to % 60;

    snprintf(ins, sizeof(ins),
             "Sauna zapnuta, běh do %02d:%02d:%02d (%dh %dm %ds)", hours_to,
             minutes_to, secs_to, hours, minutes, secs);
    strcat(tt[t_ind].text, ins);

    t_ind += 1;

    // t_rele
    tt[t_ind].priority = 9;
    // ESP_LOGI(TAG, "StavRelat %x", stavRelat);
    strcpy(buffer, "&nbsp;&nbsp;♨&nbsp;️&nbsp;&nbsp;L321: ");
    (stavRelat & 0b00000001)
        ? strcat(buffer, "<span style='color:red'>♨</span>")
        : strcat(buffer, "-");
    (stavRelat & 0b00000010)
        ? strcat(buffer, "<span style='color:red'>♨</span>")
        : strcat(buffer, "-");
    (stavRelat & 0b00001000)
        ? strcat(buffer, "<span style='color:red'>♨</span>")
        : strcat(buffer, "-");
    (stavRelat & 0b00000100) ? strcat(buffer, "&nbsp;&nbsp;světlo: 💡")
                             : strcat(buffer, "&nbsp;&nbsp;světlo: -");

    strcpy(tt[t_ind].text, buffer);

    // debug
    // ESP_LOGI(TAG, "xxx %s", buffer);

    t_ind += 1;

    // t_nastavenatemp
    tt[t_ind].priority = 8;
    snprintf(buffer, sizeof(buffer),
             "&nbsp;&nbsp;🌡&nbsp;&nbsp;Nastavená teplota: %d°C",
             nastavenaTemp);
    strcpy(tt[t_ind].text, buffer);
    // stav termostatu
    if (termState == 1)
        strcpy(buffer,
               "&nbsp;&nbsp;<span style='color:red'>&#9888; zkrat "
               "termostatu</span>");  // ⚠
    else if (termState == 2)
        strcpy(buffer,
               "&nbsp;&nbsp;<span style='color:red'>&#9888; termostat "
               "odpojen</span>");  // ⚠
    else
        buffer[0] = '\0';
    strcat(tt[t_ind].text, buffer);  // empty
                                    // stav tepelné pojistky
    if (pojTepState == 1)
        strcpy(buffer,
               "&nbsp;&nbsp;<span style='color:red'>&#9888; tepelná "
               "pojistka</span>");  // ⚠
    else
        buffer[0] = '\0';
    strcat(tt[t_ind].text, buffer);  // empty

    t_ind += 1;
    // t_dvere
    if (doorOpen) {
        tt[t_ind].priority = 9;
        int hours = doorTout / 3600;
        int minutes = (doorTout % 3600) / 60;
        int secs = doorTout % 60;
        snprintf(buffer, sizeof(buffer),
                 "&#128682;&nbsp;&nbsp;Dveře otevřené %dh %dm %ds", hours,
                 minutes,  // 🚪
                 secs);
        strcpy(tt[t_ind].text, buffer);
    }

    t_ind += 1;
    // t_rhcidlo
    if (typSauny >= 3) {
        tt[t_ind].priority = 9;
        snprintf(buffer, sizeof(buffer), "&nbsp;&nbsp;&#127778;&nbsp;&nbsp;%s",
                 typy_saun_long_graph[typSauny]);
        strcpy(tt[t_ind].text, buffer);
    }

    t_ind += 1;

    // t_para_esence
    if (parniEsence && typSauny == TYP_SAUNY_PARNI) {
        tt[t_ind].priority = 9;
        snprintf(buffer, sizeof(buffer),
                 "&nbsp;&#127807;&nbsp;&nbsp;Stav pára/esence: ");  // 🌿
        (esenceState) ? strcat(buffer, "&#11044;")
                      : strcat(buffer, "&#9711;");  // ⬤ ◯
        strcpy(tt[t_ind].text, buffer);
        snprintf(buffer, sizeof(buffer), "&nbsp;&nbsp;&nbsp;interval: %d min.",
                 intervalEsence);
        strcat(tt[t_ind].text, buffer);
    }

    ////////////////////////////////////
    // t položky - (a) zpracuj neprázdné
    int i = 0;
    int j = 1;
    while (j <= TEXTITEMS_COUNT) {
        if (tt[i].priority) {
            // ESP_LOGI(TAG,"t%d delka: %d",i, strlen(t[i].text));
            snprintf(buffer, sizeof(buffer), "\"t_%d\":\"%s\"", j, tt[i].text);
            strcat(data, buffer);
            strcat(data, ",");
            j += 1;
        }
        if ((tt[i].priority == 10) || (i + 1 == TEXTITEMS_COUNT))
            break;  // extraordinary - do not display other rows or all
                    // textitems processed
        i += 1;
    }
    // add empty rows to JSON
    for (int k = j; k <= TEXTITEMS_COUNT; k++) {
        snprintf(buffer, sizeof(buffer), "\"t_%d\":\"&nbsp;\"", k);
        strcat(data, buffer);
        strcat(data, ",");
    }

    // rfp - request to refresh webpage
    snprintf(buffer, sizeof(buffer), "\"rfp\":%d", rfp);
    strcat(data, buffer);
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

esp_err_t button_up_handler(httpd_req_t *req) {
    // authentication verification part /////////
    char cookie_value[128];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) ||
        strcmp(cookie_value, "1") != 0 || current_user_id == -1) {
        // Redirect to login if not authenticated
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    if (wifiapst == WIFIAPST_SET_RH) {
        if (p_rh_set < TYP_SAUNY_RHPARA) {
            p_rh_set += 1;
        }
    } else if (wifiapst == WIFIAPST_SET_ODLO) {
        if (p_odlozenyCas_set < MAX_ODLO_CAS) {
            p_odlozenyCas_set += 15 * 60;
        }
    } else if (wifiapst == WIFIAPST_SET_TERM) {
        // increase temperature
        if (nastavenaTemp_set < defMaxTemp) {
            nastavenaTemp_set += 1;
        }
    } else if (wifiapst == WIFIAPST_SET_TIMEOUT) {
        // dummy
        if (dobaBehu_set < MAX_DOBA_BEHU) {
            dobaBehu_set += (15 * 60);
        }
    } else if (wifiapst == WIFIAPST_NORMAL && apst != APST_TO_CONFIRM &&
               apst != APST_STARTING) {
        wifiapst = WIFIAPST_SET_TERM;
        nastavenaTemp_set = nastavenaTemp;
    }

    // ESP_LOGI(TAG, "Button up pressed");
    httpd_resp_send(req, NULL, 0);  // Send empty response
    return ESP_OK;
}

esp_err_t button_down_handler(httpd_req_t *req) {
    // authentication verification part /////////
    char cookie_value[128];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) ||
        strcmp(cookie_value, "1") != 0 || current_user_id == -1) {
        // Redirect to login if not authenticated
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    if (wifiapst == WIFIAPST_SET_RH) {
        if (p_rh_set > TYP_SAUNY_RHSUCHA) {
            p_rh_set -= 1;
        }
    } else if (wifiapst == WIFIAPST_SET_ODLO) {
        if (p_odlozenyCas_set > 0) {
            p_odlozenyCas_set -= 15 * 60;
        }
    } else if (wifiapst == WIFIAPST_SET_TERM) {
        // increase temperature
        if (nastavenaTemp_set > defMinTemp) {
            nastavenaTemp_set -= 1;
        }
    } else if (wifiapst == WIFIAPST_SET_TIMEOUT) {
        // dummy
        if (dobaBehu_set > 0) {
            dobaBehu_set -= (15 * 60);
        }
    } else if (wifiapst == WIFIAPST_NORMAL && apst != APST_TO_CONFIRM &&
               apst != APST_STARTING) {
        wifiapst = WIFIAPST_SET_TIMEOUT;
        dobaBehu_set = ((dobaBehu + (15 * 60) / 2) / (15 * 60)) *
                       (15 * 60);  // round to 15 minutes
    }
    // ESP_LOGI(TAG, "Button down pressed");
    httpd_resp_send(req, NULL, 0);  // Send empty response
    return ESP_OK;
}

esp_err_t button_set_handler(httpd_req_t *req) {
    if (wifiapst == WIFIAPST_NORMAL && state2 != 0) {
        // pobíhá nějaké nastavování jinde

        // žádná akce

    } else if (wifiapst == WIFIAPST_SET_TERM) {
        // nastavenaTemp = nastavenaTemp_set;
        if (nastavenaTemp != nastavenaTemp_set) {
            mainset_temp = nastavenaTemp_set;
        }
        wifiapst = WIFIAPST_NORMAL;
    } else if (wifiapst == WIFIAPST_SET_TIMEOUT) {
        // dobaBehu = dobaBehu_set;
        if (dobaBehu != dobaBehu_set) {
            mainset_dobabehu = dobaBehu_set;
        }
        wifiapst = WIFIAPST_NORMAL;
    } else if (wifiapst == WIFIAPST_SET_GENERAL) {
        // no changes, back from Setup
        wifiapst = WIFIAPST_NORMAL;
        rfp = 1;  // set page refresh request
    } else if (wifiapst == WIFIAPST_NORMAL) {
        wifiapst = WIFIAPST_SET_GENERAL;
        rfp = 1;  // set page refresh request
    }

    // empty response
    httpd_resp_send(req, NULL, 0);  // Send empty response
    return ESP_OK;
}

esp_err_t button_light_handler(httpd_req_t *req) {
    // authentication verification part /////////
    char cookie_value[128];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) ||
        strcmp(cookie_value, "1") != 0 || current_user_id == -1) {
        // Redirect to login if not authenticated
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // request change of light
    if (stavRelat & 0b00000100) {
        // svítí, zhasni
        mainset_lightoff = 1;
    } else {
        // nesvítí, rozsviť
        mainset_lighton = 1;
    }

    // ESP_LOGI(TAG, "Button light pressed");
    httpd_resp_send(req, NULL, 0);  // Send empty response
    return ESP_OK;
}

esp_err_t button_on_handler(httpd_req_t *req) {
    ////////// verify authentication start /////////
    char cookie_value[20];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) ||
        strcmp(cookie_value, "1") || (current_user_id == -1) != 0) {
        // Redirect to login if not authenticated
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    ////////// verify authentication end /////////

    if (wifiapst == WIFIAPST_SET_TIMEOUT) {
        // dobaBehu = dobaBehu_set;
        if (dobaBehu != dobaBehu_set) {
            mainset_dobabehu = dobaBehu_set;
        }
        wifiapst = WIFIAPST_NORMAL;
    } else if (wifiapst == WIFIAPST_SET_TERM) {
        // nastavenaTemp = nastavenaTemp_set;
        if (nastavenaTemp != nastavenaTemp_set) {
            mainset_temp = nastavenaTemp_set;
        }
        wifiapst = WIFIAPST_NORMAL;
    } else if (apst == APST_OFF) {
        // pokud vypnuto, zapni
        mainset_on = 1;
    } else if (apst == APST_TO_CONFIRM && typSpusteni == 0 &&
               typSauny < TYP_SAUNY_RHSUCHA) {
        // potvrzení spuštění bez odloženého času a RH
        mainset_hoton = 1;
    } else if (wifiapst == WIFIAPST_SET_RH) {
        // výstup z nastavování rh, poslání typu sauny přes setup větu
        // konec reportování, že jsem v rh setupu

        // připrava dat pro setup větu
        korekce_cidla_buf = setup_korekceCidla;
        nastavenikamen_buf = setup_nastaveniKamen;
        typSauny_buf = p_rh_set;
        stridanifazi_buf = setup_stridaniFazi;
        blokovaniSvetla_buf = setup_blokovaniSvetla;
        dalkoveOvladani_buf = setup_dalkoveOvladani;
        parniEsence_buf = setup_parniEsence;
        intervalEsence_buf = setup_intervalEsence;
        typspusteni_sauny_buf = setup_typSpusteni;

        // indikace požadavku na poslání setupu
        mainset_sendsetup = 1;

        // pokračování - do normálního stavu nebo odloženého času
        if (typSpusteni == 1 && apst == APST_TO_CONFIRM) {
            wifiapst = WIFIAPST_SET_ODLO;
        } else {
            wifiapst = WIFIAPST_NORMAL;
            // odstartovat saunu ihned
            mainset_hoton = 1;
        }
    } else if (apst == APST_TO_CONFIRM && typSauny >= TYP_SAUNY_RHSUCHA &&
               wifiapst != WIFIAPST_SET_ODLO) {
        // vstup do nastavování rn
        wifiapst = WIFIAPST_SET_RH;

        // rh_set je pracovní položka pro nastavování typu sauny
        p_rh_set = typSauny;
    } else if ((apst == APST_TO_CONFIRM && typSpusteni == 1) &&
               wifiapst != WIFIAPST_SET_ODLO && odlozenyNastaveno == 0) {
        // potvrzení s odloženým časem, pokud již není nastavován -
        // inicializace nastavení odloženého času na WiFi
        wifiapst = WIFIAPST_SET_ODLO;
        p_odlozenyCas_set = 0;
    } else if (apst == APST_TO_CONFIRM && hotRUN == 0 &&
               wifiapst != WIFIAPST_SET_ODLO) {
        // potvrzení v průběhu běžícího odloženého času
        mainset_hoton = 1;
    } else if (wifiapst == WIFIAPST_SET_ODLO) {
        // konec režimu odloženého času
        wifiapst = WIFIAPST_NORMAL;
        // požadavek na odeslání odloženého času do hlavní jednotky (pokud
        // je nenulový)
        //!!!
        // ESP_LOGW(TAG, "mainset_odlocas: %ld", p_odlozenyCas_set);
        mainset_odlocas =
            p_odlozenyCas_set + 1;  // dummy hodnota +1s odloženého času
                                    // kvůli zpracování nulové hodnoty

        // zrušení reportování nastavování odloženého času
    } else {
    }
    // ESP_LOGI(TAG, "Button ON pressed");
    httpd_resp_send(req, NULL, 0);  // Send empty response
    return ESP_OK;
}

esp_err_t button_off_handler(httpd_req_t *req) {
    // authentication verification part /////////
    char cookie_value[128];
    if (!get_cookie(req, "auth", cookie_value, sizeof(cookie_value)) ||
        strcmp(cookie_value, "1") != 0 || current_user_id == -1) {
        // Redirect to login if not authenticated
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // zruš odečet pomocí tlačítek
    if (wifiapst == WIFIAPST_SET_TERM || wifiapst == WIFIAPST_SET_TIMEOUT ||
        wifiapst == WIFIAPST_SET_ODLO || wifiapst == WIFIAPST_SET_RH) {
        wifiapst = WIFIAPST_NORMAL;

        // v případě nastavování odloženého času a RH možnost vypnutí
        if (wifiapst == WIFIAPST_SET_RH || wifiapst == WIFIAPST_SET_ODLO) {
            wifiapst = WIFIAPST_NORMAL;
            mainset_off = 1;
            rfp = 1;  // set page refresh request
        }

    } else if (wifiapst == WIFIAPST_SET_GENERAL) {
        // ukončení general setup pomocí OFF tlačítka
        // ESP_LOGI(TAG, "Setting rfp to 1: _handlers, after // ukončení
        // general setup pomocí OFF tlačítka; ");
        rfp = 1;  // set page refresh request
        wifiapst = WIFIAPST_NORMAL;
    } else {
        // send OFF
        mainset_off = 1;
        wifiapst = WIFIAPST_NORMAL;
        rfp = 1;  // set page refresh request
    }

    // ESP_LOGI(TAG, "Button OFF pressed");
    httpd_resp_send(req, NULL, 0);  // Send empty response
    return ESP_OK;

    // Redirect to the root page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// helper used from javascript to reset page refresh
esp_err_t resetrfp_get_handler(httpd_req_t *req) {
    /*
    ESP_LOGI(TAG,
             "Setting rfp to 0: _handlers, in function
    resetrfp_get_handler");
    */
    rfp = 0;  // reset refresh page requirement
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);  // Send empty response
    return ESP_OK;
}

// set handlers

// favicon handler
esp_err_t favicon_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico, sizeof(favicon_ico));
    return ESP_OK;
}

// potvrzení změn po ukončení WIFIAPST_SET_GENERAL
esp_err_t set_values_post_handler(httpd_req_t *req) {
    char buf[MAX_RETS_LENGTH];  // Adjust buffer size as needed
    int ret, remaining = req->content_len;

    memset(buf, 0, sizeof(buf));
    // Read the data from the request
    if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "prijaty buffer:%s", buf);


    char *query_copy = strdup(buf);
    char *token = strtok(query_copy, "&");

    while (token != NULL) {
        // Use sscanf to parse if it matches a known "key=value" pattern
        if (sscanf(token, "temp_set=%hhu", &nastavenaTemp_ret)) {
        } else if (sscanf(token, "doba_behu_set=%lu", &dobaBehu_ret)) {
        } else if (sscanf(token, "hodina_set=%hhd", &hours_ret)) {
        } else if (sscanf(token, "minuta_set=%hhd", &minutes_ret)) {
        } else if (sscanf(token, "den_set=%hhu", &date_ret)) {
        } else if (sscanf(token, "mesic_set=%hhu", &month_ret)) {
        } else if (sscanf(token, "rok_set=%hu", &year_ret)) {
        } else if (sscanf(token, "datcas_typ_set=%hhu", &datacas_typ_ret)) {
        } else if (sscanf(token, "korekce_c_set=%hhd", &korekce_cidla_ret)) {
        } else if (sscanf(token, "parniesence_set=%hhu", &parniEsence_ret)) {
        } else if (sscanf(token, "dalkove_ovladani_set=%hhu",
                          &dalkoveOvladani_ret)) {
        } else if (sscanf(token, "blokovani_svetla_set=%hhu",
                          &blokovaniSvetla_ret)) {
        } else if (sscanf(token, "typ_sauny_set=%hhu", &typSauny_ret)) {
        } else if (sscanf(token, "typspusteni_sauny_set=%hhu",
                          &typspusteni_sauny_ret)) {
        } else if (sscanf(token, "stridanifazi_set=%hhu", &stridanifazi_ret)) {
        } else if (sscanf(token, "interval_esence_set=%hhu",
                          &intervalEsence_ret)) {
        } else if (sscanf(token, "pwd_0=%32s", password_sauna_ret)) {
        } else if (sscanf(token, "pwd_1=%32s", password_servis_ret)) {
        }

        // Move to the next key-value pair
        token = strtok(NULL, "&");
    }

    free(query_copy);  // Free the allocated memory for the query string copy

    ESP_LOGI(TAG, "Parsed values:");
    ESP_LOGI(TAG, "nastavenaTemp_ret: %d", nastavenaTemp_ret);
    ESP_LOGI(TAG, "dobaBehu_ret: %lu", dobaBehu_ret);
    ESP_LOGI(TAG, "hours_ret: %d", hours_ret);
    ESP_LOGI(TAG, "minutes_ret: %d", minutes_ret);
    ESP_LOGI(TAG, "date_ret: %d", date_ret);
    ESP_LOGI(TAG, "month_ret: %d", month_ret);
    ESP_LOGI(TAG, "year_ret: %d", year_ret);
    ESP_LOGI(TAG, "datacas_typ_ret: %d", datacas_typ_ret);
    ESP_LOGI(TAG, "korekce_cidla_ret: %d", korekce_cidla_ret);
    ESP_LOGI(TAG, "parniEsence_ret: %d", parniEsence_ret);
    ESP_LOGI(TAG, "dalkoveOvladani_ret: %d", dalkoveOvladani_ret);
    ESP_LOGI(TAG, "typSauny_ret: %d", typSauny_ret);
    ESP_LOGI(TAG, "rezim_sauny_ret: %d", typspusteni_sauny_ret);
    ESP_LOGI(TAG, "stridaniFazi_ret: %d", stridanifazi_ret);
    ESP_LOGI(TAG, "intervalEsence_ret: %d", intervalEsence_ret);
    ESP_LOGI(TAG, "blokovaniSvetla_ret: %d", blokovaniSvetla_ret);
    ESP_LOGI(TAG, "password_sauna_ret: %s (delka: %d)", password_sauna_ret,
             strlen(password_sauna_ret));
    ESP_LOGI(TAG, "password_servis_ret: %s (delka: %d)", password_servis_ret,
             strlen(password_servis_ret));

    // ověř změněné hodnoty a pokud jsou, inicializuj přenos do hlavní
    // jednotky
    if (nastavenaTemp_ret != nastavenaTemp) {
        mainset_temp = nastavenaTemp_ret;
        nastavenaTemp = nastavenaTemp_ret;
    }

    if (dobaBehu_ret != dobaBehu) {
        // round to 5 minutes
        mainset_dobabehu = ((dobaBehu_ret + 150) / 300) * 300;
        dobaBehu = mainset_dobabehu;
    }

    // nastavení času
    if (datacas_typ_ret) {
        // ověř, jestli se synchronizuje čas z internetu
        if (datacas_typ_ret == DATCAS_INTERNET &&
            timeinfo_sntp.tm_year > (2023 - 1900)) {
            // copy to avoid atomicity break
            timeinfo_transfer = timeinfo_sntp;
            hours_ret = timeinfo_transfer.tm_hour;
            minutes_ret = timeinfo_transfer.tm_min;
            date_ret = timeinfo_transfer.tm_mday;
            month_ret = timeinfo_transfer.tm_mon + 1;
            year_ret = timeinfo_transfer.tm_year + 1900;
        }

        // initiate synchrotime transfer
        mainset_synchrocas = 1;

        hours_preps = hours_ret;
        minutes_preps = minutes_ret;
        date_preps = date_ret;
        month_preps = month_ret;
        year_preps = year_ret;
    }

    // ověření, zda se změnilo něco v "setup_" položkách
    if (setup_korekceCidla != korekce_cidla_ret ||
        setup_dalkoveOvladani != dalkoveOvladani_ret ||
        setup_typSauny != typSauny_ret ||
        setup_stridaniFazi != stridanifazi_ret ||
        setup_typSpusteni != typspusteni_sauny_ret ||
        setup_nastaveniKamen != nastavenikamen_ret ||
        setup_blokovaniSvetla != blokovaniSvetla_ret ||
        setup_parniEsence != parniEsence_ret ||
        setup_intervalEsence != intervalEsence_ret) {
        // zkopíruj _ret hodnoty do _buf položek
        korekce_cidla_buf = korekce_cidla_ret;
        nastavenikamen_buf = nastavenikamen_ret;
        typSauny_buf = typSauny_ret;
        stridanifazi_buf = stridanifazi_ret;
        blokovaniSvetla_buf = blokovaniSvetla_ret;
        dalkoveOvladani_buf = dalkoveOvladani_ret;
        parniEsence_buf = parniEsence_ret;
        intervalEsence_buf = intervalEsence_ret;
        typspusteni_sauny_buf = typspusteni_sauny_ret;

        // vyvolej přenos setup položek do hlavní jednotky
        mainset_sendsetup = 1;
    }



    // změna hesel, pokud jsou zadaná
    esp_err_t err;

    if (strlen(password_sauna_ret) > 0) {
        err = nvs_set_str(nvs_handle_storage, "password_sauna",
                          password_sauna_ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Chyba pri zapisu hesla uzivatele sauna do nvs");
        } else {
            strcpy(users[0].password, password_sauna_ret);
            ESP_LOGW(TAG,
                     "Nove heslo pro uzivatele sauna zapsano do nvs a zmeneno "
                     "v aplikaci");
        }
    }

    if (strlen(password_servis_ret) > 0) {
        err = nvs_set_str(nvs_handle_storage, "password_servis",
                          password_servis_ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Chyba pri zapisu hesla pro uzivatele servis do nvs");
        } else {
            strcpy(users[1].password, password_servis_ret);
            ESP_LOGW(TAG,
                     "Nove heslo pro uzivatele servis zapsano do nvs a zmeneno "
                     "v aplikaci");
        }
    }

    if (strlen(password_sauna_ret) + strlen(password_servis_ret) > 0) {
        if (nvs_commit(nvs_handle_storage) == ESP_OK) {
            ESP_LOGI(TAG, "Zmena hesel v nvs probehla v poradku");
            ;
        } else {
            ESP_LOGE(TAG, "Chyba pri inicializaci hesel v nvs");
            ;
        }
    }

    // go out from SET
    wifiapst = WIFIAPST_NORMAL;

    // Redirect back to the root page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
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

        httpd_uri_t resetrfp_uri = {.uri = "/resetrfp",
                                    .method = HTTP_GET,
                                    .handler = resetrfp_get_handler,
                                    .user_ctx = NULL};
        httpd_register_uri_handler(server, &resetrfp_uri);

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

        httpd_uri_t button_up_uri = {.uri = "/button_up",
                                     .method = HTTP_GET,
                                     .handler = button_up_handler,
                                     .user_ctx = NULL};
        httpd_register_uri_handler(server, &button_up_uri);

        httpd_uri_t image_uri = {.uri = "/logo",
                                 .method = HTTP_GET,
                                 .handler = logo_image_get_handler,
                                 .user_ctx = NULL};
        httpd_register_uri_handler(server, &image_uri);

        httpd_uri_t button_down_uri = {.uri = "/button_down",
                                       .method = HTTP_GET,
                                       .handler = button_down_handler,
                                       .user_ctx = NULL};
        httpd_register_uri_handler(server, &button_down_uri);

        httpd_uri_t button_light_uri = {.uri = "/button_light",
                                        .method = HTTP_GET,
                                        .handler = button_light_handler,
                                        .user_ctx = NULL};
        httpd_register_uri_handler(server, &button_light_uri);

        httpd_uri_t button_set_uri = {.uri = "/button_set",
                                      .method = HTTP_GET,
                                      .handler = button_set_handler,
                                      .user_ctx = NULL};
        httpd_register_uri_handler(server, &button_set_uri);

        httpd_uri_t button_on_uri = {.uri = "/button_on",
                                     .method = HTTP_GET,
                                     .handler = button_on_handler,
                                     .user_ctx = NULL};
        httpd_register_uri_handler(server, &button_on_uri);

        httpd_uri_t button_off_uri = {.uri = "/button_off",
                                      .method = HTTP_GET,
                                      .handler = button_off_handler,
                                      .user_ctx = NULL};
        httpd_register_uri_handler(server, &button_off_uri);
        //
        httpd_uri_t set_values_uri = {.uri = "/set_values",
                                      .method = HTTP_POST,
                                      .handler = set_values_post_handler,
                                      .user_ctx = NULL};
        httpd_register_uri_handler(server, &set_values_uri);

        // favicon handler
        httpd_uri_t favicon_uri = {.uri = "/favicon.ico",
                                   .method = HTTP_GET,
                                   .handler = favicon_get_handler,
                                   .user_ctx = NULL};
        httpd_register_uri_handler(server, &favicon_uri);
    }
    return server;
}
