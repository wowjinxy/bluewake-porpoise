// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual runtime pacing policy without a game, renderer or clock sleeps.
#include "frame_interp.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fi = aurora::gfx::frame_interp;
static int failures = 0;
#define CHECK(condition)                                                                                     \
  do {                                                                                                       \
    if (!(condition)) {                                                                                      \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                                 \
      ++failures;                                                                                            \
    }                                                                                                        \
  } while (0)

static void frames(int count, bool overloaded = false) {
  for (int i = 0; i < count; ++i) {
    if (overloaded)
      fi::note_overload("pacing regression");
    fi::end_game_frame();
  }
}

int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "";
  if (argc > 2 || (mode[0] && std::strcmp(mode, "0") && std::strcmp(mode, "1"))) {
    std::fprintf(stderr, "usage: frame_interp_pacing_test [0|1]\n");
    return 2;
  }
#if defined(_WIN32)
  CHECK(_putenv_s("DOL_AURORA_FRAME_INTERP_PACING", mode) == 0);
#else
  CHECK((mode[0] ? setenv("DOL_AURORA_FRAME_INTERP_PACING", mode, 1)
                 : unsetenv("DOL_AURORA_FRAME_INTERP_PACING")) == 0);
#endif
  fi::set_enabled(true);
  for (int wanted = 1; wanted <= fi::kMaxSteps; ++wanted) {
    // Reset the policy through a real settings change, including wanted == 1.
    fi::set_steps(wanted == fi::kMaxSteps ? 1 : fi::kMaxSteps);
    frames(1);
    fi::set_steps(wanted);
    frames(1);
    CHECK(fi::frame_steps() == wanted && !fi::frame_skipped());
    if (mode[0] == '0') {
      frames(24, true);
      CHECK(fi::frame_steps() == wanted && !fi::frame_skipped());
      continue;
    }
    // One stall outside the eight-frame window never reduces the rate.
    frames(1, true);
    frames(8);
    CHECK(fi::frame_steps() == wanted && !fi::frame_skipped());
    frames(2, true);
    CHECK(fi::frame_steps() == wanted && !fi::frame_skipped());
    frames(1, true);
    if (wanted > 1) {
      CHECK(fi::frame_steps() == 1 && !fi::frame_skipped());
      frames(3, true);
      CHECK(fi::frame_steps() == 1 && !fi::frame_skipped());
      frames(1, true);
    }
    CHECK(fi::frame_skipped());
    frames(89);
    CHECK(fi::frame_skipped());
    frames(1);
    CHECK(fi::frame_steps() == 1 && !fi::frame_skipped());
    if (wanted > 1) {
      frames(90);
      CHECK(fi::frame_steps() == wanted && !fi::frame_skipped());
      // A quick relapse retains the existing doubled recovery interval.
      frames(3, true);
      CHECK(fi::frame_steps() == 1 && !fi::frame_skipped());
      frames(90);
      CHECK(fi::frame_steps() == 1 && !fi::frame_skipped());
      frames(90);
      CHECK(fi::frame_steps() == wanted && !fi::frame_skipped());
    }
  }
  if (!failures)
    std::puts("frame_interp_pacing_test: all 1-7 step checks passed");
  return failures ? 1 : 0;
}
