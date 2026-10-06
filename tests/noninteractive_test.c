#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* values[9];
static const char* const names[] = {
    "BLUEWAKE_RENDERER", "BLUEWAKE_DATA_DIR", "BLUEWAKE_CARD_PATH",
    "BLUEWAKE_SRAM", "BLUEWAKE_STATE_DIR", "DOL_AURORA_CACHE_DIR",
    "BLUEWAKE_LIVE_PAD", "BLUEWAKE_NO_DIALOG", "BLUEWAKE_SETTINGS",
};
static const char* test_getenv(const char* name) {
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i)
        if (strcmp(name, names[i]) == 0) return values[i];
    return NULL;
}
#define getenv test_getenv
#include "noninteractive.h"
#undef getenv

static unsigned checks;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    fprintf(stderr, "line %u failed: %s\n", __LINE__, #condition); return 1; \
} } while (0)

int main(void) {
    const char* modes[] = {NULL, "", "aurora", "headless", "aurora-noninteractive-typo", "AURORA-NONINTERACTIVE"};
    for (size_t i = 0; i < sizeof modes / sizeof modes[0]; ++i) {
        values[0] = modes[i]; CHECK(!bluewake_noninteractive_requested());
    }
    values[0] = "aurora-noninteractive";
    CHECK(bluewake_noninteractive_requested());
    for (size_t i = 1; i <= 5; ++i) values[i] = "disposable test path";
    values[6] = "0"; values[7] = "1"; values[8] = "none";
    CHECK(bluewake_noninteractive_error() == NULL);
    for (size_t i = 1; i <= 5; ++i) {
        values[i] = NULL; CHECK(strcmp(bluewake_noninteractive_error(), names[i]) == 0);
        values[i] = ""; CHECK(strcmp(bluewake_noninteractive_error(), names[i]) == 0);
        values[i] = "disposable test path";
    }
    const char* incorrect[] = {NULL, "", "1", "00", "0disabled"};
    for (size_t i = 0; i < sizeof incorrect / sizeof incorrect[0]; ++i) {
        values[6] = incorrect[i]; CHECK(strcmp(bluewake_noninteractive_error(), "BLUEWAKE_LIVE_PAD=0") == 0);
    }
    values[6] = "0";
    values[7] = "0"; CHECK(strcmp(bluewake_noninteractive_error(), "BLUEWAKE_NO_DIALOG=1") == 0);
    values[7] = NULL; CHECK(strcmp(bluewake_noninteractive_error(), "BLUEWAKE_NO_DIALOG=1") == 0);
    values[7] = "1";
    values[8] = "player settings.ini"; CHECK(strcmp(bluewake_noninteractive_error(), "BLUEWAKE_SETTINGS=none") == 0);
    values[8] = NULL; CHECK(strcmp(bluewake_noninteractive_error(), "BLUEWAKE_SETTINGS=none") == 0);
    values[8] = "none"; CHECK(bluewake_noninteractive_error() == NULL);
    printf("noninteractive launch policy: %u checks passed\n", checks);
    return 0;
}
