"""Regenerate committed original PNGs. Only this optional tool needs Pillow."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
out=Path(__file__).resolve().parents[1]/'assets/textures'
out.mkdir(parents=True,exist_ok=True)
size=512
font=ImageFont.truetype('DejaVuSans.ttf',24) if Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf').exists() else ImageFont.load_default(size=24)
im=Image.new('RGB',(size,size)); d=ImageDraw.Draw(im)
for y in range(8):
 for x in range(8):
  d.rectangle((x*64,y*64,x*64+63,y*64+63),fill=(28,110,154) if (x+y)%2 else (225,230,208))
  d.text((x*64+5,y*64+20),f'{x},{7-y}',font=font,fill=(15,25,35))
d.rectangle((0,0,511,45),fill=(245,95,45))
d.rectangle((0,466,511,511),fill=(30,70,190))
for x in [12,268]:
 d.text((x,8),'TOP / v=1',font=font,fill='white')
 d.text((x,476),'BOTTOM v=0',font=font,fill='white')
d.line((2,46,2,465),fill='red',width=6);d.line((509,46,509,465),fill='yellow',width=6)
im.save(out/'uv_grid.png')
im=Image.new('RGB',(size,size),(35,22,65)); d=ImageDraw.Draw(im)
for x in range(-size,size*2,64):d.polygon([(x,0),(x+26,0),(x+size+26,size),(x+size,size)],fill=(220,85,145))
im.save(out/'stripes.png')
im=Image.new('RGB',(size,size),(22,39,52));d=ImageDraw.Draw(im)
for y in range(0,size,128):
 for x in range(0,size,128):
  d.rounded_rectangle((x+5,y+5,x+122,y+122),radius=12,fill=(62,95,110),outline=(180,200,200),width=4)
  d.rectangle((x+24,y+56,x+104,y+72),fill=(10,20,28))
im.save(out/'panels.png')
im=Image.new('RGB',(size,size),'black');d=ImageDraw.Draw(im)
for y in range(0,size,128):
 for x in range(0,size,128):
  d.rectangle((x+8,y+8,x+60,y+120),fill='white')
im.save(out/'panels_specular.png')
im=Image.new('RGB',(size,size),'black');d=ImageDraw.Draw(im)
for y in range(0,size,128):
 for x in range(0,size,128):d.rectangle((x+24,y+56,x+104,y+72),fill=(30,220,255))
im.save(out/'panels_emissive.png')
Image.new('RGBA',(1,1),(0,0,0,255)).save(out/'black.png')
print('Generated six original PNGs in',out)
