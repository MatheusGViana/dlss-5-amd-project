from PIL import Image
from pathlib import Path
import struct
root=Path(__file__).resolve().parents[2]
for n in ['iMMERSE_bluenoise_temporal128','iMMERSE_bluenoise_temporal128_s','iMMERSE_horizonlut']:
 im=Image.open(root/'rtgi/Textures/iMMERSE'/(n+'.png')).convert('RGBA')
 if n.endswith('_s'):
  p=im.load()
  for oy in range(0,im.height,32):
   for ox in range(0,im.width,32):
    xy=[p[ox+x,oy+y][1:3] for y in range(32) for x in range(32)]
    assert len(set(xy))==1024 and all(x<32 and y<32 for x,y in xy), 'Invalid trace permutation'
 (root/'analysis/rtgi-native'/(n+'.rgba')).write_bytes(struct.pack('<II',*im.size)+im.tobytes())
 print(n,im.size)
