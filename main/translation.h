/*
 * translation.h
 */



#include <stddef.h>
#include <stdint.h>
#include <string.h>


typedef enum { LANG_CZ = 0, LANG_EN, LANG_COUNT } LANGUAGE;

// Active language (default Czech)
// static uint8_t gst_lang = LANG_CZ;
static uint8_t gst_lang = LANG_CZ;

// translation table
typedef struct {
    char cz[MAX_TRANS_STRING_CZ_LEN];
    char en[MAX_TRANS_STRING_EN_LEN];
} translation;

static const translation cz_en[] = {{"Vypnuto", "Off"},
                                    {"Zapnuto", "On"},
                                    {"Odhlásit", "Logoff"},
                                    {"Přihlásit", "Login"},
                                    {"Neúspěšné přihlášení", "Unsuccessful login"},
                                    {"Automato - přihlášení obsluhy", "Automato - login"},
                                    {"Uživatelské jméno", "Username"},
                                    {"Heslo", "Password"},
                                    {"Automato", "Automato"},
                                    {"automato", "automato"},
                                    {"Automato - přihlášení obsluhy", "Automato - operator login"},
                                    {"Automato - přihlášení", "Automato - login"},
                                    {"Provoz", "Operations"},
                                    {"Denní trh", "Daily market"},
                                    {"Nastavení", "Settings"},
                                    {"Pravidla", "Rules"},
                                    {"Ceny", "Prices"},
                                    {"Aktivované akce", "Activated actions"},
                                    {"čas nenastaven", "time not set"},
                                    {"Nastav", "Setup"},
                                    {"Uloženo", "Saved"},
                                    {"Chyba", "Error"},
                                    {"Povoleno", "Enabled"},
                                    {"Nová podmínka", "New condition"},
                                    {"Operátor", "Operator"},
                                    {"Zpět", "Back"},
                                    {"Odstranit", "Remove"},
                                    {"Levá strana", "Left side"},
                                    {"Pravá strana", "Right side"},
                                    {"Operátor", "Operator"},
                                    {"Podmínka", "Condition"},
                                    {"Nastavení podmínek", "Conditions setting"},
                                    {"Potvrzení", "Confirm"},
                                    {"Nastavení", "Setup"},
                                    {"Akce", "Actions"},
                                    {"po", "Mon"},
                                    {"út", "Tue"},
                                    {"st", "Wed"},
                                    {"čt", "Thu"},
                                    {"pá", "Fri"},
                                    {"so", "Sat"},
                                    {"ne", "Sun"},
                                    {"Ceny OTE", "Prices OTE"},
                                    {"Nové heslo uživatele automato:","New password for automato user:"},
                                    {"Nové heslo uživatele servis:","New password for servis user:"},
                                    {"Výchozí nastavení (!)","Default settings (!)"},
                                    {"Pozor, stiskem tlačítka níže změníte všechna nastavení na výchozí a restartujete zařízení (nutno znovu připojit na wifi)","Caution, if you press the button below, all settings will be initiated to default and the device will be restarted. WiFi provisioning needed again."},
                                    {"Změny potvrďte stiskem tlačítka 'Potvrzení'","Confirm any change pressing the button 'Confirm'"},
                                    {"Zpět","Back"},
                                    {"Uživatelská nastavení","User settings"},
                                    {"Akce&nbsp;&rarr;&nbsp;naposledy aktivováno","Action&nbsp;&rarr;&nbsp;last activation timestamp"},
                                    {"<html><body>Výchozí nastavení, restartuji zařízení (po restartu připojte na WiFi)</body></html>","<html><body>Default settings, restarting the device (after restart, do WiFi provisioning)</body></html>"},
                                   

                                    /* sentinel – signals end of table */
                                    {"", ""}};


// translation function
// returns English text for the given Czech text
// or the original text if not found in the table
const char *t(const char *instr) {
    /* Nothing to do for Czech – return original text */
    if (gst_lang == LANG_CZ) return instr;

    /* Sequential search in the table (tiny, so fine) */
    for (size_t i = 0; cz_en[i].cz[0] != '\0'; ++i) {
        if (strcmp(instr, cz_en[i].cz) == 0) return cz_en[i].en; /* match found */
    }

    return instr; /* not found → fall back to original */
}
