from pathlib import Path
import importlib.util,sys,os,json,hashlib,shutil,re,tarfile
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent;R=H.parents[2]
W=H/(sys.argv[1]if len(sys.argv)>1 else'cpu-attempt1')
assert not W.exists(),'Fresh attempt required'
W.mkdir();(W/'logs').mkdir();(W/'temp').mkdir()
shutil.copyfile(__file__,W/'run_cpu_frozen.py')
OLD=R/'build/performance-02-compact-vertices-20261006/run_tests_v4.py'
spec=importlib.util.spec_from_file_location('retained_cpu_tools',OLD);b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b);b.WORK=W
bank=R/'build/healing-capable-execution-inputs/inputs';tool=bank/'toolchain';system=bank/'system';lib=bank/'system-lib';compiler=tool/'bin/clang++.exe'
olddata=json.loads((OLD.parent/'test-inputs-v4.json').read_text(encoding='utf-8'))
known={};rows=[];profiles=[];failure=None
def save(name,value):(W/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
def pin(p):
    p=Path(p).resolve();r=b.rec(p);k=b.key(p)
    if k in known:assert known[k]==r,'Input drift: '+str(p)
    else:known[k]=r
    return r
def run(role,argv,env):
    i=len(rows);print(role,flush=True)
    try:r=b.hidden_run(dict(role=role,argv=[str(x)for x in argv]),i,env)
    except BaseException:
        rows.append(json.loads((W/f'row-{i}.json').read_text(encoding='utf-8')));raise
    rows.append(r);return(W/'logs'/f'{i}.log').read_text(encoding='utf-8',errors='replace')
env={k:v for k,v in os.environ.items()if k in('SYSTEMROOT','WINDIR','COMSPEC')}
env.update(PATH=str(tool/'bin')+';C:/Windows/System32',LIB=str(lib),LIBPATH=str(lib),TEMP=str(W/'temp'),TMP=str(W/'temp'),PYTHONDONTWRITEBYTECODE='1')
shared=['-fms-compatibility-version=19.44','-march=x86-64-v3','-fms-runtime-lib=dll','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt','-D_CRT_SECURE_NO_WARNINGS','-D_CRT_NONSTDC_NO_DEPRECATE','-D_USE_MATH_DEFINES','-D_DISABLE_STRING_ANNOTATION','-D_DISABLE_VECTOR_ANNOTATION','-UNDEBUG','-nostdinc','-resource-dir='+str(tool/'clang/19'),'-isystem',str(tool/'clang/19/include')]
for d in['msvc','ucrt','shared','um']:shared+=['-isystem',str(system/'include'/d)]
shared+=['-O3','-g','-gcodeview','-ffunction-sections','-fdata-sections','-Wall','-Wextra','-Werror','-Wno-unused-function','-std=c++20']
libs=[lib/n for n in['msvcrt.lib','msvcprt.lib','vcruntime.lib','ucrt.lib','oldnames.lib','kernel32.lib','uuid.lib']]
sources=[('shader',H/'tree/GXRuntime/graphics/gxcore/src/gxcore_shader.cpp',False),('uber',H/'tree/GXRuntime/graphics/gxcore/src/gxcore_uber.cpp',False),('control-shader',H/'control/src/gxcore_shader.cpp',True),('control-uber',H/'control/src/gxcore_uber.cpp',True),('fixture',H/'sparse_uniform_test.cpp',False)]
try:
    receipt=json.loads((H/'source-receipt.json').read_text(encoding='utf-8'))
    for f in receipt['files']:
        assert b.rec(f['overlay']['path'])['sha256']==f['overlay']['sha256'],'Frozen overlay drift'
    for p in[__file__,OLD,H/'source-receipt.json',H/'fixture-inputs.json',H/'staging_budget.hpp',*libs]:pin(p)
    for p in(tool/'bin').iterdir():
        if p.is_file():pin(p)
    for profile in(['o3']if'--o3-only'in sys.argv else['asan']if'--asan-only'in sys.argv else['asan','o3']):
        path=W/profile;path.mkdir();asan=profile=='asan'
        flags=shared+(['-fsanitize=address','-fno-omit-frame-pointer']if asan else[])
        commands=[]
        for name,source,control in sources:
            obj=path/(name+'.obj');dep=obj.with_suffix('.d');pre=obj.with_suffix('.pre.d')
            inc=H/'control/include'if control else H/'tree/GXRuntime/graphics/gxcore/include'
            argv=[compiler,*flags,'-I',inc,'-I',H,'-I',H/'tree']
            run(profile+'-actual-M-'+name,[*argv,'-M','-MF',pre,'-MT',obj,source],env)
            deps={b.key(p):pin(p)for p in b.parse_make_dependencies(pre.read_text(encoding='utf-8'))};assert b.key(source)in deps
            commands.append((name,source,obj,dep,pre,argv,deps))
        save(profile+'-preflight.json',dict(dependencies=list(known.values()),sources=[dict(source=str(c[1]),dependencies=list(c[6].values()))for c in commands]))
        objects=[]
        for name,source,obj,dep,pre,argv,deps in commands:
            for r in deps.values():b.check(r)
            output=run(profile+'-compile-'+name,[*argv,'-MD','-MF',dep,'-MT',obj,'-c',source,'-o',obj],env)
            assert not re.search(r'(?im)fatal error:|\berror:|LLVM ERROR',output)
            actual={b.key(p)for p in b.parse_make_dependencies(dep.read_text(encoding='utf-8'))};assert actual==set(deps),'Actual-M/MD mismatch'
            for r in deps.values():b.check(r)
            objects.append(obj);save(profile+'-'+name+'-closure.json',dict(source=pin(source),object=b.rec(obj),preflight=b.rec(pre),depfile=b.rec(dep),dependencies=list(deps.values())))
        exe=path/'sparse.exe';repro=path/'sparse-repro.tar'
        argv=[compiler,'-fuse-ld=lld-link','-fms-compatibility-version=19.44','-fms-runtime-lib=dll','-nostartfiles','-nostdlib','-resource-dir='+str(tool/'clang/19'),'-L',lib,'-Xlinker','/NODEFAULTLIB:libcmt','-Xlinker','/ENTRY:mainCRTStartup','-Xlinker','/SUBSYSTEM:CONSOLE','-Xlinker','/OPT:REF','-Xlinker','/OPT:NOICF','-Xlinker','/MANIFEST:EMBED','-Xlinker','/REPRODUCE:'+str(repro),*objects,*libs,'-o',exe]
        if asan:
            extras=[tool/'clang/19/lib/windows/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib',tool/'clang/19/lib/windows/clang_rt.asan_dynamic-x86_64.lib',Path(olddata['stl_asan'])]
            for p in extras:pin(p)
            argv[1:1]=['-shared-libasan','-fsanitize=address'];argv+=['-Xlinker','/WHOLEARCHIVE:'+str(extras[0]),'-Xlinker','/INCLUDE:__asan_seh_interceptor',extras[1],extras[2]]
        run(profile+'-link',argv,env)
        allowed={b.serial(r['path']):r for r in known.values()};allowed.update({b.serial(p):b.rec(p)for p in objects});members=[]
        with tarfile.open(repro)as tar:
            for m in tar.getmembers():
                assert m.isfile()and m.size<128*1024*1024
                payload=tar.extractfile(m).read();name=m.name.split('/',1)[1];digest=hashlib.sha256(payload).hexdigest()
                generated=name=='response.txt'or name.casefold()==b.serial(exe)+'.manifest.res'
                if not generated:assert name.casefold()in allowed and digest==allowed[name.casefold()]['sha256'],name
                members.append(dict(member=m.name,bytes=len(payload),sha256=digest,generated=generated))
        for r in olddata['runtime']:
            b.check(r);pin(r['path']);shutil.copyfile(r['path'],path/Path(r['path']).name)
        if asan:
            dll=tool/'clang/19/lib/windows/clang_rt.asan_dynamic-x86_64.dll';pin(dll);shutil.copyfile(dll,path/dll.name)
        runenv=dict(env,PATH=str(path)+';C:/Windows/System32',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1')
        for mode in ['unset','0','11','','1']:
            envcase=dict(runenv)
            if mode!='unset':envcase['DOL_GX_SPARSE_UNIFORMS']=mode
            got=run(profile+'-env-'+(mode or'empty'),[exe,'--environment-only'],envcase)
            assert got=='Sparse runtime opt-in: '+('1'if mode=='1'else'0')+'\n',got
        text=run(profile+'-fixture',[exe],runenv)
        match=re.fullmatch(r'Uniform authored bytes: control=(\d+) sparse=(\d+); aligned=(\d+)/(\d+)\nSparse uniforms: (\d+) checks, 0 failures\n',text.replace('\r\n','\n'));assert match,text
        profiles.append(dict(profile=profile,exe=b.rec(exe),reproduction=b.rec(repro),link_inputs=members,output=text,checks=int(match[5])))
    if len(profiles)==2:assert profiles[0]['output']==profiles[1]['output'],'O3/ASan exact transcript'
except BaseException as e:failure=repr(e)
preserved=True
try:
    for r in known.values():b.check(r)
except BaseException as e:preserved=False;failure=failure or repr(e)
save('result.json',dict(status='PASS_CURRENT_SPARSE_CPU_'+('_'.join(p['profile'].upper()for p in profiles))if failure is None else'FAIL_PRESERVED',failure=failure,rows=rows,profiles=profiles,inputs_preserved=preserved,pins=list(known.values()),no_GPU_or_game_execution=True))
print(json.dumps(dict(status='PASS'if failure is None else'FAIL_PRESERVED',failure=failure,rows=len(rows),profiles=[dict(profile=p['profile'],checks=p['checks'],output=p['output'])for p in profiles])))
raise SystemExit(0 if failure is None else 1)
