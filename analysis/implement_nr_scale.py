from pathlib import Path
root=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler')
p=root/'dlssnr/amd/AmdPreSr.cpp';s=p.read_text();Path('analysis/AmdPreSr-before-scale.cpp').write_text(s)
s=s.replace('cbuffer Extent : register(b0) { uint w; uint h; };','cbuffer Extent : register(b0) { uint w; uint h; uint sourceW; uint sourceH; };',2)
s=s.replace(' if(p.x<w && p.y<h) dst[p.xy]=src.Load(int3(p.xy,0));',''' if(p.x>=w || p.y>=h)return;
 float2 uv=(float2(p.xy)+.5)*float2(sourceW,sourceH)/float2(w,h)-.5;
 int2 a=int2(floor(uv)); float2 t=frac(uv); int2 hi=int2(sourceW-1,sourceH-1);
 dst[p.xy]=lerp(lerp(src.Load(int3(clamp(a,0,hi),0)),src.Load(int3(clamp(a+int2(1,0),0,hi),0)),t.x),
 lerp(src.Load(int3(clamp(a+int2(0,1),0,hi),0)),src.Load(int3(clamp(a+1,0,hi),0)),t.x),t.y);''',1)
s=s.replace(' if(p.x<w && p.y<h) dst[p.xy]=src.Load(int3(p.xy,0));',''' if(p.x<w && p.y<h) {
 uint2 q=min(uint2((float2(p.xy)+.5)*float2(sourceW,sourceH)/float2(w,h)),uint2(sourceW-1,sourceH-1));
 dst[p.xy]=src.Load(int3(q,0)); }''',1)
shader='''constexpr char ResolveShader[] = R"(
Texture2D<float4> src:register(t0);
Texture2D<float4> baseline:register(t1);
Texture2D<float4> edited:register(t2);
RWTexture2D<float4> dst:register(u0);
cbuffer Extent:register(b0){uint w,h,lowW,lowH;};
float3 delta(int2 p){p=clamp(p,0,int2(lowW-1,lowH-1));return edited.Load(int3(p,0)).rgb-baseline.Load(int3(p,0)).rgb;}
[numthreads(8,8,1)] void main(uint3 p:SV_DispatchThreadID){
 if(p.x>=w||p.y>=h)return;
 float2 q=(float2(p.xy)+.5)*float2(lowW,lowH)/float2(w,h)-.5;
 int2 a=int2(floor(q));float2 t=frac(q);
 float3 d=lerp(lerp(delta(a),delta(a+int2(1,0)),t.x),lerp(delta(a+int2(0,1)),delta(a+1),t.x),t.y);
 float4 c=src.Load(int3(p.xy,0));dst[p.xy]=float4(clamp(c.rgb+d,0,65504),c.a);
})";
'''
s=s.replace('constexpr char DepthShader[]',shader+'constexpr char DepthShader[]')
s=s.replace('    ComPtr<ID3D12Resource> colour;','    ComPtr<ID3D12Resource> colour, scaleBaseline, scaleOutput;\n    ComPtr<ID3D12PipelineState> resolvePipeline;')
s=s.replace('D3D12_ROOT_PARAMETER params[2]','D3D12_ROOT_PARAMETER params[3]')
s=s.replace('D3D12_ROOT_SIGNATURE_DESC desc { 2, params', '''D3D12_DESCRIPTOR_RANGE residualRange { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1, 0, 0 };
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[2].DescriptorTable = { 1, &residualRange };
        D3D12_ROOT_SIGNATURE_DESC desc { 3, params''')
s=s.replace('D3D12_DESCRIPTOR_HEAP_DESC hd { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 10,','''Check(D3DCompile(ResolveShader, sizeof(ResolveShader), "AMD residual resolve", nullptr, nullptr, "main", "cs_5_0",
                         D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &error), "Resolve compile");
        ps.CS = { blob->GetBufferPointer(), blob->GetBufferSize() };
        Check(device->CreateComputePipelineState(&ps, IID_PPV_ARGS(&resolvePipeline)), "Resolve pipeline");
        D3D12_DESCRIPTOR_HEAP_DESC hd { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 14,''')
a='        const auto depthDesc = f.depth->GetDesc();'
s=s.replace(a,'''        const UINT inputW=w, inputH=h;
        const float scale=std::isfinite(cfg.modelScale)?std::clamp(cfg.modelScale,.25f,1.f):1.f;
        w=std::min(inputW,std::max(32u,UINT(std::lround(inputW*scale))));
        h=std::min(inputH,std::max(32u,UINT(std::lround(inputH*scale))));
        const bool scaled=w!=inputW||h!=inputH;
        const UINT mvW=f.motionWidth?f.motionWidth:inputW, mvH=f.motionHeight?f.motionHeight:inputH;
'''+a)
s=s.replace('const bool convertDepth = false;', '''const bool convertDepth = scaled;
        if (scaled && (depthDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
            throw std::runtime_error("NR scale: depth is not shader readable; use 100%");
        if (scaled && DepthReadFormat(depthDesc.Format)==DXGI_FORMAT_UNKNOWN)
            throw std::runtime_error("NR scale: unsupported depth view; use 100%");''')
s=s.replace('const bool resampleMotion = f.motionWidth && f.motionHeight &&\n                                    (f.motionWidth != w || f.motionHeight != h);','const bool resampleMotion = mvW != w || mvH != h;')
s=s.replace('if (resampleMotion)\n        {\n            createScratch', '''if (scaled) {
            createScratch(p->scaleBaseline,w,h,DXGI_FORMAT_R16G16B16A16_FLOAT);
            createScratch(p->scaleOutput,inputW,inputH,DXGI_FORMAT_R16G16B16A16_FLOAT);
            createScratch(p->depthCrop,w,h,DXGI_FORMAT_R32_FLOAT);
            depth=p->depthCrop.Get();
        }
        if (resampleMotion)
        {
            createScratch''',1)
