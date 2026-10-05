// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
static int inject_race;
#ifdef _WIN32
static unsigned race_calls;
static BOOL raced_move(const wchar_t* from, const wchar_t* to, DWORD flags) {
    // Intercept the production wide-path API, after its existence check and
    // immediately before publication. The competing process's newer target
    // must win because migration never requests replacement.
    assert((flags & MOVEFILE_REPLACE_EXISTING) == 0);
    ++race_calls;
    if (inject_race) {
        FILE* file = _wfopen(to, L"wb");
        assert(file && fputs("newer player data", file) >= 0 && fclose(file) == 0);
    }
    return MoveFileExW(from, to, flags);
}
#define MoveFileExW raced_move
#else
static void competing_target(const char* path) {
    if (!inject_race) return;
    FILE* file = fopen(path, "wb");
    assert(file && fputs("newer player data", file) >= 0 && fclose(file) == 0);
}
static int raced_rename(const char* from, const char* to) {
    competing_target(to);
    return rename(from, to);
}
static int raced_link(const char* from, const char* to) {
    competing_target(to);
    return link(from, to);
}
#define link raced_link
#define rename raced_rename
#endif
#include "atomic_file.h"
#ifdef _WIN32
#undef MoveFileExW
#else
#undef rename
#undef link
#endif
int main(void) {
    char from[256], to[256];
    snprintf(from, sizeof from, "copy-race-%lu-\xE2\x98\x83-source", (unsigned long)getpid());
    snprintf(to, sizeof to, "copy-race-%lu-\xE2\x98\x83-target", (unsigned long)getpid());
    FILE* file = bw_atomic_open(from, "wb");
    assert(file && fputs("legacy data", file) >= 0 && fclose(file) == 0);
    inject_race = 1;
    assert(bw_atomic_copy_if_missing(from, to));
#ifdef _WIN32
    assert(race_calls == 1);
#endif
    char bytes[64] = {0};
    file = bw_atomic_open(to, "rb");
    assert(file && fread(bytes, 1, sizeof bytes - 1, file) == strlen("newer player data"));
    assert(fclose(file) == 0 && strcmp(bytes, "newer player data") == 0);
    assert(bw_atomic_remove(to) == 0);
    inject_race = 0;
    assert(bw_atomic_copy_if_missing(from, to));
#ifdef _WIN32
    assert(race_calls == 2);
#endif
    file = bw_atomic_open(to, "rb");
    memset(bytes, 0, sizeof bytes);
    assert(file && fread(bytes, 1, sizeof bytes - 1, file) == strlen("legacy data"));
    assert(fclose(file) == 0 && strcmp(bytes, "legacy data") == 0);
    assert(bw_atomic_remove(from) == 0 && bw_atomic_remove(to) == 0);
    puts("atomic migration preserves a concurrently created target");
}
