// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from BlueWake runtime/host/src; see PROVENANCE.md.
#ifndef PC_NATIVE_NETWORK_ATOMIC_FILE_H
#define PC_NATIVE_NETWORK_ATOMIC_FILE_H
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wchar.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef _WIN32
static inline wchar_t* pc_net_atomic_wide_path(const char* path) {
    if (path == NULL) { errno = EINVAL; return NULL; }
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (count <= 0) { errno = EINVAL; return NULL; }
    wchar_t* wide = (wchar_t*)malloc((size_t)count * sizeof(*wide));
    if (wide == NULL) { errno = ENOMEM; return NULL; }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, count) != count) {
        free(wide); errno = EINVAL; return NULL;
    }
    // Pending room filenames are deliberately unique and can cross MAX_PATH
    // under private run directories. Use an absolute extended UTF-16 path for
    // CRT/Win32 operations, without relying on a process-wide manifest setting.
    DWORD absolute_count = GetFullPathNameW(wide, 0, NULL, NULL);
    if (absolute_count == 0 || absolute_count > 32760) {
        free(wide); errno = EINVAL; return NULL;
    }
    wchar_t* absolute = (wchar_t*)malloc(((size_t)absolute_count + 8) * sizeof(*absolute));
    if (absolute == NULL) { free(wide); errno = ENOMEM; return NULL; }
    DWORD written = GetFullPathNameW(wide, absolute_count, absolute, NULL);
    free(wide);
    if (written == 0 || written >= absolute_count) { free(absolute); errno = EINVAL; return NULL; }
    for (DWORD i = 0; i < written; ++i) if (absolute[i] == L'/') absolute[i] = L'\\';
    if (wcsncmp(absolute, L"\\\\?\\", 4) == 0) return absolute;
    wchar_t* extended = (wchar_t*)malloc(((size_t)written + 9) * sizeof(*extended));
    if (extended == NULL) { free(absolute); errno = ENOMEM; return NULL; }
    if (written >= 2 && absolute[0] == L'\\' && absolute[1] == L'\\') {
        wcscpy(extended, L"\\\\?\\UNC\\");
        wcscpy(extended + 8, absolute + 2);
    } else if (written >= 3 && absolute[1] == L':' && absolute[2] == L'\\') {
        wcscpy(extended, L"\\\\?\\");
        wcscpy(extended + 4, absolute);
    } else {
        free(extended); free(absolute); errno = EINVAL; return NULL;
    }
    free(absolute);
    return extended;
}
#endif
static inline FILE* pc_net_atomic_open(const char* path, const char* mode) {
#ifdef _WIN32
    wchar_t* wide = pc_net_atomic_wide_path(path);
    wchar_t wide_mode[16];
    if (wide == NULL) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1, wide_mode, 16) <= 0) {
        free(wide); errno = EINVAL; return NULL;
    }
    FILE* file = _wfopen(wide, wide_mode);
    free(wide);
    return file;
#else
    return fopen(path, mode);
#endif
}
static inline int pc_net_atomic_remove(const char* path) {
#ifdef _WIN32
    wchar_t* wide = pc_net_atomic_wide_path(path);
    if (wide == NULL) return -1;
    int result = _wremove(wide);
    free(wide);
    return result;
#else
    return remove(path);
#endif
}
static inline int pc_net_atomic_move(const char* from, const char* to, bool replace) {
#ifdef _WIN32
    wchar_t* source = pc_net_atomic_wide_path(from);
    wchar_t* target = source != NULL ? pc_net_atomic_wide_path(to) : NULL;
    if (target == NULL) { free(source); return -1; }
    bool moved = MoveFileExW(source, target, MOVEFILE_WRITE_THROUGH |
        (replace ? MOVEFILE_REPLACE_EXISTING : 0)) != FALSE;
    DWORD error = moved ? ERROR_SUCCESS : GetLastError();
    free(source); free(target);
    if (moved) return 0;
    errno = error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS ? EEXIST :
        error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? ENOENT : EACCES;
    return -1;
#else
    (void)replace;
    return rename(from, to);
#endif
}
static inline char* pc_net_atomic_path(const char* path) {
    size_t capacity = strlen(path) + 40;
    char* pending = (char*)malloc(capacity);
    if (pending) snprintf(pending, capacity, "%s.tmp-%lu", path, (unsigned long)
#ifdef _WIN32
        _getpid()
#else
        getpid()
#endif
    );
    return pending;
}
static inline bool pc_net_atomic_flush(FILE* file) {
    if (ferror(file) || fflush(file) != 0) return false;
#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#elif defined(__APPLE__)
    return fcntl(fileno(file), F_FULLFSYNC) == 0;
#else
    return fsync(fileno(file)) == 0;
#endif
}
static inline bool pc_net_atomic_finish(FILE* file, const char* pending, const char* path, bool ok) {
    if (ok && !pc_net_atomic_flush(file)) ok = false;
    if (fclose(file) != 0) ok = false;
    if (ok && pc_net_atomic_move(pending, path, true) != 0) ok = false;
    if (!ok) pc_net_atomic_remove(pending);
    return ok;
}
static inline bool pc_net_atomic_finish_dirty(FILE* file, const char* pending, const char* path, bool* dirty) {
    bool ok = pc_net_atomic_finish(file, pending, path, true);
    if (ok) *dirty = false;
    return ok;
}
static inline bool pc_net_atomic_flush_path(const char* path) {
    FILE* file = pc_net_atomic_open(path, "r+b");
    if (!file) return false;
    bool ok = pc_net_atomic_flush(file);
    if (fclose(file) != 0) ok = false;
    return ok;
}
// Startup migration retains the source and never replaces an existing target.
static inline bool pc_net_atomic_copy_if_missing(const char* from, const char* to) {
#ifdef _WIN32
    wchar_t* wide = pc_net_atomic_wide_path(to);
    if (wide == NULL) return false;
    DWORD attributes = GetFileAttributesW(wide);
    DWORD error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS;
    free(wide);
    if (attributes != INVALID_FILE_ATTRIBUTES) return true;
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return false;
#else
    struct stat status;
    if (stat(to, &status) == 0) return true;
    if (errno != ENOENT) return false;
#endif
    FILE* source = pc_net_atomic_open(from, "rb");
    if (source == NULL) return errno == ENOENT;
    char* pending = pc_net_atomic_path(to);
    FILE* target = pending ? pc_net_atomic_open(pending, "wb") : NULL;
    bool ok = target != NULL;
    char buffer[8192]; size_t count;
    while (ok && (count = fread(buffer, 1, sizeof buffer, source)) != 0)
        ok = fwrite(buffer, 1, count, target) == count;
    if (ferror(source)) ok = false;
    if (fclose(source) != 0) ok = false;
    if (target) {
        if (ok && !pc_net_atomic_flush(target)) ok = false;
        if (fclose(target) != 0) ok = false;
        // A target can appear after stat(), before this process holds a card
        // lock. Publish without replacement so migration cannot clobber it.
        if (ok) {
#ifdef _WIN32
            ok = pc_net_atomic_move(pending, to, false) == 0 || errno == EEXIST;
#else
            ok = link(pending, to) == 0 || errno == EEXIST;
#endif
        }
        pc_net_atomic_remove(pending);
    }
    free(pending);
    return ok;
}
#endif