s=s.replace('UINT dims[] { w, h };','UINT dims[] { w, h, inputW, inputH };').replace('SetComputeRoot32BitConstants(1, 2, dims','SetComputeRoot32BitConstants(1, 4, dims')
s=s.replace('UINT motionDims[] { w, h, f.motionWidth, f.motionHeight };','UINT motionDims[] { w, h, mvW, mvH };')
s=s.replace('cmd->SetPipelineState(p->depthPipeline.Get());','cmd->SetPipelineState(p->depthPipeline.Get());\n            cmd->SetComputeRoot32BitConstants(1,4,dims,0);')
s=s.replace('float(w) / f.motionWidth','float(w) / mvW').replace('float(h) / f.motionHeight','float(h) / mvH')
s=s.replace('        const bool settingsChanged =','''        if (scaled) copyGuide(p->colour.Get(),p->scaleBaseline.Get());
        const bool settingsChanged = cfg.modelScale != p->lastSettings.modelScale ||''')
s=s.replace('rtgiFrame.colour = finalColour;','rtgiFrame.colour = finalColour;\n                rtgiFrame.width=w;rtgiFrame.height=h;\n                rtgiFrame.depth=depth;\n                rtgiFrame.depthState=scaled?D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:f.depthState;')
s=s.replace('        p->resetAfterTimeout = false;', '''        if (scaled) {
            guideDescriptors(10,f.colour,p->scaleOutput.Get(),DXGI_FORMAT_R16G16B16A16_FLOAT);
            auto handle=p->heap->GetCPUDescriptorHandleForHeapStart();
            auto stride=p->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            handle.ptr+=12*stride;
            auto v=srv;v.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
            p->device->CreateShaderResourceView(p->scaleBaseline.Get(),&v,handle);
            handle.ptr+=stride;p->device->CreateShaderResourceView(finalColour,&v,handle);
            Barrier(cmd,f.colour,f.colourState,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            Barrier(cmd,p->scaleOutput.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            cmd->SetComputeRootSignature(p->root.Get());cmd->SetDescriptorHeaps(1,&heap);
            cmd->SetPipelineState(p->resolvePipeline.Get());
            auto table=heap->GetGPUDescriptorHandleForHeapStart();table.ptr+=10*stride;cmd->SetComputeRootDescriptorTable(0,table);
            table.ptr+=2*stride;cmd->SetComputeRootDescriptorTable(2,table);
            UINT rc[]{inputW,inputH,w,h};cmd->SetComputeRoot32BitConstants(1,4,rc,0);
            cmd->Dispatch((inputW+7)/8,(inputH+7)/8,1);
            Barrier(cmd,p->scaleOutput.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            Barrier(cmd,f.colour,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,f.colourState);
            finalColour=p->scaleOutput.Get();
        }
        p->resetAfterTimeout = false;''')
s=s.replace('guideSrv.Format = source->GetDesc().Format;','guideSrv.Format = ReadFormat(source->GetDesc().Format);')
p.write_text(s)
# Wiring
p=root/'dlssnr/amd/AmdPreSr.h';s=p.read_text().replace('    UINT passes = 1;','    float modelScale = 1;\n    UINT passes = 1;');p.write_text(s)
p=root/'Config.h';s=p.read_text().replace('    CustomOptional<bool> AmdRtgiEnabled','    CustomOptional<float> AmdNrScale { 1 };\n    CustomOptional<bool> AmdRtgiEnabled');p.write_text(s)
p=root/'Config.cpp';s=p.read_text().replace('            AmdRtgiEnabled.set_from_config','            AmdNrScale.set_from_config(readFloat("DlssNr", "AmdModelScale"));\n            AmdRtgiEnabled.set_from_config').replace('    ini.SetValue("AmdRtgi", "Enabled",','    ini.SetValue("DlssNr", "AmdModelScale", GetFloatValue(Instance()->AmdNrScale.value_for_config()).c_str());\n    ini.SetValue("AmdRtgi", "Enabled",');p.write_text(s)
p=root/'dlssnr/amd/AmdBridge.cpp';s=p.read_text().replace('    s.passes = cfg.', '    s.modelScale = cfg.AmdNrScale.value_or_default();\n    s.passes = cfg.');p.write_text(s)
p=root/'dlssnr/DlssNr_Menu.cpp';s=p.read_text();a=s.index('            bool pre =');b=s.index('            int passes',a);s=s[:a]+'''            ImGui::TextUnformatted("AMD processing: before Super Resolution");
            float scale = config->AmdNrScale.value_or_default()*100.f;
            if(ImGui::SliderFloat("NR resolution (%)",&scale,25,100,"%.0f%%")) config->AmdNrScale=scale/100.f;
'''+s[b:];s=s.replace('config->DlssNrRunBeforeSr = true;','config->DlssNrRunBeforeSr = true;\n                    config->AmdNrScale = 1.0f;');p.write_text(s)
p=root/'shaders/dlssnr/DlssNr_Dx12.cpp';s=p.read_text().replace('if (!beforeUpscale || !cfg.DlssNrRunBeforeSr.value_or_default() || forcePost)','if (!beforeUpscale || forcePost)').replace('            if (!cfg.DlssNrRunBeforeSr.value_or_default())\n                DlssNr::AmdBridge::InvalidateHistory();','            if(forcePost) ReportSkipOnce("AMD neural: native Ray Reconstruction is not supported; select Super Resolution");');p.write_text(s)
