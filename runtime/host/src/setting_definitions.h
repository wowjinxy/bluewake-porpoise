// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_SETTING_DEFINITIONS_H
#define BLUEWAKE_SETTING_DEFINITIONS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum BwSettingType {
    BW_SETTING_BOOL, BW_SETTING_INT, BW_SETTING_REAL, BW_SETTING_CHOICE
} BwSettingType;

typedef enum BwSettingPage {
    BW_PAGE_DISPLAY, BW_PAGE_CONTROLS, BW_PAGE_ENHANCEMENTS, BW_PAGE_MODS,
    BW_PAGE_NETWORK, BW_PAGE_SOUND_SAVES, BW_PAGE_DEVELOPER, BW_PAGE_HUD, BW_PAGE_COUNT
} BwSettingPage;

typedef enum BwSettingApply { BW_SETTING_LIVE, BW_SETTING_RESTART } BwSettingApply;
enum { BW_SETTING_HIDDEN = 1u, BW_SETTING_NO_PRESET = 2u, BW_SETTING_SESSION_ONLY = 4u };

typedef struct BwSettingChoice { const char* value; const char* label; } BwSettingChoice;

// Common metadata only: this interface does not depend on a platform Settings
// object, SDL, the running game, or environment variables. IDs are stable keys.
typedef struct BwSettingDefinition {
    const char* id;
    const char* label;
    const char* help;
    BwSettingType type;
    BwSettingPage page;
    const char* default_value;
    double minimum, maximum;
    const BwSettingChoice* choices;
    size_t choice_count;
    BwSettingApply apply;
    const char* dependency_id;
    const char* dependency_value;
    unsigned flags;
    const char* option_name;  // BetterWW option name, or NULL.
} BwSettingDefinition;

const BwSettingDefinition* bw_setting_definitions(size_t* count);
const BwSettingDefinition* bw_setting_find(const char* id);
const char* bw_setting_page_name(BwSettingPage page);
const char* bw_setting_page_id(BwSettingPage page);
uint32_t bw_setting_all_sections(void);

#ifdef __cplusplus
}
#endif
#endif
