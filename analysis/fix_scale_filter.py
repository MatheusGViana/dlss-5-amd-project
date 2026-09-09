from pathlib import Path
p=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler/dlssnr/amd/AmdPreSr.cpp');s=p.read_text();a=s.index(' float2 uv=',s.index('constexpr char CopyShader'));b=s.index('\n})";',a)
s=s[:a]+''' if(w==sourceW && h==sourceH){dst[p.xy]=src.Load(int3(p.xy,0));return;}
 // Integrate the entire source pixel footprint. A single bilinear sample aliases
 // narrow emissive lines when the model runs far below the input resolution.
 float2 lo=float2(p.xy)*float2(sourceW,sourceH)/float2(w,h);
 float2 hi=float2(p.xy+1)*float2(sourceW,sourceH)/float2(w,h);
 int2 first=int2(floor(lo)); float4 sum=0;float total=0;
 [loop]for(int y=first.y;y<int(ceil(hi.y));++y)
 [loop]for(int x=first.x;x<int(ceil(hi.x));++x){
  float2 coverage=max(0,min(hi,float2(x+1,y+1))-max(lo,float2(x,y)));
  float weight=coverage.x*coverage.y;
  sum+=src.Load(int3(clamp(int2(x,y),0,int2(sourceW-1,sourceH-1)),0))*weight;total+=weight;
 }
 dst[p.xy]=sum/max(total,1e-6);'''+s[b:]
s=s.replace('UINT lastMotionWidth = 0, lastMotionHeight = 0;','UINT lastMotionWidth = 0, lastMotionHeight = 0;\n    UINT lastInputWidth=0,lastInputHeight=0;')
s=s.replace('p->frames == 0 || p->width != w || p->height != h','p->frames == 0 || p->lastInputWidth != w || p->lastInputHeight != h',1)
s=s.replace('        p->lastMotionWidth = f.motionWidth;','        p->lastInputWidth=inputW;p->lastInputHeight=inputH;\n        p->lastMotionWidth = f.motionWidth;')
p.write_text(s)
