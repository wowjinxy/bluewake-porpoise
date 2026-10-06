#include <stdint.h>
#include <stdio.h>
__declspec(dllimport) uint32_t bluewake_probe(uint32_t);
__declspec(dllimport) uint32_t bluewake_alias_serial(void);
__declspec(dllimport) void ppc_guest_alias_clear(void);
__declspec(dllimport) void ppc_guest_alias_add(void);
__declspec(dllimport) void ppc_guest_alias_remove(void);
int main(void) {
    uint32_t value=0xFFFFFFFFu;
    for(unsigned i=0;i<100000;++i) {
        value=value*1664525u+1013904223u;
        uint32_t expected=((value*0x9E3779B1u)+(value>>7))^0x12345678u;
        if(bluewake_probe(value)!=expected) return 1;
    }
    ppc_guest_alias_clear();ppc_guest_alias_add();ppc_guest_alias_remove();
    if(bluewake_alias_serial()!=2) return 2;
    puts("PASS 100000 authored cross-TU comparisons and explicit exports");return 0;
}
