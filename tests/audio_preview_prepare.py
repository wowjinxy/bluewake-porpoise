"""Extract the real copied-output seam for synthetic public preview tests."""
import argparse
import hashlib
import json
from pathlib import Path
import re

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--main',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
source=args.main.read_text(encoding='utf-8')
start=source.index('static bool host_audio_dma_read_guest(void* user, u32 source_address,')
opening=source.index('{',start);at=opening+1;depth=1
while depth:
    depth+=(source[at]=='{')-(source[at]=='}');at+=1
actual=source[start:at]+'\n'
matches=list(re.finditer(r'#if defined\(BLUEWAKE_WINDOWS\)\n(?:(?!#endif).)*bluewake_audio_preview_mix_be16(?:(?!#endif).)*#endif\n',actual,re.S))
if len(matches)!=1:raise ValueError('Exactly one Windows preview insertion is required')
begin,end=matches[0].span()
baseline=(actual[:begin]+actual[end:]).replace('host_audio_dma_read_guest(','host_audio_dma_read_guest_original(',1)
args.output.mkdir(parents=True,exist_ok=True)
for name,text in [('copy_original.inc',baseline),('copy_projected.inc',actual)]:
    (args.output/name).write_text(text,encoding='utf-8',newline='\n')
receipt={'main_sha256':hashlib.sha256(args.main.read_bytes()).hexdigest(),
    'actual_copy_body_sha256':hashlib.sha256(actual.encode()).hexdigest(),
    'baseline_only_removes_preview_insertion':True,'source_only':True,'private_inputs':False}
(args.output/'source.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
