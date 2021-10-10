#!/usr/bin/env python
import sys,os
from PIL import Image

if len(sys.argv)!=2:
  raise Exception("Usage: %s INPUT"%(sys.argv[0],))
  
srcpath=sys.argv[1]
#dstpath=sys.argv[2]

image=Image.open(srcpath)
if image.width!=128 or image.height!=48:
  raise Exception("%s: Dimensions must be 128x48 (found %dx%d)"%(srcpath,image.width,image.height))
data=image.getdata()

def pixel_on(x,y):
  pixel=data.getpixel((x,y))
  if type(pixel)==int:
    return pixel
  for i in xrange(len(pixel)):
    if pixel[i]: return True
  return False

def box_is_blank(x,y,w,h):
  while h>0:
    for xi in xrange(x,x+w):
      if pixel_on(xi,y): return False
    y+=1
    h-=1
  return True

def measure_glyph(x,y):
  w=8
  h=8
  while w>0 and box_is_blank(x,y,1,h):
    x+=1
    w-=1
  while w>0 and box_is_blank(x+w-1,y,1,h):
    w-=1
  if not w: return x,y,0,0
  while h>0 and box_is_blank(x,y,w,1):
    y+=1
    h-=1
  while h>0 and box_is_blank(x,y+h-1,w,1):
    h-=1
  return x,y,w,h
  
metrics=[]
bits=[]
for ch in xrange(0x20,0x80):
  col=ch&15
  row=(ch>>4)-2
  srcx=col*8
  srcy=row*8
  x,y,w,h=measure_glyph(srcx,srcy)
  if not w or not h:
    if ch==0x20:
      metrics.append(0x60) # Space gets a width of 3
    else:
      metrics.append(0)
    bits.append(0)
    continue
  if x+w>=srcx+8 or y+h>=srcy+8:
    raise Exception("%s:%02x: Touches right or bottom margin"%(srcpath,ch))
  
  suby=y-srcy
  if suby>3: 
    h+=suby-3
    suby=3
    y=srcy+suby
  metrics.append((w<<5)|(h<<2)|suby)
  
  glyph=0
  mask=0x80000000
  for yi in xrange(h):
    for xi in xrange(w):
      if pixel_on(x+xi,y+yi):
        glyph|=mask
      mask>>=1
  bits.append(glyph)
  
sys.stdout.write("static struct softarcade_font MYFONT={\n")
sys.stdout.write("  .metrics={%s},\n"%(','.join(map(str,metrics)),))
sys.stdout.write("  .bits={%s},\n"%(','.join(map(str,bits)),))
sys.stdout.write("};\n")
