// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_CARD_IMPORT_H
#define BLUEWAKE_CARD_IMPORT_H
#include <cstdint>
#include <string>
#include <vector>

// Pure, bounded format conversion. The caller first makes an immutable copy,
// then validates the resulting DOLCARD1 bytes with GXRuntime before publication.
// All three quest logs and the nine picture blocks stay byte-for-byte intact.
// No original file, slot checksum, item flag or progress field is repaired.
struct BwCardImportInfo {
    enum class Format { Gci, Raw } format = Format::Gci;
    bool backup_copy_valid = false;
    bool redundant_filesystem_copy_used = false;
};
bool bluewake_card_import_convert(const std::vector<uint8_t>& source,
    std::vector<uint8_t>& container, BwCardImportInfo& info, std::string& error);
#endif
