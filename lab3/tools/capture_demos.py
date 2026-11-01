"""Optional Windows visual QA: captures only the app's client area; needs Pillow."""
import ctypes
from ctypes import wintypes
from pathlib import Path
import subprocess
import time
import os
import sys
from PIL import ImageGrab

root=Path(__file__).resolve().parents[1]
build=root/'build-vs2022'
out=root/'docs/screenshots'
out.mkdir(parents=True,exist_ok=True)
user32=ctypes.windll.user32
user32.SetProcessDPIAware()
user32.FindWindowW.restype=wintypes.HWND
env=os.environ.copy()
if '--clean-implicit-layers' in sys.argv:
    empty=build/'empty-implicit-layers';empty.mkdir(exist_ok=True)
    env['VK_IMPLICIT_LAYER_PATH']=str(empty)
title='Lab 3 - Texture Mapping'
for demo in ['base','sampling','emission','specular','caps','seam']:
    with (root/f'docs/{demo}-stdout.log').open('w') as stdout, (root/f'docs/{demo}-stderr.log').open('w') as stderr:
        proc=subprocess.Popen([str(build/'Debug/cg-lab3.exe'),f'--demo={demo}','--frames=1200'],cwd=build,
            stdout=stdout,stderr=stderr,env=env,creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline=time.monotonic()+20
            hwnd=0
            while not hwnd and time.monotonic()<deadline and proc.poll() is None:
                hwnd=user32.FindWindowW(None,title)
                time.sleep(.05)
            if not hwnd: raise RuntimeError(f'{demo}: no app window')
            user32.ShowWindow(hwnd,9)
            user32.SetForegroundWindow(hwnd)
            time.sleep(1)
            rect=wintypes.RECT();user32.GetClientRect(hwnd,ctypes.byref(rect))
            point=wintypes.POINT(0,0);user32.ClientToScreen(hwnd,ctypes.byref(point))
            bbox=(point.x,point.y,point.x+rect.right,point.y+rect.bottom)
            ImageGrab.grab(bbox=bbox).save(out/f'{demo}.png')
            user32.PostMessageW(hwnd,0x0010,0,0) # WM_CLOSE: normal application cleanup.
            code=proc.wait(timeout=20)
            if code: raise RuntimeError(f'{demo}: exit={code}')
            print(f'{demo}: captured {bbox}, exit={code}')
        finally:
            if proc.poll() is None:
                if hwnd:user32.PostMessageW(hwnd,0x0010,0,0)
                proc.wait(timeout=20)
