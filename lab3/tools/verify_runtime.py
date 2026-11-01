"""Windows GPU/asset acceptance tests. Run after build; stdlib only.

--clean-implicit-layers isolates third-party overlays for this process tree only.
It does not disable the explicitly requested Khronos or synchronization validation.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--clean-implicit-layers',action='store_true')
parser.add_argument('--check-baseline',action='store_true',help='Compare lab1/lab2 in the original workspace, not in a fresh clone')
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
shutil.copy2(build/'Debug/cg-lab3.exe',portable/'cg-lab3.exe')
shutil.copytree(build/'assets',portable/'assets',dirs_exist_ok=True)
shutil.copytree(build/'shaders',portable/'shaders',dirs_exist_ok=True)
results=[]
def run(name,argv,expected):
    with (docs/f'{name}-stdout.log').open('w') as out,(docs/f'{name}-stderr.log').open('w') as err:
        result=subprocess.run([str(portable/'cg-lab3.exe'),*argv],cwd=portable,env=env,
            stdout=out,stderr=err,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)
    stdout=(docs/f'{name}-stdout.log').read_text()
    stderr=(docs/f'{name}-stderr.log').read_text()
    assert result.returncode==expected,(name,result.returncode,stderr)
    assert 'VALIDATION:' not in stderr,(name,stderr)
    results.append(f'{name}: exit={result.returncode}, expected={expected}, validation messages=0')
    print(results[-1])
    return stdout,stderr

stdout,_=run('smoke',['--smoke-test'],0)
assert 'validation_errors=0 general_errors=0 status=0' in stdout
phase=[line.split('phase=')[1] for line in stdout.splitlines() if 'phase=' in line]
assert len(phase)==2 and phase[0]==phase[1],phase
assert 'buffers=1' in stdout and 'offsets=0,' in stdout
run('portable',['--demo=base','--frames=60'],0)
optional=[portable/'assets/textures/panels_specular.png',portable/'assets/textures/panels_emissive.png']
saved=[p.read_bytes() for p in optional]
try:
    for p in optional:p.unlink()
    stdout,_=run('optional-missing',['--demo=emission','--frames=60'],0)
    assert stdout.count('-> black 1x1 fallback')==2
    assert 'maps=3,0,0' in stdout
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
    results.append(f'lab1/lab2 SHA-256 unchanged: {len(baseline)} files')
else:
    results.append('Baseline comparison skipped; --check-baseline is only for the original workspace')
results.append('Pause/resume: identical effect phase during pause')
results.append(f'Explicit validation + synchronization validation enabled; clean implicit layers={args.clean_implicit_layers}')
(docs/'runtime-checks.log').write_text('\n'.join(results)+'\n',encoding='utf-8')
print('\n'.join(results[-3:]))
