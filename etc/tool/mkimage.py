#!/usr/bin/env python
import sys,os
from PIL import Image

if len(sys.argv)!=2:
  raise Exception("Usage: %s INPUT"%(sys.argv[0],))
  
srcpath=sys.argv[1]

image=Image.open(srcpath)
data=image.getdata()

colorkey=0

def convert_pixel(src):
  global colorkey
  if type(src)==int:
    # Single channel, assume it's gray in 0..255.
    r=g=b=src
  elif len(src)==2:
    if not colorkey:
      colorkey=0x1c
    if not src[1]: return colorkey
    r=g=b=src[0]
  elif len(src)==3:
    r,g,b=src
  elif len(src)==4:
    if not colorkey:
      colorkey=0x1c
    if not src[3]: return colorkey
    r,g,b,a=src
  else: raise Exception("%s: Unexpected pixel %r"%(srcpath,src))
  pixel=(b&0xe0)|((g&0xe0)>>3)|(r>>6)
  if colorkey and pixel==colorkey: return 0x18
  return pixel

name=os.path.basename(srcpath).split('.')[0]
sys.stdout.write("SOFTARCADE_IMAGE_DECLARE(%s,%d,%d,\n"%(name,image.width,image.height))
for y in xrange(image.height):
  sys.stdout.write("  ")
  for x in xrange(image.width):
    pixel=convert_pixel(data.getpixel((x,y)))
    sys.stdout.write("%d,"%(pixel,))
  sys.stdout.write("\n")
sys.stdout.write(")\n")
if colorkey:
  sys.stdout.write("image_%s.colorkey=%d;\n"%(name,colorkey,))
