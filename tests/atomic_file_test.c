// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
static int failing;
#ifdef _WIN32
#include <windows.h>
static unsigned replacement_failures;
static BOOL test_move(const wchar_t* from, const wchar_t* to, DWORD flags) {
    if (failing) {
        assert((flags & MOVEFILE_REPLACE_EXISTING) != 0);
        ++replacement_failures;
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return MoveFileExW(from, to, flags);
}
#define MoveFileExW test_move
#else
static int test_rename(const char* from, const char* to) {
    if (failing) { errno = EACCES; return -1; }
    return rename(from, to);
}
#define rename test_rename
#endif
#include "atomic_file.h"
#ifdef _WIN32
#undef MoveFileExW
#else
#undef rename
#endif
#include "../runtime/host/src/ipl_sram.c"
int main(void) {
    char path[256];
    snprintf(path, sizeof path, "atomic-test-%lu", (unsigned long)getpid());
    BluewakeIplSram d;
    bluewake_ipl_sram_init(&d, path);
    u8 old[64], bytes[64];
    memcpy(old, d.sram, sizeof old);
    d.sram[19] ^= 4;
    failing = 1;
    persist(&d);
#ifdef _WIN32
    assert(replacement_failures == 1);
#endif
    FILE* file = fopen(path, "rb");
    assert(file && fread(bytes, 1, sizeof bytes, file) == sizeof bytes);
    fclose(file);
    assert(memcmp(old, bytes, sizeof bytes) == 0);
    assert(bw_atomic_flush_path(path));
    char missing[320]; snprintf(missing, sizeof missing, "%s.missing", path);
    assert(!bw_atomic_flush_path(missing));
    bool dirty = true;
    char* pending = bw_atomic_path(path);
    FILE* settings = fopen(pending, "wb");
    assert(settings && fputs("settings=new", settings) >= 0);
    assert(!bw_atomic_finish_dirty(settings, pending, path, &dirty));
    assert(dirty);
#ifdef _WIN32
    assert(replacement_failures == 2);
#endif
    failing = 0;
    settings = fopen(pending, "wb");
    assert(settings && fputs("settings=new", settings) >= 0);
    assert(bw_atomic_finish_dirty(settings, pending, path, &dirty));
    assert(!dirty);
    free(pending);
    persist(&d);
    file = fopen(path, "rb");
    assert(file && fread(bytes, 1, sizeof bytes, file) == sizeof bytes);
    fclose(file);
    assert(memcmp(d.sram, bytes, sizeof bytes) == 0);
    char copy[300]; snprintf(copy, sizeof copy, "%s.copy", path);
    assert(bw_atomic_copy_if_missing(path, copy));
    file = fopen(copy, "rb");
    assert(file && fread(bytes, 1, sizeof bytes, file) == sizeof bytes);
    fclose(file);
    assert(memcmp(d.sram, bytes, sizeof bytes) == 0);
    d.sram[19] ^= 1; persist(&d);
    assert(bw_atomic_copy_if_missing(path, copy));
    file = fopen(copy, "rb");
    assert(file && fread(old, 1, sizeof old, file) == sizeof old);
    fclose(file);
    assert(memcmp(old, bytes, sizeof old) == 0);
    remove(copy);
    remove(path);
    puts("atomic file failure/retry regression passed");
}
