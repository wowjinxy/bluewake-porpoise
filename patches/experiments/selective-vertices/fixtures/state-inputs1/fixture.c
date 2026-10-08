#include "efb_input_diagnostic.h"

int main(int argc, char** argv) {
    if (argc != 4) return 3;
    int allowed = atoi(argv[1]), count = atoi(argv[2]), variation = atoi(argv[3]);
    bw_efb_inputs_init(allowed);
    for (int i = 0; i < count; ++i) {
        unsigned kind = i % 2 ? 2 : 1;
        uint32_t actual = i % 2 ? 0x123456u + i : 0xFF123400u + i;
        if (variation) actual ^= 0x100u;
        uint32_t value = bw_efb_inputs_value(kind, 4, 0x80001100u + i * 4u,
            0xC8000000u + (kind == 2 ? 0x400000u : 0u) + i * 4u,
            600u + i, actual);
        printf("%08x\n", value);
    }
    bw_efb_inputs_finish();
    return 0;
}
