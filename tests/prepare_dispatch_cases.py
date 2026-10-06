import hashlib,json
from pathlib import Path
import sys, tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts/windows'))
import dispatch_prepare as helper
WORK=tempfile.TemporaryDirectory(prefix='dispatch-preparation-')
HERE=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(WORK.name)
HERE.mkdir(parents=True,exist_ok=True)

def source(base,cold=False,returning=False,sparse=False,count=64):
    name=f'func_{base:08X}'+('_cold'if cold else '')
    prefix='cold'if cold else 'label'
    addresses=[base+i*(16 if sparse else 4)for i in range(count)]
    start=f'void {name}(CPUState* ctx) {{\n'
    cases=''.join(f'    case 0x{a:08X}u: goto {prefix}_{a:08X};\n'for a in addresses)
    switch='    switch (ctx->pc) {\n'+cases+'    default: return;\n    }\n'
    labels=''
    for i,a in enumerate(addresses):
        cycles=1+i%5
        labels+=f'''{prefix}_{a:08X}:
    ctx->pc = 0x{a:08X}u;
    if (!dolrecomp_charge_precise(ctx, {cycles}u, 0x{a:08X}u)) return;
    if (!dolrecomp_block_can_precharge(ctx, {cycles}u)) ctx->cycle_observation_suffix ^= 0x15u;
    ctx->gpr[{i%31}] ^= 0x{(i*2654435761)&0xffffffff:08X}u;
    ctx->cr += {i+1}u;
    ctx->pc = 0x{a+4:08X}u;
    return;
'''
    if returning:
        body=start+f'    goto return_dispatch_{base:08X};\n'+labels+f'return_dispatch_{base:08X}:\n'+helper.BUDGET+'\n'+switch+'}\n'
    else:body=start+switch+labels+'}\n'
    return helper.INCLUDE+body,name,addresses

def rename(s,name,new):return s.replace(helper.INCLUDE,'').replace(name,new)

def main():
    checks=0
    def check(v):
        nonlocal checks;checks+=1;assert v
    out='';records=[]
    for kind,base,cold,ret,sparse in [('entry',0x80004000,False,False,False),('cold',0xc04200d4,True,False,False),('return',0x80008000,False,True,True)]:
        original,name,addresses=source(base,cold,ret,sparse)
        variants=[]
        for label,dense,ranges in [('original',False,False),('dense',True,False),('ranges',False,True),('both',True,True)]:
            converted,counts=helper.transform(original,dense=dense,return_ranges=ranges)
            check(counts['dense']==int(dense));check(counts['ranges']==int(ranges and ret))
            if counts['dense']or counts['ranges']:check(helper.transform(converted,dense=dense,return_ranges=ranges)[0]==converted)
            new='test_'+kind+'_'+label;variants.append(new)
            out+=rename(converted,name,new)+'\n'
        records.append(dict(kind=kind,base=base,addresses=addresses,functions=variants))
    # Actual fresh helper insertion appears in multiple independently generated
    # fixture variants; keep one definition without changing transformed bodies.
    first=True
    def remove(match):
        nonlocal first
        if first:first=False;return match[0]
        return ''
    import re
    out=re.sub(re.escape(helper.SLOT),remove,out)
    (HERE/'cases.inc').write_text(out,encoding='utf8',newline='\n')
    original,_,_=source(0x80004000,returning=True,sparse=True)
    malformed=[original.replace('    }\n','    // missing close\n',1),
      original.replace('    default: return;','    default: observe(ctx); return;'),
      original.replace('goto label_80004000;','goto label_80004004;',1),
      original.replace('    case 0x80004010u:','    case 0x80004000u:',1),
      original.replace('void func_','void unknown_',1),
      original.replace(helper.BUDGET,'    if (ctx->downcount < 0) return;'),
      original.replace('switch (ctx->pc)','switch (ctx->lr)'),
      original.replace('0x80004000u: goto label_80004000','0x80004001u: goto label_80004001'),
      original.replace('label_80004000:\n','',1),original+helper.INCLUDE,
      original.replace('\n','\r\n'),original.replace('CPUState* ctx','CPUState* unknown')]
    malformed += ['#define ctx changing_cpu()\n'+original,
                  '#define bw_dispatch_pc_slot custom_observer\n'+original,
                  helper.SLOT+original]
    for s in malformed:
        converted,counts=helper.transform(s,dense=True,return_ranges=True)
        check(converted==s);check(counts=={'dense':0,'ranges':0})
    # Existing computed-goto entries and native guarded returns are declined.
    entry,_,_=source(0x80004000)
    unknown=entry.replace('    switch (ctx->pc) {','    static void* table[64];\n    switch (ctx->pc) {')
    check(helper.transform(unknown,dense=True)[0]==unknown)
    guarded=original.replace('    case 0x80004000u: goto label_80004000;', '    case 0x80004000u:\n        if (!bw_healing_return_continue(ctx, 0x80004000u)) return;\n        goto label_80004000;')
    check(helper.transform(guarded,dense=True,return_ranges=True)[0]==guarded)
    check(helper.transform(original)[0]==original)
    # Standalone assembly probes use enough sparse cases to expose real codegen.
    asm=''
    original,name,_=source(0x80008000,returning=True,sparse=True,count=256)
    for label,dense,ranges in [('original',False,False),('dense',True,False),('ranges',False,True),('both',True,True)]:
        converted,_=helper.transform(original,dense=dense,return_ranges=ranges)
        asm+=rename(converted,name,'probe_'+label)+'\n'
    first=True;asm=re.sub(re.escape(helper.SLOT),remove,asm)
    (HERE/'assembly-cases.inc').write_text(asm,encoding='utf8',newline='\n')
    (HERE/'cases.json').write_text(json.dumps(dict(parser_checks=checks,cases=records,helper_sha256=hashlib.sha256(Path(helper.__file__).read_bytes()).hexdigest()),indent=2)+'\n')
    print(f'Exact parser: {checks} checks passed')
if __name__=='__main__':main()
