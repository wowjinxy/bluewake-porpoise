"""Source-pinned three-ABI GPU parity runner. Requires root's explicit slot grant.

Private Windows dependency recipes are references, not portable prerequisites.
Fresh attempts retain failures, actual -M/-MD closures, linker reproduction,
actual WGSL, authored output pixels, and all commands. No window/input/game.
"""
from pathlib import Path
import argparse
from collections import Counter
import hashlib
import json
import ntpath
import subprocess
import tarfile
import time

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[2]
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
norm=lambda p:ntpath.normpath(str(p)).casefold()

def require(condition,message):
    if not condition:raise RuntimeError(message)

def rec(p):
    p=Path(p).resolve()
    return {'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)}

def dependencies(p):
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

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--authorized-slot',action='store_true',required=True)
parser.add_argument('--uniform-source',type=Path,required=True,help='Frozen sourceN directory containing tree and source-receipt.json')
parser.add_argument('--uniform-receipt-sha',required=True)
parser.add_argument('--interface-source',type=Path,help='Frozen attemptN directory containing tree and source-receipt.json')
parser.add_argument('--interface-receipt-sha')
parser.add_argument('--reuse-generator-objects',type=Path,help='Retained previous attempt; never reuse fixture objects')
args=parser.parse_args()
require(args.authorized_slot,'Compiler and GPU slot must be granted by root')
require(bool(args.interface_source)==bool(args.interface_receipt_sha),'Interface path and receipt hash must accompany each other')
number=1
while (OUT/f'attempt{number}').exists():number+=1
WORK=OUT/f'attempt{number}';WORK.mkdir();(WORK/'temp').mkdir();(WORK/'inputs').mkdir()
pins={};rows=[];products={};closures={};link_inputs={};object_reuse=[]

def pin(p):
    item=rec(p);key=norm(p)
    if key in pins:require(item==pins[key],'Inconsistent input pin '+str(p))
    pins[key]=item;return item

def check():
    for item in pins.values():require(sha(item['path'])==item['sha256'],'Input changed '+item['path'])

environment_source=ROOT/'build/slowdown-marker-20261007/host1/work/commands.json'
env=json.loads(environment_source.read_text(encoding='utf-8'))['environment']
env['TEMP']=env['TMP']=str(WORK/'temp')
driver=ROOT/'build/healing-capable-execution-inputs/inputs/toolchain/bin/clang++.exe'
resource=ROOT/'build/healing-capable-execution-inputs/inputs/toolchain/clang/19'
dawn=ROOT/'build/mods-integration/app/_deps/dawn_prebuilt-src'

def run(role,argv,cwd=WORK,timeout=180):
    start=time.monotonic();log=WORK/f'{len(rows)}-{role}.log'
    try:
        p=subprocess.run(argv,cwd=cwd,env=env,creationflags=subprocess.CREATE_NO_WINDOW,
                         stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=timeout)
        output=p.stdout;code=p.returncode
    except subprocess.TimeoutExpired as error:
        output=error.stdout or ''
        if isinstance(output,bytes):output=output.decode('utf-8',errors='replace')
        log.write_text(output,encoding='utf-8')
        rows.append({'role':role,'argv':argv,'cwd':str(cwd),'timeout_seconds':timeout,'timed_out':True,'log':rec(log)})
        raise
    log.write_text(output,encoding='utf-8')
    rows.append({'role':role,'argv':argv,'cwd':str(cwd),'returncode':code,'seconds':time.monotonic()-start,'log':rec(log)})
    (WORK/'partial.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
    print(role,code,output[-1800:],flush=True)
    require(code==0,role+' failed')

def snapshot_tree(name,source,receipt_sha=None):
    source=Path(source).resolve();base=source if receipt_sha is None else source/'tree'
    if receipt_sha is not None:
        receipt=source/'source-receipt.json';require(sha(receipt)==receipt_sha,'Exact frozen '+name+' receipt');pin(receipt)
        data=json.loads(receipt.read_text(encoding='utf-8'))
        if 'files' in data:
            declared={r['relative_path']:r['overlay']['sha256'] for r in data['files']}
        else:
            declared={k:v['after'] for k,v in data['changes'].items()};declared.update(data.get('preserved',{}))
        for rel,digest in declared.items():
            p=base/rel;require(sha(p)==digest,'Frozen receipt source '+str(p));pin(p)
    target=WORK/'inputs'/name/'GXRuntime'
    for rel in ('graphics/gxcore/include/gxruntime/gxcore/shader.hpp',
                'graphics/gxcore/src/gxcore_shader.cpp','graphics/gxcore/src/gxcore_uber.cpp'):
        src=base/'GXRuntime'/rel;dest=target/rel;pin(src)
        dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(src.read_bytes());pin(dest)
    return target

def images(directory):
    out={}
    for line in (directory/'images.tsv').read_text(encoding='utf-8').splitlines():
        case,variant,filename=line.split('\t');key=(case,variant);require(key not in out,'Unique image '+str(key))
        file=directory/filename;require(file.stat().st_size==64*64*8,'RGBA8+D32 size '+str(file));out[key]=file
    return out

try:
    for p in [environment_source,driver,driver.with_name('lld-link.exe'),OUT/'prepare_fixture.py',OUT/'cases.cpp.inc',
              OUT/'run_fixture.py',OUT/'preparation.json',OUT/'uniform_pixels_test.cpp',dawn/'lib/webgpu_dawn.lib']:
        pin(p)
    preparation=json.loads((OUT/'preparation.json').read_text(encoding='utf-8'))
    require(sha(OUT/'uniform_pixels_test.cpp')==preparation['fixture']['sha256'],'Prepared fixture source')
    for r in preparation['baseline']:
        require(sha(r['snapshot'])==r['sha256'],'Frozen baseline snapshot');pin(r['snapshot'])
        require(sha(r['source'])==r['sha256'],'Preserved active source baseline');pin(r['source'])
    fixture=WORK/'inputs/uniform_pixels_test.cpp';fixture.write_bytes((OUT/'uniform_pixels_test.cpp').read_bytes());pin(fixture)
    trees={'baseline':snapshot_tree('baseline',OUT/'baseline')}
    trees['uniform']=snapshot_tree('uniform',args.uniform_source,args.uniform_receipt_sha)
    if args.interface_source:trees['interface']=snapshot_tree('interface',args.interface_source,args.interface_receipt_sha)
    runtime_sources=[dawn/'bin'/name for name in ('webgpu_dawn.dll','dxcompiler.dll','dxil.dll')]
    for p in runtime_sources:pin(p)
    libraries=[Path(p) for p in env['LIB'].split(';') if p]
    for name in ('msvcprt.lib','ucrt.lib','vcruntime.lib','oldnames.lib','kernel32.lib','msvcrt.lib'):
        lib=next((directory/name for directory in libraries if (directory/name).is_file()),None)
        require(lib is not None,'Runtime linker input '+name);pin(lib)
    flags=['-resource-dir='+str(resource),'-fms-compatibility-version=19.44','-std=c++20','-O3','-Wall','-Wextra',
           '-Werror','-Wno-unused-function','-fms-runtime-lib=dll','-I'+str(dawn/'include')]
    before={};commands={}
    for name,tree in trees.items():
        directory=WORK/name;directory.mkdir()
        mode=['-I'+str(tree/'graphics/gxcore/include')]
        if name!='baseline':mode+=['-DK7_UNIFORMS_CANDIDATE=1']
        if name=='interface':mode+=['-DK7_INTERFACES_CANDIDATE=1']
        sources=[fixture,tree/'graphics/gxcore/src/gxcore_shader.cpp',tree/'graphics/gxcore/src/gxcore_uber.cpp']
        commands[name]=(directory,mode,sources)
        for i,source in enumerate(sources):
            md=directory/f'preflight{i}.d'
            run(name+f'-preflight{i}',[str(driver),*flags,*mode,'-M','-MT',str(directory/f'{i}.obj'),'-MF',str(md),str(source)])
            before[name,i]={norm(p):rec(p) for p in dependencies(md)}
            for p in dependencies(md):pin(p)
    closures={name:{str(i):len(before[name,i]) for i in range(3)} for name in trees}
    check();(WORK/'before.json').write_text(json.dumps(list(pins.values()),indent=2)+'\n',encoding='utf-8')
    for name,(directory,mode,sources) in commands.items():
        for i,source in enumerate(sources):
            md=directory/f'{i}.d';obj=directory/f'{i}.obj'
            if args.reuse_generator_objects and i>0:
                previous=args.reuse_generator_objects.resolve();receipt=previous/'result.json';pin(receipt)
                prior=json.loads(receipt.read_text(encoding='utf-8'));old_pins={norm(r['path']):r for r in prior['pins']}
                old_obj=previous/name/f'{i}.obj';old_md=previous/name/f'{i}.d';pin(old_md)
                require(norm(old_obj) in old_pins and sha(old_obj)==old_pins[norm(old_obj)]['sha256'],'Retained exact generator object')
                old_deps={norm(p):rec(p) for p in dependencies(old_md)}
                for key,r in old_deps.items():
                    require(key in old_pins and r==old_pins[key],'Retained actual -MD dependency '+r['path']);pin(r['path'])
                fingerprints=lambda records:Counter((r['bytes'],r['sha256']) for r in records.values())
                require(fingerprints(old_deps)==fingerprints(before[name,i]),'Current -M content equals reused object actual -MD closure')
                compile_row=next(r for r in prior['rows'] if r['role']==name+f'-compile{i}')
                require(compile_row['returncode']==0,'Retained compile success')
                for flag in flags:
                    require(flag in compile_row['argv'],'Retained compiler flag '+flag)
                require(all(flag in compile_row['argv'] for flag in mode if flag.startswith('-D')),'Retained ABI mode defines')
                pin(old_obj);obj.write_bytes(old_obj.read_bytes())
                detail={'role':name+f'-reuse-generator{i}','object':rec(obj),'from':rec(old_obj),
                        'prior_receipt':rec(receipt),'actual_prior_MD':rec(old_md),
                        'prior_dependencies':list(old_deps.values()),'current_M':rec(directory/f'preflight{i}.d'),
                        'exact_dependency_content_multiset':True,'fixture_object_reused':False}
                object_reuse.append(detail);rows.append(detail);print(detail['role'],'PASS',flush=True)
            else:
                run(name+f'-compile{i}',[str(driver),*flags,*mode,'-MD','-MT',str(obj),'-MF',str(md),'-c',str(source),'-o',str(obj)])
                require({norm(p) for p in dependencies(md)}==set(before[name,i]),'Exact actual -M/-MD closure '+name+str(i))
            check();pin(obj)
        exe=directory/'uniform_pixels_test.exe'
        command=[str(driver),'-fuse-ld=lld',*flags,*mode,*[str(directory/f'{i}.obj') for i in range(len(sources))],
                 str(dawn/'lib/webgpu_dawn.lib'),'-Wl,ucrt.lib,vcruntime.lib,/NODEFAULTLIB:libucrt.lib,/NODEFAULTLIB:libcmt.lib,/NODEFAULTLIB:libvcruntime.lib',
                 '-Wl,/reproduce:'+str(directory/'link-repro.tar'),'-o',str(exe)]
        run(name+'-link',command)
        actual=[]
        with tarfile.open(directory/'link-repro.tar') as archive:
            for member in archive:
                if not member.isfile() or member.name.endswith('/response.txt') or member.name.endswith('.manifest.res'):continue
                digest=hashlib.sha256(archive.extractfile(member).read()).hexdigest()
                matches=[r for r in pins.values() if r['sha256']==digest and r['bytes']==member.size]
                require(matches,'Undeclared actual link input '+member.name)
                actual.append({'tar':member.name,'sha256':digest,'bytes':member.size,'input':matches[0]['path']})
        link_inputs[name]=actual
        for source in runtime_sources:
            target=directory/source.name;target.write_bytes(source.read_bytes());pin(target)
        check();products[name]={'executable':rec(exe)}
    print('ALL_COMPILERS_FINISHED_D3D12_CORRECTNESS_BEGIN',flush=True)
    for name,(directory,_,_) in commands.items():
        check();run(name+'-GPU',[products[name]['executable']['path']],cwd=directory,timeout=420);check()
        products[name]['pixels']=json.loads((directory/'pixels.json').read_text(encoding='utf-8'))
    baseline=images(WORK/'baseline');uniform=images(WORK/'uniform')
    cases={case for case,_ in baseline};require(len(cases)>=75,'Meaningful authored cases')
    comparison=[]
    aliases={'specialized-full':'specialized-full','specialized-selective':'specialized-selective','uber-full':'uber-full',
             'sparse-full':'specialized-full','sparse-selective':'specialized-selective','uber-sparse':'uber-full',
             'pruned-full':'specialized-full','pruned-selective':'specialized-selective',
             'pruned-sparse-full':'specialized-full','pruned-sparse-selective':'specialized-selective'}
    for name in trees:
        current=images(WORK/name);require({case for case,_ in current}==cases,'Identical case set '+name)
        for (case,variant),actual in sorted(current.items()):
            expected=baseline[case,aliases[variant]]
            a=actual.read_bytes();b=expected.read_bytes()
            if a!=b:
                diff=next(i for i,(x,y) in enumerate(zip(a,b)) if x!=y)
                raise RuntimeError(f'GPU mismatch {name}/{case}/{variant} byte{diff} actual{a[diff]} expected{b[diff]}')
            comparison.append({'source':name,'case':case,'variant':variant,'actual':rec(actual),'baseline':rec(expected)})
    shader_pairs=[]
    if 'interface' in trees:
        for source in sorted((WORK/'uniform').glob('*.wgsl')):
            peer=WORK/'interface'/source.name
            require(peer.is_file() and source.read_bytes()==peer.read_bytes(),'Interface prune=false byteexact '+source.name)
            shader_pairs.append({'uniform':rec(source),'interface_prune_false':rec(peer)})
        require(shader_pairs,'Actual interface-default WGSL comparisons')
    for name in trees:
        bindings=(WORK/name/'uniform-bindings.tsv').read_text(encoding='utf-8').splitlines()
        require(bindings,'Actual production use helper log '+name)
    check()
    closures={name:{str(i):len(before[name,i]) for i in range(3)} for name in trees}
    result={'status':'PASS_ACTUAL_OFFSCREEN_D3D12_BASELINE_UNIFORM_INTERFACE_RGBA8_AND_D32_PARITY',
            'rows':rows,'pins':list(pins.values()),'actual_dependencies':closures,'actual_link_inputs':link_inputs,
            'products':products,'image_comparisons':comparison,'interface_default_shader_pairs':shader_pairs,
            'reused_generator_objects':object_reuse,'dynamic_uniform_offsets':True,'explicit_complete_bank_layout':True,
            'source_only_authored_cases':True,'game_window_surface_or_input':False,'channel_tolerance':0,
            'poison_unstaged_bank_tails':True,'bound_bank_sizes':[656,1536,640],
            'limits':['Synthetic shader/binding/fetch equality only; no speed measurement or native gameplay qualification.',
                      'Fixed bank ranges and nonzero dynamic offsets exercise GPU bindings; production staging/deduplication/failure rollback require separate CPU/backend gates.',
                      'Uber compares against matching baseline uber; this does not certify unsupported late-Z/early-depth equivalence to specialized shaders or actual async factory transitions.',
                      'All texture map slots share one authored image, not different movie planes.',
                      'No renderer pipeline factory, early-depth multipass, Smooth Motion or presentation path runs here.']}
except BaseException as error:
    result={'status':'FAILED_RETAINED','error':repr(error),'rows':rows,'pins':list(pins.values()),
            'products':products,'actual_dependencies':closures,'actual_link_inputs':link_inputs,'reused_generator_objects':object_reuse}
    (WORK/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    raise
(WORK/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'status':result['status'],'receipt':str(WORK/'result.json'),'sha256':sha(WORK/'result.json')}),flush=True)
