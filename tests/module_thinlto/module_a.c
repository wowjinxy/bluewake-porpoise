#include <stdint.h>
extern uint32_t thin_helper(uint32_t);
__declspec(dllexport) uint32_t bluewake_probe(uint32_t value) { return thin_helper(value)^0x12345678u; }
/* Explicit host-resolved symbols must survive internalization. */
static uint32_t alias_serial;
void ppc_set_mem_write_journal(void) {++alias_serial;}
void ppc_guest_alias_clear(void) {alias_serial=0;}
void ppc_guest_alias_add(void) {++alias_serial;}
void ppc_guest_alias_add_shared(void) {++alias_serial;}
void ppc_guest_alias_get_storage(void) {++alias_serial;}
void ppc_guest_alias_remove(void) {++alias_serial;}
void ppc_guest_alias_resolve(void) {++alias_serial;}
__declspec(dllexport) uint32_t bluewake_alias_serial(void) {return alias_serial;}
