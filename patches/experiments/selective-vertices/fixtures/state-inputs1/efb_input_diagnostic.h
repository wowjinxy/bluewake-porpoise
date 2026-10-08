#ifndef BLUEWAKE_PRIVATE_EFB_INPUT_DIAGNOSTIC_H
#define BLUEWAKE_PRIVATE_EFB_INPUT_DIAGNOSTIC_H
/* Private correctness-only input control. Never included in a timing build. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static struct {
    FILE* stream;
    unsigned mode;
    int finished;
    uint64_t count, total, physical_differences;
} bw_efb_inputs;

static void bw_efb_inputs_fail(const char* reason) {
    fprintf(stderr, "[efb-inputs] FAIL mode=%u entry=%llu reason=%s\n",
            bw_efb_inputs.mode, (unsigned long long)bw_efb_inputs.count, reason);
    fflush(stderr);
    _Exit(90);
}

static uint64_t bw_efb_inputs_get(const unsigned char* p, unsigned n) {
    uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= (uint64_t)p[i] << (i * 8u);
    return v;
}
static void bw_efb_inputs_put(unsigned char* p, unsigned n, uint64_t v) {
    for (unsigned i = 0; i < n; ++i) p[i] = (unsigned char)(v >> (i * 8u));
}

static void bw_efb_inputs_finish(void) {
    if (!bw_efb_inputs.mode || bw_efb_inputs.finished) return;
    bw_efb_inputs.finished = 1;
    if (bw_efb_inputs.mode == 2 && bw_efb_inputs.count != bw_efb_inputs.total)
        bw_efb_inputs_fail("unconsumed replay entries");
    if (fclose(bw_efb_inputs.stream) != 0) bw_efb_inputs_fail("close");
    bw_efb_inputs.stream = NULL;
    fprintf(stderr, "[efb-inputs] PASS mode=%u entries=%llu total=%llu physical_differences=%llu timing_eligible=0\n",
            bw_efb_inputs.mode, (unsigned long long)bw_efb_inputs.count,
            (unsigned long long)(bw_efb_inputs.mode == 1 ? bw_efb_inputs.count : bw_efb_inputs.total),
            (unsigned long long)bw_efb_inputs.physical_differences);
}

static void bw_efb_inputs_init(int allowed) {
    const char* record = getenv("BLUEWAKE_EFB_INPUT_RECORD");
    const char* replay = getenv("BLUEWAKE_EFB_INPUT_REPLAY");
    if (!record && !replay) return;
    if (!allowed || (record && replay) || (record && !*record) || (replay && !*replay))
        bw_efb_inputs_fail("context or conflicting/empty options");
    unsigned char header[16] = {'B','W','E','F','B','0','1',0,32,0,0,0,0,0,0,0};
    if (record) {
        bw_efb_inputs.mode = 1;
#ifdef _WIN32
        int fd = _open(record, _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
        if (fd < 0) bw_efb_inputs_fail("exclusive record open");
        bw_efb_inputs.stream = _fdopen(fd, "wb");
        if (!bw_efb_inputs.stream) { _close(fd); bw_efb_inputs_fail("record fdopen"); }
#else
        int fd = open(record, O_CREAT | O_EXCL | O_WRONLY, 0600);
        if (fd < 0) bw_efb_inputs_fail("exclusive record open");
        bw_efb_inputs.stream = fdopen(fd, "wb");
        if (!bw_efb_inputs.stream) { close(fd); bw_efb_inputs_fail("record fdopen"); }
#endif
        if (fwrite(header, 1, sizeof(header), bw_efb_inputs.stream) != sizeof(header))
            bw_efb_inputs_fail("record header write");
    } else {
        bw_efb_inputs.mode = 2;
        bw_efb_inputs.stream = fopen(replay, "rb");
        if (!bw_efb_inputs.stream) bw_efb_inputs_fail("replay open");
        unsigned char actual[16];
        if (fread(actual, 1, sizeof(actual), bw_efb_inputs.stream) != sizeof(actual) ||
            memcmp(header, actual, sizeof(actual))) bw_efb_inputs_fail("replay header");
        if (fseek(bw_efb_inputs.stream, 0, SEEK_END)) bw_efb_inputs_fail("replay end seek");
        long bytes = ftell(bw_efb_inputs.stream);
        if (bytes < 16 || bytes > 16 + 32 * 1000000L || (bytes - 16) % 32)
            bw_efb_inputs_fail("replay length");
        bw_efb_inputs.total = (uint64_t)(bytes - 16) / 32;
        if (fseek(bw_efb_inputs.stream, 16, SEEK_SET)) bw_efb_inputs_fail("replay data seek");
    }
    if (atexit(bw_efb_inputs_finish)) bw_efb_inputs_fail("atexit");
}

static uint32_t bw_efb_inputs_value(unsigned kind, unsigned size, uint32_t pc,
                                    uint32_t address, uint64_t retrace, uint32_t actual) {
    if (!bw_efb_inputs.mode) return actual;
    if ((kind != 1 && kind != 2) || size != 4 || (kind == 2 && actual > 0xFFFFFFu))
        bw_efb_inputs_fail("invalid physical input");
    if (bw_efb_inputs.count >= 1000000) bw_efb_inputs_fail("entry bound");
    unsigned char entry[32] = {0};
    bw_efb_inputs_put(entry, 8, bw_efb_inputs.count);
    bw_efb_inputs_put(entry + 8, 8, retrace);
    bw_efb_inputs_put(entry + 16, 4, pc);
    bw_efb_inputs_put(entry + 20, 4, address);
    bw_efb_inputs_put(entry + 24, 4, actual);
    entry[28] = (unsigned char)kind;
    entry[29] = (unsigned char)size;
    uint32_t value = actual;
    if (bw_efb_inputs.mode == 1) {
        if (fwrite(entry, 1, sizeof(entry), bw_efb_inputs.stream) != sizeof(entry))
            bw_efb_inputs_fail("record entry write");
    } else {
        unsigned char expected[32];
        if (bw_efb_inputs.count >= bw_efb_inputs.total ||
            fread(expected, 1, sizeof(expected), bw_efb_inputs.stream) != sizeof(expected))
            bw_efb_inputs_fail("replay exhausted");
        if (memcmp(entry, expected, 24) || memcmp(entry + 28, expected + 28, 4))
            bw_efb_inputs_fail("replay context mismatch");
        value = (uint32_t)bw_efb_inputs_get(expected + 24, 4);
        if (kind == 2 && value > 0xFFFFFFu) bw_efb_inputs_fail("invalid replay depth");
        if (value != actual) ++bw_efb_inputs.physical_differences;
    }
    ++bw_efb_inputs.count;
    return value;
}
#endif
