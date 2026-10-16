// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit diagnostic observations only. No native pointer or mutable view escapes.
#pragma once
#include <stdint.h>

typedef struct PcSaveUiObservation {
    bool window_present, collection_present, save_present, buffer_valid;
    uint8_t window_proc, collection_mode, cursor, save_status, proc, choice;
    bool retry_read_armed, existing_read_required, recollection_exchanged;
    uint64_t save_attempt, completed_reads;
    int32_t last_save_result, last_read_result;
    uint8_t buffered_slots[5712];
} PcSaveUiObservation;

// Game-thread copy. Inactive/deleted menus return zeroed observations.
// Never consumes SaveSync/LoadSync or a lifecycle event.
void pc_save_ui_observe(PcSaveUiObservation* out);
