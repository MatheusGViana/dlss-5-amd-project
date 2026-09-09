from pathlib import Path
root=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler')
def edit(path, pairs):
 s=path.read_text()
 for a,b in pairs:
  assert a in s,a
  s=s.replace(a,b)
 path.write_text(s)
edit(root/'Config.h',[(f'AmdRtgi{k} {{ {a} }}',f'AmdRtgi{k} {{ {b} }}') for k,a,b in [('Lighting','1','5'),('Occlusion','5','1'),('Thickness','.25f','.1f'),('FarPlane','1000','600')]]+ [('    CustomOptional<float> AmdRtgiMix', '    CustomOptional<float> AmdRtgiContact { 0 };\n    CustomOptional<float> AmdRtgiSaturation { 1 };\n    CustomOptional<float> AmdRtgiRadius { 1 };\n    CustomOptional<float> AmdRtgiMix')])
edit(root/'dlssnr/amd/AmdPreSr.h',[('lighting = 1, occlusion = 5','lighting = 5, occlusion = 1'),('thickness = .25f','thickness = .1f'),('farPlane = 1000','farPlane = 600'),('    bool operator==(const RtgiSettings&)', '    float contact = 0, saturation = 1, radius = 1;\n    bool operator==(const RtgiSettings&)')])
for k in ['Contact','Saturation','Radius']:
 edit(root/'Config.cpp', [('            AmdRtgiMix.set_from_config',f'            AmdRtgi{k}.set_from_config(readFloat("AmdRtgi", "{k}"));\n            AmdRtgiMix.set_from_config'),('    ini.SetValue("AmdRtgi", "Mix",',f'    ini.SetValue("AmdRtgi", "{k}", GetFloatValue(Instance()->AmdRtgi{k}.value_for_config()).c_str());\n    ini.SetValue("AmdRtgi", "Mix",')])
 edit(root/'dlssnr/amd/AmdBridge.cpp',[('    s.rtgi.mix =',f'    s.rtgi.{k.lower()} = cfg.AmdRtgi{k}.value_or_default();\n    s.rtgi.mix =')])
edit(root/'dlssnr/DlssNr_Menu.cpp',[(f'AmdRtgi{k} = {a};',f'AmdRtgi{k} = {b};') for k,a,b in [('Lighting','1.0f','5.0f'),('Occlusion','5.0f','1.0f'),('Thickness','.25f','.1f'),('FarPlane','1000.0f','600.0f')]]+ [('                slider("Bounce lighting",', '                slider("Contact shading", config->AmdRtgiContact, 0, 2);\n                slider("Bounce saturation", config->AmdRtgiSaturation, 0, 2);\n                slider("Sample radius", config->AmdRtgiRadius, .25f, 3);\n                slider("Bounce lighting",'),('                    config->AmdRtgiMix =','                    config->AmdRtgiContact = 0.0f;\n                    config->AmdRtgiSaturation = 1.0f;\n                    config->AmdRtgiRadius = 1.0f;\n                    config->AmdRtgiMix =')])
edit(root/'dlssnr/amd/RtgiNative.cpp',[('rp[1].Constants = { 0, 0, 16 }','rp[1].Constants = { 0, 0, 20 }'),('cfg.mix = bound','cfg.contact = bound(cfg.contact, 0, 2, 0);\n    cfg.saturation = bound(cfg.saturation, 0, 2, 1);\n    cfg.radius = bound(cfg.radius, .25f, 3, 1);\n    cfg.mix = bound'),('0, 10, 1);','0, 10, 5);'),('cfg.occlusion, 0, 10, 5','cfg.occlusion, 0, 10, 1'),('cfg.thickness, 0, 1, .25f','cfg.thickness, 0, 1, .1f'),('100000, 1000','100000, 600'),('float smoothness, pad;','float smoothness, pad;\n        float contact, saturation, radius, pad2;'),('cfg.smoothness, 0 };','cfg.smoothness, 0, cfg.contact, cfg.saturation, cfg.radius, 0 };'),('&c, 16,','&c, 20,')])
edit(Path('analysis/experimental-lighting/Lighting.hlsl'),[('uint Inspect,Denoiser; float Smoothness,Pad;','uint Inspect,Denoiser; float Smoothness,Pad;\n float Contact,Saturation,Radius,Pad2;'),('*(0.012+0.07*Thickness)','*(0.012+0.07*Thickness)*Radius'),('&& Inspect==0','&& Contact==0 && Inspect==0'),(' float visibility=saturate', ''' // Short-range depth cavity shading. It is nondirectional, not a light-source shadow map.
 float cavity=0;
 if(Contact>0) {
  [unroll]for(int j=0;j<8;j++) {
   float angle=j*0.78539816;
   int2 q=bounded(p+int2(round(float2(cos(angle),sin(angle))*2)));
   float relative=(z-distanceAt(q))/max(z,1e-5);
   cavity+=saturate(relative*40-0.02)*saturate(1-relative*4);
  }
 }
 float contactVisibility=1-saturate(cavity*Contact/8);
 float luminance=dot(filtered.rgb,float3(0.2126,0.7152,0.0722));
 filtered.rgb=max(0,lerp(luminance.xxx,filtered.rgb,Saturation));
 float visibility=saturate'''),('base*lerp(Ambient,1,visibility)*visibility','base*lerp(Ambient,1,visibility)*visibility*contactVisibility')])
edit(Path('PACKAGE_AMD.ps1'),[('v2.8','v2.9'),('README-v28','README-v29'),('Lighting=1\nOcclusion=5','Lighting=5\nOcclusion=1'),('Thickness=0.25','Thickness=0.1'),('FarPlane=1000','FarPlane=600\nContact=0\nSaturation=1\nRadius=1')])
