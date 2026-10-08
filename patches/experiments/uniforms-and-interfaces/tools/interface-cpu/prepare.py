from pathlib import Path
import hashlib,json,shutil
H=Path(__file__).resolve().parent;R=H.parents[2]
B=R/'build/k7-uniforms-20261008/source2/tree/GXRuntime/graphics/gxcore'
C=H.parent/'attempt3/tree/GXRuntime/graphics/gxcore'
assert not(H/'control').exists(),'Fresh independent control only'
rows=[]
for rel in ['include/gxruntime/gxcore/shader.hpp','src/gxcore_shader.cpp']:
    original=B/rel;payload=original.read_bytes();text=payload.decode('utf-8').replace('\r\n','\n')
    text=text.replace('gxruntime::gxcore','gxruntime::interface_control')
    out=H/'control'/rel;out.parent.mkdir(parents=True,exist_ok=True);out.write_text(text,encoding='utf-8',newline='\n')
    rows.append(dict(original=str(original),original_sha256=hashlib.sha256(payload).hexdigest(),control=str(out),control_sha256=hashlib.sha256(out.read_bytes()).hexdigest(),transform='namespace rename only; LF normalization'))
(H/'preparation.json').write_text(json.dumps(dict(control=rows,candidate=str(C),candidate_receipt=str(H.parent/'attempt3/source-receipt.json')),indent=2)+'\n',encoding='utf-8')
print('Prepared independent exact source2 control namespace; no compilation')
