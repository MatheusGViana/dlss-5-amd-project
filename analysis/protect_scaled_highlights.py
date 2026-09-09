from pathlib import Path
p=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler/dlssnr/amd/AmdPreSr.cpp');s=p.read_text();old=''' float4 c=src.Load(int3(p.xy,0));dst[p.xy]=float4(clamp(c.rgb+d,0,65504),c.a);''';new=''' float4 c=src.Load(int3(p.xy,0));
 // A reduced neural pixel mixes surfaces and small emitters. Suppress its edit
 // where the original pixel disagrees with that footprint, rather than spreading
 // the edit blindly across high-contrast edges. No previous frame is reused.
 int2 hi=int2(lowW-1,lowH-1);
 float3 b=lerp(lerp(baseline.Load(int3(clamp(a,0,hi),0)).rgb,baseline.Load(int3(clamp(a+int2(1,0),0,hi),0)).rgb,t.x),
 lerp(baseline.Load(int3(clamp(a+int2(0,1),0,hi),0)).rgb,baseline.Load(int3(clamp(a+1,0,hi),0)).rgb,t.x),t.y);
 float3 magnitude=max(max(abs(c.rgb),abs(b)),1e-5);
 float mismatch=max(abs(c.r-b.r)/magnitude.r,max(abs(c.g-b.g)/magnitude.g,abs(c.b-b.b)/magnitude.b));
 float confidence=1-smoothstep(.15,.75,mismatch);
 // Keep extreme low-resolution edits bounded relative to the current footprint.
 float3 limit=.5*max(abs(b),abs(c.rgb));
 d=clamp(d,-limit,limit)*confidence;
 dst[p.xy]=float4(clamp(c.rgb+d,0,65504),c.a);''';assert old in s;s=s.replace(old,new);p.write_text(s)
