from pathlib import Path
import importlib.util,sys,os,json,hashlib,shutil,re,tarfile,ntpath
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent;R=H.parents[2]
W=H/(sys.argv[1] if len(sys.argv)>1 else 'interp-attempt1')
assert not W.exists(),'Fresh attempt required'
W.mkdir();(W/'logs').mkdir();(W/'temp').mkdir()
OLD=R/'build/performance-02-compact-vertices-20261006/run_tests_v4.py'
spec=importlib.util.spec_from_file_location('retained_fixture_tools',OLD);b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b);b.WORK=W
bank=R/'build/healing-capable-execution-inputs/inputs';tool=bank/'toolchain';system=bank/'system';lib=bank/'system-lib';compiler=tool/'bin/clang++.exe'
sdk=R/'ref/recompcore/GXRuntime';overlay=H/'overlay/sdk/GXRuntime'
fixture=W/'selective_interp_test.cpp';shutil.copyfile(H/'selective_interp_test.cpp',fixture)
olddata=json.loads((OLD.parent/'test-inputs-v4.json').read_text());known={};rows=[];profiles=[];failure=None
def save(name,value): (W/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
def pin(p):
 p=Path(p).resolve();r=b.rec(p);k=b.key(p)
 if k in known: assert known[k]==r,'Input drift'
 else:known[k]=r
 return r
def run(role,argv,env):
 i=len(rows);print(role,flush=True)
 try:r=b.hidden_run(dict(role=role,argv=[str(x)for x in argv]),i,env)
 except BaseException:
  rows.append(json.loads((W/f'row-{i}.json').read_text()));raise
 rows.append(r);return (W/'logs'/f'{i}.log').read_text(errors='replace')
env={k:v for k,v in os.environ.items()if k in ('SYSTEMROOT','WINDIR','COMSPEC')};env.update(PATH=str(tool/'bin')+';C:/Windows/System32',LIB=str(lib),LIBPATH=str(lib),TEMP=str(W/'temp'),TMP=str(W/'temp'),PYTHONDONTWRITEBYTECODE='1')
shared=['-fms-compatibility-version=19.44','-march=x86-64-v3','-fms-runtime-lib=dll','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt','-D_CRT_SECURE_NO_WARNINGS','-D_CRT_NONSTDC_NO_DEPRECATE','-D_USE_MATH_DEFINES','-UNDEBUG','-nostdinc','-nostdinc++','-resource-dir='+str(tool/'clang/19'),'-isystem',str(tool/'clang/19/include')]
for d in ['msvc','ucrt','shared','um']:shared+=['-isystem',str(system/'include'/d)]
shared+=['-O3','-g','-gcodeview','-ffunction-sections','-fdata-sections']
tus=['graphics/frontend/src/render_sink.cpp','graphics/gxcore/src/gxcore.cpp','graphics/gxcore/src/gxcore_shader.cpp','graphics/gxcore/src/gxcore_uber.cpp','graphics/gxcore/src/texture_decode.cpp','graphics/gxcore/src/texture_encode.cpp','src/guest_memory.c']
libs=[lib/n for n in ['msvcrt.lib','msvcprt.lib','vcruntime.lib','ucrt.lib','oldnames.lib','kernel32.lib','uuid.lib']]+sorted((bank/'host-link').glob('*-absl_*.lib'))
try:
 for p in [__file__,OLD,H/'selective_interp_test.cpp',*libs]:pin(p)
 for p in (tool/'bin').iterdir():
  if p.is_file():pin(p)
 for profile in (['candidate-asan']if '--asan'in sys.argv else['candidate-o3']):
  path=W/profile;path.mkdir();reference=profile.startswith('reference');asan=profile.endswith('asan')
  flags=['-I',str(overlay/'graphics/gxcore/include')]+shared+json.loads((H/'interp-production-flags.json').read_text())+['-UNDEBUG','-D_DISABLE_STRING_ANNOTATION','-D_DISABLE_VECTOR_ANNOTATION']+(['-fsanitize=address','-fno-omit-frame-pointer']if asan else[])
  inc=[]
  if not reference:
   for d in ['include','graphics/frontend/include','graphics/gxcore/include']:inc+=['-I',str(overlay/d)]
  for d in ['include','graphics/frontend/include','graphics/gxcore/include']:inc+=['-I',str(sdk/d)]
  inc[:0]=['-I',str(overlay/'graphics/gxcore/include'),'-I',str(sdk/'graphics/aurora/lib/gfx')]
  sources=[overlay/'graphics/aurora/lib/gfx/frame_interp.cpp']
  sources.append(fixture);commands=[]
  for i,source in enumerate(sources):
   obj=path/(source.stem+'.obj');dep=obj.with_suffix('.d');pre=obj.with_suffix('.pre.d')
   cpp=source.suffix!='.c';argv=[str(compiler if cpp else tool/'bin/clang.exe'),*flags,*inc,'-std=gnu++20'if cpp else'-std=c17']
   if reference and source==sources[-1]:argv+=['-DCOMPACT_TEST_REFERENCE']
   run(profile+'-actual-M-'+source.stem,[*argv,'-M','-MF',pre,'-MT',obj,source],env)
   deps={b.key(p):pin(p)for p in b.parse_make_dependencies(pre.read_text())}
   assert b.key(source)in deps
   commands.append((source,obj,dep,pre,argv,deps))
  save(profile+'-preflight.json',dict(dependencies=list(known.values()),sources=[dict(source=str(c[0]),dependencies=list(c[5].values()))for c in commands]))
  objects=[]
  for source,obj,dep,pre,argv,deps in commands:
   for r in deps.values():b.check(r)
   output=run(profile+'-compile-'+source.stem,[*argv,'-MD','-MF',dep,'-MT',obj,'-c',source,'-o',obj],env)
   assert not re.search(r'(?im)fatal error:|\berror:|LLVM ERROR',output)
   actual={b.key(p)for p in b.parse_make_dependencies(dep.read_text())};assert actual==set(deps),'Actual-M/MD mismatch'
   for r in deps.values():b.check(r)
   objects.append(obj);save(profile+'-'+source.stem+'-closure.json',dict(source=pin(source),object=b.rec(obj),preflight=b.rec(pre),depfile=b.rec(dep),dependencies=list(deps.values())))
  exe=path/'decode.exe';repro=path/'decode-repro.tar'
  argv=[str(compiler),'-fuse-ld=lld-link','-fms-compatibility-version=19.44','-fms-runtime-lib=dll','-nostartfiles','-nostdlib','-resource-dir='+str(tool/'clang/19'),'-L',str(lib),'-Xlinker','/NODEFAULTLIB:libcmt','-Xlinker','/ENTRY:mainCRTStartup','-Xlinker','/SUBSYSTEM:CONSOLE','-Xlinker','/OPT:REF','-Xlinker','/OPT:NOICF','-Xlinker','/MANIFEST:EMBED','-Xlinker','/REPRODUCE:'+str(repro),*[str(p)for p in objects],*[str(p)for p in libs],'-o',str(exe)]
  if asan:
   extras=[tool/'clang/19/lib/windows/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib',tool/'clang/19/lib/windows/clang_rt.asan_dynamic-x86_64.lib',Path(olddata['stl_asan'])]
   for p in extras:pin(p)
   argv[1:1]=['-shared-libasan','-fsanitize=address'];argv+=['-Xlinker','/WHOLEARCHIVE:'+str(extras[0]),'-Xlinker','/INCLUDE:__asan_seh_interceptor',str(extras[1]),str(extras[2])]
  run(profile+'-link',argv,env)
  # Pin actual linker reproduction members, rejecting undeclared input files.
  allowed={b.serial(r['path']):r for r in known.values()};allowed.update({b.serial(p):b.rec(p)for p in objects});members=[]
  with tarfile.open(repro)as tar:
   for m in tar.getmembers():
    assert m.isfile()and m.size<128*1024*1024
    payload=tar.extractfile(m).read();name=m.name.split('/',1)[1];h=hashlib.sha256(payload).hexdigest()
    generated=name=='response.txt'or name.casefold()==b.serial(exe)+'.manifest.res'
    if not generated:assert name.casefold()in allowed and h==allowed[name.casefold()]['sha256'],name
    members.append(dict(member=m.name,bytes=len(payload),sha256=h,generated=generated))
  for r in olddata['runtime']:
   b.check(r);pin(r['path']);shutil.copyfile(r['path'],path/Path(r['path']).name)
  if asan:
   dll=tool/'clang/19/lib/windows/clang_rt.asan_dynamic-x86_64.dll';pin(dll);shutil.copyfile(dll,path/dll.name)
  outputs=[]
  for mode in ['1']:
   e=dict(env,PATH=str(path)+';C:/Windows/System32',ASAN_OPTIONS='detect_leaks=0')
   if mode!='unset':e['DOL_GXCORE_SELECTIVE_VERTICES']=mode
   text=run(profile+'-run-'+(mode or'empty'),[str(exe)],e);assert 'FAIL'not in text
   match=re.search(r'selective_interp_layout: (\d+) checks, 0 failures',text);assert match,text
   outputs.append(dict(mode=mode,output=text,cases=4097,checks=int(match[1]),digest=hashlib.sha256(text.encode()).hexdigest()))
  profiles.append(dict(profile=profile,exe=b.rec(exe),reproduction=b.rec(repro),link_inputs=members,outputs=outputs))
 assert len({x['digest']for p in profiles for x in p['outputs']})==1,'Canonical semantic digest differs'
except BaseException as e:failure=repr(e)
preserved=True
try:
 for r in known.values():b.check(r)
except BaseException as e:preserved=False;failure=failure or repr(e)
save('result.json',dict(status=('PASS_ACTUAL_INTERPOLATION_CAPTURE_ASAN'if '--asan'in sys.argv else'PASS_ACTUAL_INTERPOLATION_CAPTURE_O3')if failure is None else'FAIL_PRESERVED',failure=failure,rows=rows,profiles=profiles,inputs_preserved=preserved,pins=list(known.values()),no_GPU_or_game_execution=True))
print(json.dumps(dict(status='PASS'if failure is None else'FAIL_PRESERVED',failure=failure,rows=len(rows),profiles=[dict(profile=p['profile'],outputs=p['outputs'])for p in profiles])))
raise SystemExit(0 if failure is None else 1)
