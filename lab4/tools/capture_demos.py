"""Capture real Windows client areas and compare shadow toggles. Requires Pillow.

Run from any directory after the Debug build. Captures are evidence from the
running Vulkan app; no images are synthesized. Each app closes via WM_CLOSE.
"""
from pathlib import Path
import argparse
import ctypes
from ctypes import wintypes
import os
import subprocess
import time
from PIL import ImageGrab,ImageChops,ImageStat

parser=argparse.ArgumentParser()
parser.add_argument('--clean-implicit-layers',action='store_true')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
build=root/'build-vs2022'
out=root/'docs/screenshots';out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
if args.clean_implicit_layers:
    empty=build/'empty-implicit-layers';empty.mkdir(exist_ok=True)
    env['VK_IMPLICIT_LAYER_PATH']=str(empty)
user32=ctypes.windll.user32
user32.SetProcessDPIAware()
user32.FindWindowW.restype=wintypes.HWND
title='Lab 4 - Shadow Mapping'
demos=['directional','no-shadows','spot','spot-no-shadows','combined','combined-no-shadows',
       'sampling-static','sampling-static-no-shadows','model-shift','camera-shift','sun-shift',
       'spot-shift','small-coverage','zero-bias','large-bias','emission']
results=[]
for demo in demos:
    with (root/f'docs/capture-{demo}-stdout.log').open('w') as stdout,(root/f'docs/capture-{demo}-stderr.log').open('w') as stderr:
        proc=subprocess.Popen([str(build/'Debug/cg-lab4.exe'),f'--demo={demo}'],cwd=build,
            stdout=stdout,stderr=stderr,env=env,creationflags=subprocess.CREATE_NO_WINDOW)
        hwnd=0
        try:
            deadline=time.monotonic()+20
            while not hwnd and time.monotonic()<deadline and proc.poll() is None:
                hwnd=user32.FindWindowW(None,title);time.sleep(.05)
            if not hwnd:raise RuntimeError(f'{demo}: no app window')
            user32.ShowWindow(hwnd,9);user32.SetForegroundWindow(hwnd)
            time.sleep(1.2)
            rect=wintypes.RECT();user32.GetClientRect(hwnd,ctypes.byref(rect))
            point=wintypes.POINT();user32.ClientToScreen(hwnd,ctypes.byref(point))
            bbox=(point.x,point.y,point.x+rect.right,point.y+rect.bottom)
            # A window exists before shader/pipeline initialization completes.
            # Wait for a rendered navy background instead of saving the white startup surface.
            deadline=time.monotonic()+20
            while True:
                capture=ImageGrab.grab(bbox=bbox)
                if max(capture.getpixel((1270,10))[:3])<100 and max(ImageStat.Stat(capture).stddev)>15:
                    break
                if proc.poll() is not None or time.monotonic()>deadline:
                    raise RuntimeError(f'{demo}: no rendered frame before timeout')
                time.sleep(.15)
            assert capture.size==(1280,720),(demo,capture.size)
            capture.save(out/f'{demo}.png')
        finally:
            if proc.poll() is None:
                if hwnd:user32.PostMessageW(hwnd,0x0010,0,0)
                proc.wait(timeout=20)
        assert proc.returncode==0,(demo,proc.returncode)
    stderr=(root/f'docs/capture-{demo}-stderr.log').read_text()
    assert 'VALIDATION:' not in stderr and 'GENERAL:' not in stderr,(demo,stderr[-2000:])
    results.append(f'{demo}: real client capture, exit=0, validation/general messages=0')
    print(results[-1],flush=True)

from PIL import Image
def pixels(im):
    return im.get_flattened_data() if hasattr(im,'get_flattened_data') else im.getdata()
for on,off in [('directional','no-shadows'),('spot','spot-no-shadows'),('combined','combined-no-shadows'),('sampling-static','sampling-static-no-shadows')]:
    # Exclude all UI. Equal camera/material/light states differ only by shadow toggles.
    a=Image.open(out/f'{on}.png').convert('RGB').crop((420,0,1280,720))
    b=Image.open(out/f'{off}.png').convert('RGB').crop((420,0,1280,720))
    diff=ImageChops.difference(a,b)
    changed=sum(max(p)>12 for p in pixels(diff))
    assert changed>1000,(on,'shadow toggle made too little difference',changed)
    # Turning on visibility may only reduce direct lighting in a static A/B pair.
    brighter=sum(max(x-y for x,y in zip(p,q))>3 for p,q in zip(pixels(a),pixels(b)))
    assert brighter<100,(on,'unexpected brightening',brighter)
    results.append(f'{on} vs {off}: {changed} changed scene pixels (>12 levels), {brighter} unexpectedly brighter pixels')
(root/'docs/visual-checks.log').write_text('\n'.join(results)+'\n',encoding='utf-8')
print('\n'.join(results[-4:]),flush=True)
