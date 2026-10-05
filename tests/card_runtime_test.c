#include "card_runtime.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define CARD_PROBE_EX 0x8031D438u
#define CARD_MOUNT_ASYNC 0x8031DAFCu
#define CARD_MOUNT 0x8031DC9Cu
#define CARD_OPEN 0x8031E74Cu
#define CARD_CLOSE 0x8031E8C4u
#define CARD_CREATE 0x8031EC68u
#define CARD_READ 0x8031F0E0u
#define CARD_WRITE 0x8031F45Cu
#define CARD_WRITE_ASYNC 0x8031F348u
#define CARD_CALLBACK_RETURN 0x7FFF0000u

static void call_card(CPUState* cpu, u32 address) {
    cpu->pc = address;
    cpu->lr = 0x80001000u;
    assert(bluewake_card_runtime_dispatch(cpu));
    assert(cpu->pc == 0x80001000u);
}

typedef struct PendingWrite {
    CPUState* cpu;
    pthread_mutex_t status_mutex;
    bool entered;
    bool finished;
} PendingWrite;

static void* write_worker(void* argument) {
    PendingWrite* pending = (PendingWrite*)argument;
    assert(pthread_mutex_lock(&pending->status_mutex) == 0);
    pending->entered = true;
    assert(pthread_mutex_unlock(&pending->status_mutex) == 0);
    // This is the real runtime dispatch, not a simulated write or lock probe.
    call_card(pending->cpu, CARD_WRITE_ASYNC);
    assert(pthread_mutex_lock(&pending->status_mutex) == 0);
    pending->finished = true;
    assert(pthread_mutex_unlock(&pending->status_mutex) == 0);
    return NULL;
}

static void write_status(PendingWrite* pending, bool* entered, bool* finished) {
    assert(pthread_mutex_lock(&pending->status_mutex) == 0);
    *entered = pending->entered;
    *finished = pending->finished;
    assert(pthread_mutex_unlock(&pending->status_mutex) == 0);
}

static u8* persistent_bytes(const char* path, size_t* length) {
    FILE* file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size > 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    u8* bytes = (u8*)malloc((size_t)size);
    assert(bytes != NULL);
    assert(fread(bytes, 1, (size_t)size, file) == (size_t)size);
    assert(fclose(file) == 0);
    *length = (size_t)size;
    return bytes;
}

static void expect_callback(CPUState* cpu, u32 address) {
    const u32 saved_pc = cpu->pc;
    cpu->gpr[20] = 0x12345678u;
    bluewake_card_runtime_service_callback(cpu);
    assert(cpu->pc == address);
    assert(cpu->lr == CARD_CALLBACK_RETURN);
    assert(cpu->gpr[3] == 0u);
    assert(cpu->gpr[4] == DOL_CARD_RESULT_READY);
    cpu->pc = CARD_CALLBACK_RETURN;
    cpu->gpr[20] = 0u;
    bluewake_card_runtime_service_callback(cpu);
    assert(cpu->pc == saved_pc);
    assert(cpu->gpr[20] == 0x12345678u);
}

