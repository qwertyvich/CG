"""Run GPU acceptance tests after a Debug build. No third-party Python packages.

--clean-implicit-layers isolates stale overlay manifests for this process tree.
Explicit Khronos validation and synchronization validation remain enabled in C++.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--clean-implicit-layers',action='store_true')
parser.add_argument('--check-baseline',action='store_true',help='Original workspace only')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
build=root/'build-vs2022'
docs=root/'docs'
env=os.environ.copy()
if args.clean_implicit_layers:
    empty=build/'empty-implicit-layers';empty.mkdir(exist_ok=True)
    env['VK_IMPLICIT_LAYER_PATH']=str(empty)
portable=build/'portable-runtime'
portable.mkdir(exist_ok=True)
shutil.copy2(build/'Debug/cg-lab4.exe',portable/'cg-lab4.exe')
shutil.copytree(build/'assets',portable/'assets',dirs_exist_ok=True)
shutil.copytree(build/'shaders',portable/'shaders',dirs_exist_ok=True)
results=[]

def run(name,argv,expected=0):
    with (docs/f'{name}-stdout.log').open('w') as out,(docs/f'{name}-stderr.log').open('w') as err:
        result=subprocess.run([str(portable/'cg-lab4.exe'),*argv],cwd=portable,env=env,
            stdout=out,stderr=err,timeout=60,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    stdout=(docs/f'{name}-stdout.log').read_text()
    stderr=(docs/f'{name}-stderr.log').read_text()
    assert result.returncode==expected,(name,result.returncode,stderr[-4000:])
    assert 'VALIDATION:' not in stderr,(name,stderr[-4000:])
    assert 'GENERAL:' not in stderr,(name,stderr[-4000:])
    results.append(f'{name}: exit={result.returncode}, validation messages=0, general messages=0')
    print(results[-1],flush=True)
    return stdout,stderr

stdout,_=run('smoke',['--smoke-test'])
assert 'validation_errors=0 general_errors=0 status=0' in stdout
phase=[line.split('phase=')[1] for line in stdout.splitlines() if 'phase=' in line]
assert len(phase)==2 and phase[0]==phase[1],phase
assert 'models=3' in stdout and 'buffers=1' in stdout and 'Floor vertices=4 indices=6' in stdout
assert 'Dynamic Rendering path=core 1.3 feature explicitly enabled' in stdout
assert 'grow SSBOs to 34' in stdout and 'remove selected spot' in stdout
for demo,expected_counts in [('directional',(60,0)),('spot',(0,60)),('combined',(60,60)),('no-shadows',(0,0)),('sampling',(60,0))]:
    stdout,_=run(f'portable-{demo}',[f'--demo={demo}','--frames=60'])
    counts=tuple(int(x) for x in re.findall(r'Shadow pass count\[\d\]=(\d+)',stdout))
    assert counts==expected_counts,(demo,counts)
    results.append(f'{demo}: shadow draws on every active frame, pass counts={counts}')

optional=[portable/'assets/textures/panels_specular.png',portable/'assets/textures/panels_emissive.png']
saved=[p.read_bytes() for p in optional]
try:
    for p in optional:p.unlink()
    stdout,_=run('optional-missing',['--demo=emission','--frames=5'])
    assert stdout.count('-> black 1x1 fallback')==2 and 'maps=3,0,0' in stdout
finally:
    for p,data in zip(optional,saved):p.write_bytes(data)
albedo=portable/'assets/textures/uv_grid.png'
saved=albedo.read_bytes()
try:
    albedo.unlink()
    _,stderr=run('albedo-missing',['--frames=2'],1)
    assert 'Invalid/missing image' in stderr
    albedo.write_bytes(b'invalid PNG')
    _,stderr=run('albedo-corrupt',['--frames=2'],1)
    assert 'Invalid/missing image' in stderr
finally:albedo.write_bytes(saved)
if args.check_baseline:
    baseline=json.loads((docs/'baseline.json').read_text(encoding='utf-8'))
    changed=[name for name,digest in baseline.items() if not (root.parent/name).is_file() or
        hashlib.sha256((root.parent/name).read_bytes()).hexdigest()!=digest]
    assert not changed,changed
    results.append(f'lab1/lab2/lab3 SHA-256 unchanged: {len(baseline)} files')
results.append(f'Explicit validation + synchronization validation enabled; clean implicit layers={args.clean_implicit_layers}')
(docs/'runtime-checks.log').write_text('\n'.join(results)+'\n',encoding='utf-8')
print('\n'.join(results[-2:]),flush=True)
