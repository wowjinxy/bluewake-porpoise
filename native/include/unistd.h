// SPDX-License-Identifier: MIT
// Minimal unistd.h compatibility shim for Windows
#ifndef COS_PC_COMPAT_UNISTD_H
#define COS_PC_COMPAT_UNISTD_H

#if !defined(_WIN32)
#include_next <unistd.h>
#else

#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

// Windows headers carry their own linkage declarations. Keep them outside
// the compatibility functions' extern "C" block, and omit min/max macros.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// UCRT declares off_t itself (32 bits on Win64). Keep that ABI intact;
// positioned I/O and fseeko/ftello below use explicit 64-bit offsets.

#ifdef __cplusplus
extern "C" {
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#endif

#ifdef _WIN32
#define stat _stat64
#endif

#include <fcntl.h>
#include <stdarg.h>

#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#endif
#ifndef O_WRONLY
#define O_WRONLY _O_WRONLY
#endif
#ifndef O_RDWR
#define O_RDWR _O_RDWR
#endif
#ifndef O_APPEND
#define O_APPEND _O_APPEND
#endif
#ifndef O_CREAT
#define O_CREAT _O_CREAT
#endif
#ifndef O_TRUNC
#define O_TRUNC _O_TRUNC
#endif
#ifndef O_EXCL
#define O_EXCL _O_EXCL
#endif
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif

#ifndef O_CLOEXEC
#ifdef _O_NOINHERIT
#define O_CLOEXEC _O_NOINHERIT
#else
#define O_CLOEXEC 0
#endif
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// io.h/stdio.h/process.h already declare the legacy POSIX spellings of
// open, access, unlink, isatty, fileno, getpid, close, read and write.
// Defining static functions with those names conflicts with UCRT's externs.

typedef SSIZE_T ssize_t;

typedef uint32_t useconds_t;

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define R_OK 4
#define W_OK 2
#define X_OK 0
#define F_OK 0

#include <direct.h>

static inline int cos_pc_mkdir(const char* path, int mode) {
    (void)mode;
    return _mkdir(path);
}
// UCRT's legacy mkdir has one argument; POSIX callers supply a mode as well.
#define mkdir(path, mode) cos_pc_mkdir((path), (mode))

static inline ssize_t pread(int fd, void* buf, size_t count, int64_t offset) {
    int64_t cur = _lseeki64(fd, 0, SEEK_CUR);
    if (_lseeki64(fd, offset, SEEK_SET) < 0) return -1;
    int bytes = _read(fd, buf, (unsigned int)count);
    _lseeki64(fd, cur, SEEK_SET);
    return (ssize_t)bytes;
}

static inline ssize_t pwrite(int fd, const void* buf, size_t count, int64_t offset) {
    int64_t cur = _lseeki64(fd, 0, SEEK_CUR);
    if (_lseeki64(fd, offset, SEEK_SET) < 0) return -1;
    int bytes = _write(fd, buf, (unsigned int)count);
    _lseeki64(fd, cur, SEEK_SET);
    return (ssize_t)bytes;
}

static inline unsigned int sleep(unsigned int seconds) {
    Sleep(seconds * 1000);
    return 0;
}

#include <time.h>

static inline int usleep(useconds_t usec) {
    Sleep(usec / 1000);
    return 0;
}

static inline int nanosleep(const struct timespec* req, struct timespec* rem) {
    (void)rem;
    if (!req) return -1;
    unsigned long ms = (unsigned long)(req->tv_sec * 1000 + (req->tv_nsec + 999999) / 1000000);
    Sleep(ms);
    return 0;
}

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

static inline int clock_gettime(int clk_id, struct timespec* ts) {
    if (ts == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (clk_id == CLOCK_REALTIME) {
        if (timespec_get(ts, TIME_UTC) == TIME_UTC) return 0;
        errno = EIO;
        return -1;
    }
    if (clk_id == CLOCK_MONOTONIC) {
        LARGE_INTEGER frequency, counter;
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
            !QueryPerformanceCounter(&counter) || counter.QuadPart < 0) {
            errno = EIO;
            return -1;
        }
        const int64_t remainder = counter.QuadPart % frequency.QuadPart;
        ts->tv_sec = (time_t)(counter.QuadPart / frequency.QuadPart);
        // Divide the whole seconds first so a long uptime cannot overflow the
        // nanosecond multiplication. The fallback covers unusually high QPC
        // frequencies while keeping tv_nsec in the timespec range.
        if (remainder <= INT64_MAX / 1000000000LL) {
            ts->tv_nsec = (long)(remainder * 1000000000LL / frequency.QuadPart);
        } else {
            const long double nanoseconds = (long double)remainder * 1000000000.0L /
                                            (long double)frequency.QuadPart;
            ts->tv_nsec = nanoseconds >= 1000000000.0L ? 999999999L : (long)nanoseconds;
        }
        return 0;
    }
    errno = EINVAL;
    return -1;
}

static inline int pause(void) {
    Sleep(0xFFFFFFFF);
    return -1;
}

static inline int ftrylockfile(FILE* f) {
    _lock_file(f);
    return 0;
}

static inline void funlockfile(FILE* f) {
    _unlock_file(f);
}

static inline void flockfile(FILE* f) {
    _lock_file(f);
}

static inline int fseeko(FILE* stream, int64_t offset, int origin) {
    return _fseeki64(stream, offset, origin);
}

static inline int64_t ftello(FILE* stream) {
    return _ftelli64(stream);
}

static inline int setenv(const char* name, const char* value, int overwrite) {
    if (!overwrite && getenv(name) != NULL) {
        return 0;
    }
    return _putenv_s(name, value);
}

static inline int unsetenv(const char* name) {
    return _putenv_s(name, "");
}

static inline char* realpath(const char* path, char* resolved_path) {
    return _fullpath(resolved_path, path, PATH_MAX);
}

#include <strings.h>

#ifdef __cplusplus
}
#endif

#ifdef EXCEPTION_BREAKPOINT
#undef EXCEPTION_BREAKPOINT
#endif

#endif // _WIN32
#endif // COS_PC_COMPAT_UNISTD_H
