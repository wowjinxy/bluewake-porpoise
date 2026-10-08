"""Create a private main-only correctness overlay, with byte-exact inverse check."""
from pathlib import Path
import hashlib,json
OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[2]
rec=lambda p:dict(path=str(p.resolve()),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
source=ROOT/'runtime/host/src/main.c'
header=OUT/'efb_input_diagnostic.h'
original=source.read_bytes()
assert b'bw_efb_inputs' not in original
newline=b'\r\n' if b'\r\n' in original else b'\n'
replacements=[
 (b'#include "efb_peek.h"\r\n',b'#include "efb_peek.h"\r\n#include "efb_input_diagnostic.h"\r\n'),
 (b'        return argb;\r\n    }\r\n    if (g_efb_peek_enabled',
  b'        return bw_efb_inputs_value(1, size, ctx->pc, address, g_host_retrace_count, argb);\r\n    }\r\n    if (g_efb_peek_enabled'),
 (b'        return z;\r\n    }\r\n\r\n    (void)bluewake_cycle_domain_observe(',
  b'        return bw_efb_inputs_value(2, size, ctx->pc, address, g_host_retrace_count, z);\r\n    }\r\n\r\n    (void)bluewake_cycle_domain_observe('),
 (b'    if (g_ipl_sram.enabled)\r\n',b'    bw_efb_inputs_init(aurora_enabled && g_noninteractive && g_efb_peek_enabled);\r\n    if (g_ipl_sram.enabled)\r\n')]
replacements=[(before.replace(b'\r\n',newline),after.replace(b'\r\n',newline)) for before,after in replacements]
candidate=original
for before,after in replacements:
 assert candidate.count(before)==1,repr(before)
 candidate=candidate.replace(before,after)
inverse=candidate
for before,after in reversed(replacements):
 assert inverse.count(after)==1
 inverse=inverse.replace(after,before)
assert inverse==original
dest=OUT/'overlay/runtime/host/src/main.c'
dest.parent.mkdir(parents=True,exist_ok=True)
with dest.open('xb')as f:f.write(candidate)
destheader=dest.parent/header.name
with destheader.open('xb')as f:f.write(header.read_bytes())
value=dict(status='PRIVATE_EFB_CORRECTNESS_OVERLAY_NOT_BUILT',timing_eligible=False,
           inverse_byte_exact=True,preparer=rec(Path(__file__)),files=[
  dict(relative_path='runtime/host/src/main.c',original=rec(source),overlay=rec(dest)),
  dict(relative_path='runtime/host/src/efb_input_diagnostic.h',original=None,overlay=rec(destheader))])
with (OUT/'source-receipt.json').open('x',encoding='utf8')as f:json.dump(value,f,indent=2);f.write('\n')
print(json.dumps(dict(receipt=rec(OUT/'source-receipt.json'))))
