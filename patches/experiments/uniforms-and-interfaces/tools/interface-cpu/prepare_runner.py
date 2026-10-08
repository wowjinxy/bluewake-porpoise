from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[2]
source=R/'build/k7-uniforms-20261008/source2/run_cpu.py'
t=source.read_text(encoding='utf-8')
def sub(a,b):
    global t
    assert t.count(a)==1,(a[:80],t.count(a));t=t.replace(a,b,1)
sub("sources=[('shader',H/'tree/GXRuntime/graphics/gxcore/src/gxcore_shader.cpp',False),('uber',H/'tree/GXRuntime/graphics/gxcore/src/gxcore_uber.cpp',False),('control-shader',H/'control/src/gxcore_shader.cpp',True),('control-uber',H/'control/src/gxcore_uber.cpp',True),('fixture',H/'sparse_uniform_test.cpp',False)]",
    "sources=[('shader',H.parent/'attempt3/tree/GXRuntime/graphics/gxcore/src/gxcore_shader.cpp',False),('control-shader',H/'control/src/gxcore_shader.cpp',True),('fixture',H/'interface_test.cpp',False)]")
sub("    receipt=json.loads((H/'source-receipt.json').read_text(encoding='utf-8'))\n    for f in receipt['files']:\n        assert b.rec(f['overlay']['path'])['sha256']==f['overlay']['sha256'],'Frozen overlay drift'\n    for p in[__file__,OLD,H/'source-receipt.json',H/'fixture-inputs.json',H/'staging_budget.hpp',*libs]:pin(p)",
    "    receipt=json.loads((H.parent/'attempt3/source-receipt.json').read_text(encoding='utf-8'))\n    for rel,row in receipt['changes'].items():\n        assert b.rec(H.parent/'attempt3/tree'/rel)['sha256']==row['after'],'Frozen interface drift'\n    for p in[__file__,OLD,H.parent/'attempt3/source-receipt.json',H/'preparation.json',*libs]:pin(p)")
sub("inc=H/'control/include'if control else H/'tree/GXRuntime/graphics/gxcore/include'", "inc=H/'control/include'if control else H.parent/'attempt3/tree/GXRuntime/graphics/gxcore/include'")
a=t.index('        for mode in [\'unset\',\'0\',\'11\',\'\',\'1\']:')
z=t.index("        profiles.append(",a)
t=t[:a]+'''        text=run(profile+'-fixture',[exe],runenv)
        match=re.fullmatch(r'Interface authored: (\\d+) cases, (\\d+) checks, 0 failures\\n',text.replace('\\r\\n','\\n'));assert match,text
'''+t[z:]
sub("checks=int(match[5])","checks=int(match[2])")
sub("PASS_CURRENT_SPARSE_CPU_","PASS_INTERFACE_CPU_")
(H/'run_cpu.py').write_text(t,encoding='utf-8',newline='\n')
print('Prepared3TU current-uniform-independent interface runner')
