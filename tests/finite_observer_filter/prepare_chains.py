"""Generate the tested chain include only when actual production main matches it."""
from pathlib import Path
import argparse,re,importlib.util
p=argparse.ArgumentParser();p.add_argument('--repo-root',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args()
def function(text,name):
    m=re.search(r'^static bool '+re.escape(name)+r'\([^\n]*\)\s*\{',text,re.M)
    if m is None:raise RuntimeError('Missing actual host observation chain')
    end=m.end();depth=1
    while depth:
        if end>=len(text):raise RuntimeError('Unterminated observation chain')
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[m.start():end]
reference=Path(__file__).with_name('chains.inc').read_text()
actual=(a.repo_root/'runtime/host/src/main.c').read_text()
spec=importlib.util.spec_from_file_location('observation_facts_prepare',Path(__file__).resolve().parents[1]/'observation_facts/prepare.py')
helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
actual=helper.legacy_observation_predicate(actual).replace('host_can_skip_observation','filtered_chain',1)
if actual!=function(reference,'filtered_chain'):
    raise RuntimeError('Actual host chain changed: refresh and review original-vs-filtered oracle before testing')
if not (a.repo_root/'runtime/host/src/finite_observer_filter.h').is_file():
    raise RuntimeError('The candidate filter is not installed in production sources')
a.output_dir.mkdir(parents=True,exist_ok=True)
(a.output_dir/'finite_observer_chains.inc').write_text(reference,encoding='utf-8',newline='\n')
