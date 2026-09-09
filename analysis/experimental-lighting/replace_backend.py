from pathlib import Path
p=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler/dlssnr/amd/RtgiNative.cpp')
s=p.read_text()
start=s.index('    Texture encoded,')
end=s.index('    RtgiSettings lastSettings;', start)
s=s[:start]+'''    Texture trace, output;
    bool validHistory = false;
    UINT width = 0, height = 0, table = 0, stride;
'''+s[end:]
start=s.index('        encoded = Make(w, h);')
end=s.index('        width = w;',start)
s=s[:start]+'''        trace = Make(w, h);
        output = Make(w, h);
'''+s[end:]
start=s.index('    for (auto name : { "PrepareInputCS"')
end=s.index('    {\n        auto bytes = Read',start)
s=s[:start]+'    for (auto name : { "GatherCS", "ResolveCS" })\n'+s[end:]
start=s.index('    UINT index = 0;')
end=s.index('\nRtgiNative::~RtgiNative()',start)
s=s[:start]+'}\n'+s[end:]
start=s.index('    if (!p->uploaded)')
s=s[:start]+'''    Impl::Texture colour { frame.colour, frame.colourState }, depth { frame.depth, frame.depthState };
    struct Constants {
        UINT w, h, reverse, quality;
        float farPlane, tanHalf, thickness, fade;
        float lighting, occlusion, ambient, mix;
        UINT inspect, denoiser;
        float smoothness, pad;
    } c { frame.width, frame.height, UINT(frame.depthInverted), cfg.quality,
          cfg.farPlane, std::tan(cfg.fov * .00872664626f), cfg.thickness, cfg.fade,
          cfg.lighting, cfg.occlusion, cfg.ambient, cfg.mix, cfg.inspect, cfg.denoiser, cfg.smoothness, 0 };
    p->Run("GatherCS", c.w, c.h, 8, &c, 16,
           { { 0, { &colour } }, { 1, { &depth } } }, { { 0, { &p->trace } } });
    p->Run("ResolveCS", c.w, c.h, 8, &c, 16,
           { { 0, { &colour } }, { 1, { &depth } }, { 2, { &p->trace } } }, { { 0, { &p->output } } });
    p->State(p->output, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    p->State(colour, frame.colourState);
    p->State(depth, frame.depthState);
    return p->output.resource.Get();
}
} // namespace AmdPreSr
'''
p.write_text(s)
