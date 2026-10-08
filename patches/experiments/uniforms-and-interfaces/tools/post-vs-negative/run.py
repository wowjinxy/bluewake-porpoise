from pathlib import Path
import importlib.util,os,sys,json,hashlib,shutil,tarfile
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent;R=H.parents[2];S=H.parent/'source2'
W=H/(sys.argv[1]if len(sys.argv)>1 else'attempt1');assert not W.exists();W.mkdir();(W/'logs').mkdir();(W/'temp').mkdir()
shutil.copyfile(__file__,W/'run_frozen.py')
OLD=R/'build/performance-02-compact-vertices-20261006/run_tests_v4.py'
spec=importlib.util.spec_from_file_location('negative_tools',OLD);b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b);b.WORK=W
prior=json.loads((S/'cpu-attempt1/result.json').read_text(encoding='utf-8'));assert prior['status']=='PASS_CURRENT_SPARSE_CPU_ASAN_O3'
known={};rows=[];profiles=[];failure=None
def pin(p):
    r=b.rec(p);k=b.key(p)
    if k in known:assert known[k]==r,'Input drift'
    else:known[k]=r
    return r
def run(role,argv,env,expected=0):
    i=len(rows);print(role,flush=True)
    try:row=b.hidden_run(dict(role=role,argv=[str(x)for x in argv]),i,env)
    except RuntimeError:
        row=json.loads((W/f'row-{i}.json').read_text(encoding='utf-8'))
        if not(expected!=0 and row['exit_code']==expected and row['error']is None and row['drained']):rows.append(row);raise
    rows.append(row);assert row['exit_code']==expected
    return(W/'logs'/f'{i}.log').read_text(encoding='utf-8')
M=next(r['argv']for r in prior['rows']if r['role']=='o3-actual-M-fixture')
C=next(r['argv']for r in prior['rows']if r['role']=='o3-compile-fixture')
L=next(r['argv']for r in prior['rows']if r['role']=='o3-link')
bank=R/'build/healing-capable-execution-inputs/inputs';tool=bank/'toolchain';lib=bank/'system-lib'
env={k:v for k,v in os.environ.items()if k in('SYSTEMROOT','WINDIR','COMSPEC')};env.update(PATH=str(tool/'bin')+';C:/Windows/System32',LIB=str(lib),LIBPATH=str(lib),TEMP=str(W/'temp'),TMP=str(W/'temp'))
try:
    for p in[__file__,OLD,H/'post_vs_failure.cpp',S/'cpu-attempt1/result.json',S/'source-receipt.json']:pin(p)
    for r in prior['pins']:b.check(r);pin(r['path'])
    for f in(S/'cpu-attempt1/o3').glob('*.obj'):pin(f)
    for mode,fixed,expected in[('bypass',False,1),('repaired',True,0)]:
        path=W/mode;path.mkdir();obj=path/'fixture.obj';pre=path/'fixture.pre.d';dep=path/'fixture.d';exe=path/'fixture.exe';repro=path/'link-repro.tar'
        def command(values):
            out=[str(x).replace(str(S/'cpu-attempt1/o3/fixture.obj'),str(obj)).replace(str(S/'cpu-attempt1/o3/fixture.pre.d'),str(pre)).replace(str(S/'cpu-attempt1/o3/fixture.d'),str(dep)).replace(str(S/'sparse_uniform_test.cpp'),str(H/'post_vs_failure.cpp'))for x in values]
            if fixed:out.insert(1,'-DQUALIFY_POST_VS_FIXED')
            return out
        run(mode+'-actual-M',command(M),env)
        deps={b.key(p):pin(p)for p in b.parse_make_dependencies(pre.read_text(encoding='utf-8'))}
        run(mode+'-compile',command(C),env)
        assert {b.key(p)for p in b.parse_make_dependencies(dep.read_text(encoding='utf-8'))}==set(deps),'Actual-M/MD mismatch'
        link=[str(x).replace(str(S/'cpu-attempt1/o3/fixture.obj'),str(obj)).replace(str(S/'cpu-attempt1/o3/sparse.exe'),str(exe)).replace(str(S/'cpu-attempt1/o3/sparse-repro.tar'),str(repro))for x in L]
        run(mode+'-link',link,env)
        allowed={b.serial(r['path']):r for r in known.values()};allowed[b.serial(obj)]=b.rec(obj);members=[]
        with tarfile.open(repro)as tar:
            for m in tar.getmembers():
                assert m.isfile()and m.size<128*1024*1024
                data=tar.extractfile(m).read();n=m.name.split('/',1)[1];digest=hashlib.sha256(data).hexdigest();generated=n=='response.txt'or n.casefold()==b.serial(exe)+'.manifest.res'
                if not generated:assert n.casefold()in allowed and digest==allowed[n.casefold()]['sha256'],n
                members.append(dict(member=m.name,bytes=len(data),sha256=digest,generated=generated))
        for dll in(S/'cpu-attempt1/o3').glob('*.dll'):pin(dll);shutil.copyfile(dll,path/dll.name)
        text=run(mode+'-fixture',[exe],dict(env,PATH=str(path)+';C:/Windows/System32'),expected)
        assert text=='PostVS A/B/fail/A: mismatched_bytes='+('0'if fixed else'5664')+', failed='+('0'if fixed else'1')+'\n',text
        profiles.append(dict(mode=mode,expected_exit=expected,output=text,object=b.rec(obj),preflight=b.rec(pre),depfile=b.rec(dep),dependencies=list(deps.values()),link_inputs=members))
except BaseException as e:failure=repr(e)
for r in known.values():b.check(r)
result=dict(status='PASS_EXPECTED_NEGATIVE_AND_REPAIRED_POST_VS'if failure is None else'FAIL_PRESERVED',failure=failure,rows=rows,profiles=profiles,pins=list(known.values()),no_GPU_or_native_execution=True)
(W/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=result['status'],failure=failure,profiles=profiles),indent=2))
raise SystemExit(0 if failure is None else 1)
