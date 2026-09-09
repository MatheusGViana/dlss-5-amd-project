from pathlib import Path
p=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler/dlssnr/amd/AmdBridge.cpp');s=p.read_text();s=s.replace('thread_local FrameIdentity lastFrame {};','std::mutex frameMutex;\nFrameIdentity lastFrame {};').replace('thread_local UINT stableFrames = 0;','UINT stableFrames = 0;');a='''bool Before(ID3D12GraphicsCommandList* cmd, NVSDK_NGX_Parameter* params, ID3D12CommandQueue* q)
{''';assert a in s;s=s.replace(a,a+'''
    // A single backend consumes one SR stream even if the engine rotates worker threads.
    // Serialize shared settling/identity state; thread-local replacement ownership stays unchanged.
    std::lock_guard frameGuard(frameMutex);''');p.write_text(s)
