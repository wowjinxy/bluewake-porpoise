#pragma once
#include "pc/pc_remote_body.h"
#ifdef __cplusplus
class J3DModel;
class daPy_lk_c;
bool pc_network_body_diagnostic_selected();
void pc_network_body_diagnostic_capture(J3DModel*, daPy_lk_c*);
void pc_network_body_diagnostic_before_p1();
void pc_network_body_diagnostic_frame();
#endif
