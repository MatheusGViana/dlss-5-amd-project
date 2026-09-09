from pathlib import Path
r=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler')
p=r/'dlssnr/DlssNr_Menu.cpp';s=p.read_text();a='''            float scale = config->AmdNrScale.value_or_default()*100.f;
            if(ImGui::SliderFloat("NR resolution (%)",&scale,25,100,"%.0f%%")) config->AmdNrScale=scale/100.f;''';b='''            static float scale = 100.f;
            static bool editingScale = false;
            if (!editingScale) scale = config->AmdNrScale.value_or_default()*100.f;
            ImGui::SliderFloat("NR resolution (%)",&scale,25,100,"%.0f%%");
            editingScale = ImGui::IsItemActive();
            // Commit once after dragging or text entry, not one model rebuild per mouse move.
            if(ImGui::IsItemDeactivatedAfterEdit()) config->AmdNrScale=scale/100.f;''';assert a in s;s=s.replace(a,b);p.write_text(s)
p=r/'dlssnr/amd/AmdBridge.cpp';s=p.read_text();a='    const FrameIdentity current { f.colour, f.motion, f.depth, f.width, f.height };';b='''    // Let SR finish its reconfiguration before rebuilding the private HIP model.
    // Do not retain or replay the old image while input sizes are settling.
    static UINT settlingWidth=0, settlingHeight=0;
    static float settlingScale=1.f;
    static ULONGLONG settlingSince=0;
    const float requestedScale=Config::Instance()->AmdNrScale.value_or_default();
    const auto now=GetTickCount64();
    if(settlingWidth!=f.width || settlingHeight!=f.height || settlingScale!=requestedScale) {
        settlingWidth=f.width;settlingHeight=f.height;settlingScale=requestedScale;settlingSince=now;
        b->InvalidateHistory();
    }
    if(now-settlingSince<300) {
        Message("AMD neural: waiting for resolution settings to settle");
        return true;
    }
'''+a;assert a in s;s=s.replace(a,b);p.write_text(s)
p=r/'dlssnr/amd/AmdPreSr.cpp';s=p.read_text();s=s.replace('const bool guideChange = p->lastMotionWidth != f.motionWidth', 'const bool guideChange = p->lastInputWidth != inputW || p->lastInputHeight != inputH ||\n                                 p->lastMotionWidth != f.motionWidth');p.write_text(s)
p=Path('analysis/smoke.cpp');s=p.read_text();a='      if (resizeEveryFrame) {';assert a in s;s=s.replace(a,'''      if(argc>15 && _wtoi(argv[15])) {
        const float scales[]{1.f,.5f,.25f,.75f};
        settings.modelScale=scales[iteration%4];
      }
'''+a);p.write_text(s)
