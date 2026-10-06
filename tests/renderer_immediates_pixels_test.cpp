// SPDX-License-Identifier: GPL-3.0-or-later
// Run the actual lighting/FIFO pixel oracle and record which device path ran.
#include "gxruntime/aurora_backend.h"
#include "gfx/common.hpp"
#include <cstdio>

static bool initialize_with_immediate_receipt(int argc, char** argv,
                                             const AuroraBackendConfig* config) {
  const bool ok = dol_aurora_initialize(argc, argv, config);
  if (ok) {
    std::printf("GPU immediate constants: requested=%u active=%u\n",
                unsigned(aurora::gfx::immediate_constants_requested()),
                unsigned(aurora::gfx::immediate_constants()));
    std::fflush(stdout);
  }
  return ok;
}

#define dol_aurora_initialize initialize_with_immediate_receipt
#include "renderer_lighting_pixels_test.cpp"
#undef dol_aurora_initialize
