"""Capture client areas of the actual applications in isolated runtime folders."""
from pathlib import Path
import ctypes,os,subprocess,time,json,shutil,argparse
from ctypes import wintypes
from PIL import ImageGrab,ImageStat

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'reports/screenshots';OUT.mkdir(parents=True,exist_ok=True)
TMP=ROOT/'reports/tmp';TMP.mkdir(exist_ok=True)
u=ctypes.windll.user32;u.SetProcessDPIAware();u.FindWindowW.restype=wintypes.HWND
empty=TMP/'empty-implicit-layers';empty.mkdir(exist_ok=True)
env=os.environ.copy();env['VK_IMPLICIT_LAYER_PATH']=str(empty)
records=[]
parser=argparse.ArgumentParser();parser.add_argument('--labs',nargs='+',default=['lab1','lab2','lab3','lab4'])
options=parser.parse_args()

def run(lab,exe,build,title,demos):
    if lab not in options.labs:return
    runtime=TMP/f'runtime-{lab}';runtime.mkdir(exist_ok=True)
    shutil.copy2(ROOT/build/exe,runtime/Path(exe).name)
    source=ROOT/build/'shaders'
    if not source.exists():source=ROOT/lab/'shaders'
    shutil.copytree(source,runtime/'shaders',dirs_exist_ok=True)
    if (ROOT/build/'assets').exists():shutil.copytree(ROOT/build/'assets',runtime/'assets',dirs_exist_ok=True)
    for name,args,clicks in demos:
        with (TMP/f'{name}-stdout.log').open('w') as out,(TMP/f'{name}-stderr.log').open('w') as err:
            p=subprocess.Popen([str(runtime/Path(exe).name),*args],cwd=runtime,env=env,stdout=out,stderr=err,creationflags=subprocess.CREATE_NO_WINDOW)
            hwnd=0
            try:
                deadline=time.monotonic()+30
                while not hwnd and time.monotonic()<deadline and p.poll() is None:
                    hwnd=u.FindWindowW(None,title);time.sleep(.1)
                if not hwnd:raise RuntimeError(f'{name}: no window')
                u.ShowWindow(hwnd,9);u.SetForegroundWindow(hwnd)
                rect=wintypes.RECT();u.GetClientRect(hwnd,ctypes.byref(rect));point=wintypes.POINT();u.ClientToScreen(hwnd,ctypes.byref(point))
                bbox=(point.x,point.y,point.x+rect.right,point.y+rect.bottom)
                while True:
                    im=ImageGrab.grab(bbox=bbox)
                    if max(im.getpixel((im.width-10,10))[:3])<120 and max(ImageStat.Stat(im).stddev)>15:break
                    if time.monotonic()>deadline:raise RuntimeError(f'{name}: no rendered content')
                    time.sleep(.15)
                time.sleep(.5)
                for x,y in clicks:
                    u.SetCursorPos(point.x+x,point.y+y);time.sleep(.2);u.mouse_event(2,0,0,0,0);time.sleep(.15);u.mouse_event(4,0,0,0,0);time.sleep(.8)
                ImageGrab.grab(bbox=bbox).save(OUT/f'{name}.png')
            finally:
                if p.poll() is None:
                    if hwnd:u.PostMessageW(hwnd,0x0010,0,0)
                    p.wait(timeout=20)
        stderr=(TMP/f'{name}-stderr.log').read_text(errors='replace')
        assert p.returncode==0,(name,p.returncode,stderr[-1200:])
        assert 'VALIDATION:' not in stderr,(name,stderr[-1200:])
        records.append({'image':f'{name}.png','application':lab,'exit_code':p.returncode,'args':args,'clicks':clicks,'validation_messages':stderr.count('VALIDATION:')})
        print(name,'captured; exit=0',flush=True)

run('lab1','Debug/vulkan-starter-app.exe','lab1/build-debug','Vulkan Starter App',[
    ('lab1-perspective',[],[]),('lab1-orthographic',[],[(143,85)])])
run('lab2','Debug/cg-lab2.exe','lab2/build-vs2022','Lab 2 - Camera and Lighting',[
    ('lab2-lighting',[],[]),('lab2-nohelpers',[],[(31,551),(75,574)])])
run('lab3','Debug/cg-lab3.exe','lab3/build-vs2022','Lab 3 - Texture Mapping',[
    ('lab3-textures',['--demo=base'],[]),('lab3-emissive',['--demo=emission'],[])])
run('lab4','Debug/cg-lab4.exe','lab4/build-vs2022','Lab 4 - Shadow Mapping',[
    ('lab4-directional',['--demo=directional'],[]),('lab4-combined',['--demo=combined'],[])])
manifest=ROOT/'reports/capture_manifest.json'
previous=json.loads(manifest.read_text(encoding='utf-8')) if manifest.exists() else []
records=[r for r in previous if r['application'] not in options.labs]+records
records.sort(key=lambda r:r['image'])
manifest.write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
