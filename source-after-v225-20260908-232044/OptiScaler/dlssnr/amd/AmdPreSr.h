#pragma once
#include <d3d12.h>
#include <filesystem>
#include <string>

namespace AmdPreSr
{
struct Frame
{
    ID3D12Resource *colour = nullptr, *motion = nullptr, *depth = nullptr, *exposure = nullptr;
    UINT width = 0, height = 0;
    float motionScaleX = 1, motionScaleY = 1;
    // Active display extent, excluding allocation padding; zero means render-resolution vectors.
    UINT motionWidth = 0, motionHeight = 0;
    float preExposure = 1, exposureScale = 1;
    bool reset = false, depthInverted = false;
    D3D12_RESOURCE_STATES colourState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES motionState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES depthState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES exposureState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
};
struct LookSettings
{
    bool enabled = false;
    UINT appearance = 2, inspect = 0;
    bool detectSkin = true;
    float mix = 1, materialDetail = 1.15f, shapeDefinition = 1.2f, localLighting = 1.15f;
    float skinDetail = 1.1f, skinSoftness = .486f, specularControl = .58f, highlightRollOff = .9f;
    float colourSeparation = 0, shadowDepth = .2f, antiHalo = .901f, flatAreaProtection = 0;
    float tone = 0, exposureEV = 1, contrast = 1, saturation = 1, highlightCompression = 0;
};
struct RtgiSettings {
    bool enabled = false;
    UINT quality = 2, denoiser = 1, inspect = 0;
    float mix = 1, lighting = 5, occlusion = 1, ambient = 1;
    float thickness = .1f, smoothness = .5f, fade = .3f, fov = 60, farPlane = 600;
    float contact = 0, saturation = 1, radius = 1;
    bool operator==(const RtgiSettings&) const = default;
};
struct Settings
{
    UINT encoding = 0; // Auto, Linear, sRGB, Gamma 2.2
    bool toneChannels = false;
    float modelScale = 1;
    UINT passes = 1;
    float tone = 0, structure = 1, skin = 1;
    LookSettings look;
    RtgiSettings rtgi;
};
// Process lifetime owner: intentionally not destroyed/unloaded while HIP threads exist.
class Backend
{
    struct Impl;
    Impl* p;

  public:
    Backend(ID3D12Device*, ID3D12CommandQueue*, const std::filesystem::path& directory);
    // Records pre-SR work. Returns a FP16 input for the upscaler, or nullptr on skip/failure.
    ID3D12Resource* Record(ID3D12GraphicsCommandList*, const Frame&, const Settings&);
    // Bind the actual render queue BEFORE submission; do not launch GPU work yet.
    int PendingListIndex(UINT, ID3D12CommandList* const*) const;
    void Submitting(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
    // Must run immediately AFTER real queue submission, including non-upscale lists.
    void Submitted(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
    bool Ready();
    bool Shutdown(); // call before loader-lock teardown, after all submissions
    void InvalidateHistory(); // applied at the next safe recording boundary
    std::string Status() const;
    UINT64 RecordedFrames() const;
};
} // namespace AmdPreSr
