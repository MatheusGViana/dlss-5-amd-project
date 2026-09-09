from pathlib import Path
p=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler/dlssnr/amd/AmdPreSr.cpp');s=p.read_text();s=s.replace('motionDesc.Format != DXGI_FORMAT_R32G32_FLOAT && motionDesc.Format != DXGI_FORMAT_R16G16_SNORM)', 'motionDesc.Format != DXGI_FORMAT_R32G32_FLOAT && motionDesc.Format != DXGI_FORMAT_R16G16_SNORM &&\n            motionDesc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT && motionDesc.Format != DXGI_FORMAT_R32G32B32A32_FLOAT)')
a='''        auto desc = f.colour->GetDesc();''';b='''        // Reject transient/dummy guides before any GPU commands or native jobs.
        // A later valid frame must be allowed to recover without restarting.
        const auto cd=f.colour->GetDesc();
        const UINT iw=f.width?f.width:UINT(cd.Width), ih=f.height?f.height:cd.Height;
        for(auto guide : {f.motion,f.depth}) {
            auto gd=guide->GetDesc();
            if(gd.Width<iw || gd.Height<ih || gd.SampleDesc.Count!=1 || gd.DepthOrArraySize!=1 ||
               gd.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
                const std::string reason="AMD neural: waiting for valid full-size guides; received "+Layout(guide);
                if(p->status!=reason)p->Log(reason);
                p->resetRequested=true;
                return nullptr;
            }
        }
'''+a;assert a in s;s=s.replace(a,b,1);p.write_text(s)
p=Path('analysis/smoke.cpp');s=p.read_text();a='    int rtgiMode =';print('')
# Reuse the actual motion upload layout, widening only where its channels are written.
print('\n'.join(x for x in s.splitlines() if 'R16G16' in x or 'kind ==' in x or 'channels' in x))