int main(int argc, char** argv) {
    // An optional fixture parent lets Windows and POSIX builds retain receipts
    // without depending on /tmp. All cards, including the default-path check,
    // stay under this unique test-owned directory.
    const char* fixture_parent = argc > 1 ? argv[1] : ".";
    assert(mkdir(fixture_parent, 0755) == 0 || errno == EEXIST);
    struct timespec now;
    assert(clock_gettime(CLOCK_REALTIME, &now) == 0);
    char directory[4096];
    assert(snprintf(directory, sizeof directory, "%s/card-runtime-%llu-%lu",
                    fixture_parent, (unsigned long long)now.tv_sec,
                    (unsigned long)now.tv_nsec) < (int)sizeof directory);
    assert(mkdir(directory, 0755) == 0);
    char path[4096];
    assert(snprintf(path, sizeof path, "%s/nested/GZLE01.card", directory) <
           (int)sizeof path);

    CPUState cpu;
    assert(cpu_init(&cpu));
    assert(!bluewake_card_runtime_begin_snapshot(NULL));
    assert(!bluewake_card_runtime_begin_snapshot(""));
    assert(!bluewake_card_runtime_begin_snapshot(path)); // closed
    bluewake_card_runtime_close(); // rejected begin must not leak the mutex
    assert(bluewake_card_runtime_open(path));
    assert(strcmp(bluewake_card_runtime_path(), path) == 0);
    assert(!bluewake_card_runtime_begin_snapshot(NULL));
    assert(!bluewake_card_runtime_begin_snapshot(""));
    assert(!bluewake_card_runtime_begin_snapshot("other/GZLE01.card"));
    assert(bluewake_card_runtime_begin_snapshot(path));
    assert(dol_card_validate(path));
    bluewake_card_runtime_end_snapshot();

    bluewake_card_runtime_suspend_writes(true);
    assert(!bluewake_card_runtime_begin_snapshot(path));
    cpu.gpr[3] = 0u;
    call_card(&cpu, CARD_MOUNT);
    assert((s32)cpu.gpr[3] == DOL_CARD_RESULT_BUSY);
    bluewake_card_runtime_suspend_writes(false);

    const u32 size_address = 0x80002000u;
    const u32 sector_address = 0x80002004u;
    cpu.gpr[3] = 0u;
    cpu.gpr[4] = size_address;
    cpu.gpr[5] = sector_address;
    call_card(&cpu, CARD_PROBE_EX);
    assert((s32)cpu.gpr[3] == 0);
    assert(mem_read32(&cpu, size_address) == 4u);
    assert(mem_read32(&cpu, sector_address) == 8192u);

    cpu.gpr[3] = 0u;
    cpu.gpr[6] = 0xFFFFFFFFu;
    call_card(&cpu, CARD_MOUNT);
    assert((s32)cpu.gpr[3] == 0);

    cpu.gpr[3] = 0u;
    cpu.gpr[6] = 0x80003000u;
    call_card(&cpu, CARD_MOUNT_ASYNC);
    assert((s32)cpu.gpr[3] == 0);
    cpu.gpr[20] = 0x12345678u;
    bluewake_card_runtime_service_callback(&cpu);
    assert(cpu.pc == 0x80003000u);
    assert(cpu.lr == CARD_CALLBACK_RETURN);
    assert(cpu.gpr[3] == 0u);
    assert(cpu.gpr[4] == 0u);
    cpu.pc = CARD_CALLBACK_RETURN;
    cpu.gpr[20] = 0u;
    bluewake_card_runtime_service_callback(&cpu);
    assert(cpu.pc == 0x80001000u);
    assert(cpu.gpr[20] == 0x12345678u);

    const u32 name_address = 0x80002100u;
    const u32 info_address = 0x80002200u;
    const u32 write_address = 0x80004000u;
    const u32 read_address = 0x80008000u;
    const char name[] = "gczelda";
    for (size_t i = 0; i < sizeof name; i++)
        mem_write8(&cpu, name_address + (u32)i, (u8)name[i]);

    cpu.gpr[3] = 0u;
    cpu.gpr[4] = name_address;
    cpu.gpr[5] = 8192u;
    cpu.gpr[6] = info_address;
    cpu.gpr[7] = 0xFFFFFFFFu;
    call_card(&cpu, CARD_CREATE);
    assert((s32)cpu.gpr[3] == 0);

    for (u32 i = 0; i < 8192u; i++)
        mem_write8(&cpu, write_address + i, (u8)(i * 37u));
    cpu.gpr[3] = info_address;
    cpu.gpr[4] = write_address;
    cpu.gpr[5] = 8192u;
    cpu.gpr[6] = 0u;
    cpu.gpr[7] = 0xFFFFFFFFu;
    call_card(&cpu, CARD_WRITE);
    assert((s32)cpu.gpr[3] == 0);

    // A queued completion before the snapshot must survive it unchanged.
    cpu.gpr[3] = 0u;
    cpu.gpr[6] = 0x80003000u;
    call_card(&cpu, CARD_MOUNT_ASYNC);
    assert((s32)cpu.gpr[3] == DOL_CARD_RESULT_READY);

    size_t original_length;
    u8* original = persistent_bytes(path, &original_length);
    for (u32 i = 0; i < 8192u; i++)
        mem_write8(&cpu, write_address + i, (u8)(i * 37u + 19u));
    cpu.gpr[3] = info_address;
    cpu.gpr[4] = write_address;
    cpu.gpr[5] = 8192u;
    cpu.gpr[6] = 0u;
    cpu.gpr[7] = 0x80003004u;
    PendingWrite pending = {0};
    pending.cpu = &cpu;
    assert(pthread_mutex_init(&pending.status_mutex, NULL) == 0);
    assert(bluewake_card_runtime_begin_snapshot(path));
    pthread_t worker;
    assert(pthread_create(&worker, NULL, write_worker, &pending) == 0);
    bool entered = false;
    bool finished = false;
    for (unsigned tries = 0; tries < 2000 && !entered; ++tries) {
        write_status(&pending, &entered, &finished);
        if (!entered) usleep(1000);
    }
    assert(entered && !finished);
    // Give the worker a chance to reach the real dispatch lock. It must wait,
    // rather than returning BUSY or changing the persistent card.
    usleep(50000);
    write_status(&pending, &entered, &finished);
    assert(!finished);
    assert(dol_card_validate(path));
    size_t held_length;
    u8* held = persistent_bytes(path, &held_length);
    assert(held_length == original_length);
    assert(memcmp(held, original, held_length) == 0);
    free(held);
    bluewake_card_runtime_end_snapshot();
    assert(pthread_join(worker, NULL) == 0);
    write_status(&pending, &entered, &finished);
    assert(finished);
    assert((s32)cpu.gpr[3] == DOL_CARD_RESULT_READY);
    assert(pthread_mutex_destroy(&pending.status_mutex) == 0);
    assert(dol_card_validate(path));
    size_t updated_length;
    u8* updated = persistent_bytes(path, &updated_length);
    assert(updated_length == original_length);
    assert(memcmp(updated, original, updated_length) != 0);
    free(updated);
    free(original);
    expect_callback(&cpu, 0x80003000u); // completion queued before begin
    expect_callback(&cpu, 0x80003004u); // waiting write completed after end
    const u32 callback_idle_pc = cpu.pc;
    bluewake_card_runtime_service_callback(&cpu);
    assert(cpu.pc == callback_idle_pc);

    cpu.gpr[3] = info_address;
    call_card(&cpu, CARD_CLOSE);
    assert((s32)cpu.gpr[3] == 0);

    bluewake_card_runtime_close();
    assert(!bluewake_card_runtime_begin_snapshot(path));
    assert(bluewake_card_runtime_open(path));
    assert(bluewake_card_runtime_begin_snapshot(path));
    assert(dol_card_validate(path));
    bluewake_card_runtime_end_snapshot();
    cpu.gpr[3] = 0u;
    cpu.gpr[6] = 0xFFFFFFFFu;
    call_card(&cpu, CARD_MOUNT);

    cpu.gpr[3] = 0u;
    cpu.gpr[4] = name_address;
    cpu.gpr[5] = info_address;
    call_card(&cpu, CARD_OPEN);
    assert((s32)cpu.gpr[3] == 0);
    cpu.gpr[3] = info_address;
    cpu.gpr[4] = read_address;
    cpu.gpr[5] = 8192u;
    cpu.gpr[6] = 0u;
    cpu.gpr[7] = 0xFFFFFFFFu;
    call_card(&cpu, CARD_READ);
    assert((s32)cpu.gpr[3] == 0);
    for (u32 i = 0; i < 8192u; i++)
        assert(mem_read8(&cpu, read_address + i) == (u8)(i * 37u + 19u));

    cpu.gpr[3] = 0u;
    cpu.gpr[6] = 0x80003008u;
    call_card(&cpu, CARD_MOUNT_ASYNC);
    assert((s32)cpu.gpr[3] == DOL_CARD_RESULT_READY);
    expect_callback(&cpu, 0x80003008u); // callbacks still work after reopen

    bluewake_card_runtime_close();
    cpu_free(&cpu);
    assert(remove(path) == 0);

    assert(setenv("HOME", directory, 1) == 0);
    assert(bluewake_card_runtime_open(NULL));
    char default_path[4096];
    assert(snprintf(default_path, sizeof default_path,
                    "%s/Library/Application Support/BlueWake/GZLE01.card",
                    directory) < (int)sizeof default_path);
    assert(strcmp(bluewake_card_runtime_path(), default_path) == 0);
    bluewake_card_runtime_close();
    assert(access(default_path, F_OK) == 0);
    assert(remove(default_path) == 0);
    puts("CARD bridge persistence and snapshot-lock contract test passed.");
    return 0;
}
