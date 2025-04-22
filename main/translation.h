#include <stddef.h>
#include <stdint.h>
#include <string.h>


typedef enum { LANG_CZ = 0, LANG_EN, LANG_COUNT } LANGUAGE;

// Active language (default Czech)
// static uint8_t gst_lang = LANG_CZ;
static uint8_t gst_lang = LANG_EN;

// translation table
typedef struct {
    char cz[MAX_TRANS_STRING_CZ_LEN];
    char en[MAX_TRANS_STRING_EN_LEN];
} translation;

static const translation cz_en[] = {
    {"Vypnuto", "Off"},
    {"Zapnuto", "On"},
    {"Odhlásit", "Logoff"},
    {"Přihlásit", "Login"},
    {"Neúspěšné přihlášení", "Unsuccessful login"},
    {"Automato - přihlášení obsluhy", "Automato - login"},
    {"Uživatelské jméno", "Username"},
    {"Heslo", "Password"},
    {"Automato", "Automato"},
    {"Automato - přihlášení obsluhy", "Automato - operator login"},
    {"Automato - přihlášení", "Automato - login"},
    {"Provoz", "Operation"},
    {"Denní trh", "Daily market"},
    {"Nastavení", "Settings"},
    {"Pravidla", "Rules"},

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
        if (strcmp(instr, cz_en[i].cz) == 0)
            return cz_en[i].en; /* match found */
    }

    return instr; /* not found → fall back to original */
}
