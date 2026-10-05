#ifndef BW_SPRINT_FIXTURE_CPU_H
#define BW_SPRINT_FIXTURE_CPU_H
#include <stdint.h>
typedef uint32_t u32;
typedef struct CPUState { uint32_t speed,rate,sentinel; uint8_t* ram; uint32_t ram_size; } CPUState;
#ifdef __cplusplus
extern "C" {
#endif
u32 mem_read32(CPUState*,u32);
void mem_write32(CPUState*,u32,u32);
#ifdef __cplusplus
}
#endif
#endif
