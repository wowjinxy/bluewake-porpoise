"""Bounded source-pinned D3D12 fixture. Invoke only after a build/GPU slot grant."""
from pathlib import Path
import hashlib
import json
import ntpath
import re
import subprocess
import tarfile
import time

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[2]
SDK=ROOT/'build/k7-adaptations-20261008/compact1/overlay/sdk/GXRuntime'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
norm=lambda p:ntpath.normpath(str(p)).casefold()

def rec(p):
    p=Path(p).resolve()
    return {'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)}

def require(condition,message):
    if not condition:raise RuntimeError(message)

def deps(p):
    s=Path(p).read_text(encoding='utf-8').replace('\\\n',' ').split(': ',1)[1]
    out=[];word='';i=0
    while i<len(s):
        c=s[i]
        if c=='\\' and i+1<len(s) and s[i+1] in ' \t#\\':word+=s[i+1];i+=2;continue
        if c.isspace():
            if word:out.append(word);word=''
        else:word+=c
        i+=1
    if word:out.append(word)
    return list(dict.fromkeys(out))

number=1
while (OUT/f'attempt{number}').exists():number+=1
WORK=OUT/f'attempt{number}';WORK.mkdir();(WORK/'temp').mkdir()
env=json.loads((ROOT/'build/slowdown-marker-20261007/host1/work/commands.json').read_text())['environment']
env['TEMP']=env['TMP']=str(WORK/'temp')
driver=ROOT/'build/healing-capable-execution-inputs/inputs/toolchain/bin/clang++.exe'
resource=ROOT/'build/healing-capable-execution-inputs/inputs/toolchain/clang/19'
dawn=ROOT/'build/mods-integration/app/_deps/dawn_prebuilt-src'
shader=ROOT/'ref/recompcore/GXRuntime/graphics/gxcore/src/gxcore_shader.cpp'
sources=[OUT/'compact_pixels_test.cpp',shader]
source_receipt=ROOT/'build/k7-adaptations-20261008/compact1/source-receipt.json'
frozen=json.loads(source_receipt.read_text(encoding='utf-8'))
header=SDK/'graphics/gxcore/include/gxruntime/gxcore/shader.hpp'
header_receipt=next(r for r in frozen['files'] if norm(r['candidate'])==norm(header))
require(sha(header)==header_receipt['candidate_sha256'],'Frozen candidate header')
require(sha(header_receipt['source'])==header_receipt['source_sha256'],'Preserved original header')
pins={norm(p):rec(p) for p in [driver,driver.with_name('lld-link.exe'),OUT/'prepare_fixture.py',OUT/'cases.cpp.inc',OUT/'run_fixture.py',*sources,
                               header,source_receipt,header_receipt['source'],dawn/'lib/webgpu_dawn.lib']}
rows=[]
for name in ('webgpu_dawn.dll','dxcompiler.dll','dxil.dll'):
    source=dawn/'bin'/name;pins[norm(source)]=rec(source)
    target=WORK/name;target.write_bytes(source.read_bytes());pins[norm(target)]=rec(target)
flags=['-resource-dir='+str(resource),'-fms-compatibility-version=19.44','-std=c++20','-O3','-Wall','-Wextra','-Werror',
       '-Wno-unused-function','-fms-runtime-lib=dll','-I'+str(SDK/'graphics/gxcore/include'),'-I'+str(dawn/'include')]

def run(role,args,timeout=180):
    start=time.monotonic()
    p=subprocess.run(args,cwd=WORK,env=env,creationflags=subprocess.CREATE_NO_WINDOW,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=timeout)
    log=WORK/f'{len(rows)}-{role}.log';log.write_text(p.stdout,encoding='utf-8')
    row={'role':role,'argv':args,'returncode':p.returncode,'seconds':time.monotonic()-start,'log':rec(log)};rows.append(row)
    (WORK/'partial.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
    print(role,p.returncode,p.stdout[-2500:],flush=True)
    require(p.returncode==0,role+' failed')

def check():
    for r in pins.values():require(sha(r['path'])==r['sha256'],'Input changed '+r['path'])

try:
    before={}
    for i,source in enumerate(sources):
        md=WORK/f'preflight{i}.d'
        run(f'preflight{i}',[str(driver),*flags,'-M','-MT',str(WORK/f'{i}.obj'),'-MF',str(md),str(source)])
        before[i]={norm(p):rec(p) for p in deps(md)};pins.update(before[i])
    libraries=[Path(p) for p in env['LIB'].split(';') if p]
    for name in ['msvcprt.lib','ucrt.lib','vcruntime.lib','oldnames.lib','kernel32.lib','msvcrt.lib']:
        p=next((d/name for d in libraries if (d/name).is_file()),None)
        if p:pins[norm(p)]=rec(p)
    check();(WORK/'before.json').write_text(json.dumps(list(pins.values()),indent=2)+'\n',encoding='utf-8')
    for i,source in enumerate(sources):
        md=WORK/f'{i}.d';obj=WORK/f'{i}.obj'
        run(f'compile{i}',[str(driver),*flags,'-MD','-MT',str(obj),'-MF',str(md),'-c',str(source),'-o',str(obj)])
        require({norm(p) for p in deps(md)}==set(before[i]),'Exact actual MD closure')
        check();pins[norm(obj)]=rec(obj)
    executable=WORK/'compact_pixels_test.exe'
    command=[str(driver),'-fuse-ld=lld',*flags,*[str(WORK/f'{i}.obj') for i in range(len(sources))],str(dawn/'lib/webgpu_dawn.lib'),
             '-Wl,ucrt.lib,vcruntime.lib,/NODEFAULTLIB:libucrt.lib,/NODEFAULTLIB:libcmt.lib,/NODEFAULTLIB:libvcruntime.lib',
             '-Wl,/reproduce:'+str(WORK/'link-repro.tar'),'-o',str(executable)]
    run('link',command)
    actual=[]
    with tarfile.open(WORK/'link-repro.tar') as archive:
        for member in archive:
            if not member.isfile() or member.name.endswith('/response.txt') or member.name.endswith('.manifest.res'):continue
            digest=hashlib.sha256(archive.extractfile(member).read()).hexdigest()
            matches=[r for r in pins.values() if r['sha256']==digest and r['bytes']==member.size]
            require(matches,'Undeclared actual link input '+member.name)
            actual.append({'tar':member.name,'sha256':digest,'bytes':member.size,'input':matches[0]['path']})
    check();run('GPU-pixels',[str(executable)]);check()
    shader_pairs=[]
    for full in sorted(WORK.glob('*.wgsl')):
        match=re.fullmatch(r'(.+)-full-(\d+)\.wgsl',full.name)
        if not match:continue
        selective=full.with_name(match[1]+'-selective-'+match[2]+'.wgsl')
        require(selective.is_file() and full.read_bytes()==selective.read_bytes(),'Byte-identical full/selective WGSL '+full.name)
        shader_pairs.append({'full':rec(full),'selective':rec(selective)})
    require(shader_pairs,'Actual shader pairs')
    result={'status':'PASS_OFFSCREEN_D3D12_SELECTIVE_VS_FULL_COLOR_AND_DEPTH','rows':rows,'pins':list(pins.values()),
            'actual_dependencies':{str(i):len(v) for i,v in before.items()},'actual_link_inputs':actual,
            'pixels':json.loads((WORK/'pixels.json').read_text()),'shader_pairs':shader_pairs,'executable':rec(executable),
            'game_window_surface_or_input':False,'exact_depth32float_bytes':True,
            'limits':['Synthetic GPU vertex-fetch/shader equality only; does not replace actual decoder, renderer factory, native-game or timing qualification.',
                      'Multitexmap bindings use the same authored texture image in all slots; this checks required binding layout and coordinate fetch, not different movie planes.',
                      'No early-depth multipass factory or Smooth Motion integration is executed by this standalone fixture.']}
except BaseException as error:
    result={'status':'FAILED_RETAINED','error':repr(error),'rows':rows,'pins':list(pins.values())}
    (WORK/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    raise
(WORK/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'status':result['status'],'receipt':str(WORK/'result.json'),'sha256':sha(WORK/'result.json')}),flush=True)
