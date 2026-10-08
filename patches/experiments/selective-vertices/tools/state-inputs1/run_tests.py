"""Single C TU diagnostic contract tests. Run only with root compiler slot."""
from pathlib import Path
import hashlib,importlib.util,json,os,shutil,subprocess,sys,tarfile
OUT=Path(__file__).resolve().parent;ROOT=OUT.parents[2]
WORK=OUT/(sys.argv[1] if len(sys.argv)>1 else 'tests1')
assert not WORK.exists(),'Fresh attempt only'
WORK.mkdir();(WORK/'temp').mkdir();(WORK/'logs').mkdir()
helper=ROOT/'build/k7-adaptations-20261008/build1/prepare.py'
spec=importlib.util.spec_from_file_location('build_helpers',helper)
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
bank=ROOT/'build/healing-capable-execution-inputs/inputs'
tool=bank/'toolchain';lib=bank/'system-lib';compiler=tool/'bin/clang.exe'
pins={};rows=[];checks=[]
def pin(p):
 r=b.rec(p);key=b.key(r['path'])
 assert key not in pins or pins[key]==r,'Input drift'
 pins[key]=r;return r
def run(role,args,env,expected=0):
 for r in pins.values():b.check(r)
 r=subprocess.run([str(x)for x in args],cwd=WORK,env=env,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=60)
 row=dict(role=role,argv=[str(x)for x in args],returncode=r.returncode,stdout=r.stdout.decode(errors='replace'),stderr=r.stderr.decode(errors='replace'))
 rows.append(row);(WORK/'logs'/f'{len(rows):03}.json').write_text(json.dumps(row,indent=2)+'\n')
 assert r.returncode==expected,row
 return row
env={k:v for k,v in os.environ.items()if k in ('SYSTEMROOT','WINDIR','COMSPEC')}
env.update(PATH=str(tool/'bin')+';C:/Windows/System32',LIB=str(lib),LIBPATH=str(lib),TEMP=str(WORK/'temp'),TMP=str(WORK/'temp'))
source=OUT/'fixture.c';obj=WORK/'fixture.obj';pre=WORK/'fixture.pre.d';dep=WORK/'fixture.d';exe=WORK/'fixture.exe';repro=WORK/'link-repro.tar'
flags=['-std=c17','-O2','-fms-compatibility-version=19.44','-fms-runtime-lib=dll','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt','-D_CRT_SECURE_NO_WARNINGS','-nostdinc','-resource-dir='+str(tool/'clang/19'),'-isystem',str(tool/'clang/19/include')]
for part in ['msvc','ucrt','shared','um']:flags+=['-isystem',str(bank/'system/include'/part)]
libs=[lib/name for name in ['msvcrt.lib','vcruntime.lib','ucrt.lib','oldnames.lib','kernel32.lib','uuid.lib']]
failure=None
try:
 for p in [Path(__file__),helper,source,OUT/'efb_input_diagnostic.h',*libs]:pin(p)
 for p in (tool/'bin').iterdir():
  if p.is_file():pin(p)
 run('actual-M',[compiler,*flags,'-M','-MF',pre,'-MT',obj,source],env)
 closure={b.key(p):pin(p)for p in b.parse_make_dependencies(pre.read_text())}
 run('compile',[compiler,*flags,'-MD','-MF',dep,'-MT',obj,'-c',source,'-o',obj],env)
 assert {b.key(p)for p in b.parse_make_dependencies(dep.read_text())}==set(closure),'M/MD mismatch'
 run('link',[compiler,'-fuse-ld=lld-link','-fms-runtime-lib=dll','-nostartfiles','-nostdlib','-resource-dir='+str(tool/'clang/19'),'-L',lib,'-Xlinker','/ENTRY:mainCRTStartup','-Xlinker','/SUBSYSTEM:CONSOLE','-Xlinker','/MANIFEST:EMBED','-Xlinker','/REPRODUCE:'+str(repro),obj,*libs,'-o',exe],env)
 known={b.serial(r['path']):r for r in pins.values()};known[b.serial(obj)]=b.rec(obj);members=[]
 with tarfile.open(repro)as tar:
  for m in tar.getmembers():
   assert m.isfile() and m.size<128*1024*1024
   data=tar.extractfile(m).read();name=m.name.split('/',1)[1];digest=hashlib.sha256(data).hexdigest()
   generated=name=='response.txt' or name.casefold()==b.serial(exe)+'.manifest.res'
   if not generated:assert name.casefold()in known and known[name.casefold()]['sha256']==digest,name
   members.append(dict(member=m.name,sha256=digest,bytes=len(data),generated=generated))
 runtime=json.loads((ROOT/'build/performance-02-compact-vertices-20261006/test-inputs-v4.json').read_text())['runtime']
 for r in runtime:
  b.check(r);pin(r['path']);shutil.copyfile(r['path'],WORK/Path(r['path']).name)
 env['PATH']=str(WORK)+';C:/Windows/System32'
 def case(name,record=None,replay=None,count=4,variation=0,allowed=1,rc=0):
  e=dict(env)
  if record is not None:e['BLUEWAKE_EFB_INPUT_RECORD']=str(record)
  if replay is not None:e['BLUEWAKE_EFB_INPUT_REPLAY']=str(replay)
  row=run(name,[exe,allowed,count,variation],e,rc);checks.append(dict(name=name,expected=rc,actual=row['returncode']));return row
 off=case('off-returns-physical')['stdout']
 trace=WORK/'normal.bin';record=case('record',record=trace)
 data=trace.read_bytes();assert len(data)==16+32*4 and record['stdout']==off
 case('exclusive-output-refusal',record=trace,rc=90)
 assert trace.read_bytes()==data
 assert case('replay',replay=trace)['stdout']==off
 varied=case('replay-physical-differences',replay=trace,variation=1)
 assert varied['stdout']==off and 'physical_differences=4' in varied['stderr']
 case('unconsumed',replay=trace,count=3,rc=90)
 case('exhausted',replay=trace,count=5,rc=90)
 case('disallowed-context',replay=trace,allowed=0,rc=90)
 case('conflicting-options',record=WORK/'never.bin',replay=trace,rc=90)
 assert not (WORK/'never.bin').exists()
 case('empty-record',record='',rc=90)
 case('empty-replay',replay='',rc=90)
 for name,offset,byte in [('magic',0,0),('entry-size',8,31),('flags',12,1),('seq',16,1),('retrace',24,0),('pc',32,1),('address',36,1),('kind',44,2),('size',45,1),('reserved',46,1),('bad-depth',16+32+27,0xFF)]:
  bad=bytearray(data);assert bad[offset]!=byte;bad[offset]=byte;p=WORK/(name+'.bin');p.write_bytes(bad);case(name,replay=p,rc=90)
 for name,payload in [('truncated-header',data[:15]),('truncated-entry',data[:-1]),('trailing-byte',data+b'X')]:
  p=WORK/(name+'.bin');p.write_bytes(payload);case(name,replay=p,rc=90)
 assert trace.read_bytes()==data
except BaseException as e:failure=repr(e)
for r in pins.values():b.check(r)
result=dict(status='PASS_PRIVATE_EFB_INPUT_CONTRACT'if failure is None else'FAIL_PRESERVED',failure=failure,checks=checks,pins=list(pins.values()),rows=rows,timing_eligible=False)
if failure is None:result.update(object=b.rec(obj),executable=b.rec(exe),link_reproduction=b.rec(repro),link_inputs=members,source_closure=list(closure.values()))
(WORK/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(status=result['status'],failure=failure,checks=len(checks))))
raise SystemExit(0 if failure is None else 1)
