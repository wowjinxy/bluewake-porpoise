#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "overlap_observation.h"

int main(void) {
    const char* values[] = {NULL, "", "0", "1", "false", "00"};
    for (unsigned i = 0; i < sizeof values / sizeof values[0]; ++i) {
        assert(bluewake_overlap_observation_enabled(values[i], true));
        assert(bluewake_overlap_observation_enabled(values[i], false) == (i != 2));
    }
    return 0;
}
