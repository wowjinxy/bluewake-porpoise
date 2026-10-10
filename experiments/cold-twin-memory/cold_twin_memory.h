#ifndef BW_COLD_TWIN_MEMORY1_H
#define BW_COLD_TWIN_MEMORY1_H
/* Private experiment: caller has just proved the original gather RAM guard.
 * This contract is deliberately restricted to the exact fixed-MEM1 C2 profile.
 * No callback, reservation or journal operation is elided on a failed guard. */
#if !defined(BW_GUEST_MEM1) || !defined(BW_GUEST_MEM1_SIZE)
#error "cold twin memory requires the qualified fixed MEM1 compile profile"
#endif
_Static_assert(BW_GUEST_MEM1_SIZE == 0x02000000u, "fixed MEM1 size");
static inline __attribute__((always_inline)) u8 bwcold_load8(u32 address) {
    return BW_RAM_BYTES[address - GC_RAM_BASE];
}
static inline __attribute__((always_inline)) u16 bwcold_load16(u32 address) {
    u16 value;
    memcpy(&value, BW_RAM_BYTES + (address - GC_RAM_BASE), 2);
    return __builtin_bswap16(value);
}
static inline __attribute__((always_inline)) u32 bwcold_load32(u32 address) {
    u32 value;
    memcpy(&value, BW_RAM_BYTES + (address - GC_RAM_BASE), 4);
    return __builtin_bswap32(value);
}
static inline __attribute__((always_inline)) void bwcold_store8(u32 address, u8 value) {
    BW_RAM_BYTES[address - GC_RAM_BASE] = value;
}
static inline __attribute__((always_inline)) void bwcold_store16(u32 address, u16 value) {
    const u16 word = __builtin_bswap16(value);
    memcpy(BW_RAM_BYTES + (address - GC_RAM_BASE), &word, 2);
}
static inline __attribute__((always_inline)) void bwcold_store32(u32 address, u32 value) {
    const u32 word = __builtin_bswap32(value);
    memcpy(BW_RAM_BYTES + (address - GC_RAM_BASE), &word, 4);
}
#endif
