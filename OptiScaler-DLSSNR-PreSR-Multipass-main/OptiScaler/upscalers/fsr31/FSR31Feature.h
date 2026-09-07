#pragma once

#include "ffx_upscale.h"

#include <upscalers/IFeature.h>

inline static void FfxLogCallback(uint32_t type, const wchar_t* message)
{
    std::wstring string(message);
    LOG_DEBUG("FSR Runtime: {0}", wstring_to_string(string));
}

class FSR31Feature : public virtual IFeature
{
  private:
    double _lastFrameTime;
    unsigned int _lastWidth = 0;
    unsigned int _lastHeight = 0;
    static inline feature_version _version { 3, 1, 2 };

  protected:
    std::string _name = "FSR";

    ffxContext _context = nullptr;
    ffxCreateContextDescUpscale _contextDesc = {};

    virtual bool InitFSR3(const NVSDK_NGX_Parameter* InParameters) = 0;

    double GetDeltaTime();

    static inline void parse_version(const char* version_str) { _version.parse_version(version_str); }

    static inline void ffxResolveTypelessFormat(uint32_t& format)
    {
        switch (format)
        {
        case FFX_API_SURFACE_FORMAT_R10G10B10A2_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM;
            return;

        case FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT;
            return;

        case FFX_API_SURFACE_FORMAT_R16G16B16A16_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT;
            return;

        case FFX_API_SURFACE_FORMAT_R32G32_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_R32G32_FLOAT;
            return;

        case FFX_API_SURFACE_FORMAT_R8G8B8A8_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM;
            return;

        case FFX_API_SURFACE_FORMAT_B8G8R8A8_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM;
            return;

        case FFX_API_SURFACE_FORMAT_R16G16_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_R16G16_FLOAT;
            return;

        case FFX_API_SURFACE_FORMAT_R32_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_R32_FLOAT;
            return;

        case FFX_API_SURFACE_FORMAT_R8G8_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_R8G8_UNORM;
            return;

        case FFX_API_SURFACE_FORMAT_R16_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_R16_FLOAT;
            return;

        case FFX_API_SURFACE_FORMAT_R8_TYPELESS:
            format = FFX_API_SURFACE_FORMAT_R8_UNORM;
            return;

        default:
            return; // Already typed or unknown
        }
    }

    float _velocity = 1.0f;
    float _reactiveScale = 1.0f;
    float _shadingScale = 1.0f;
    float _accAddPerFrame = 0.333f;
    float _minDisOccAcc = -0.333f;

  public:
    feature_version Version() override { return _version; }

    FSR31Feature(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters);

    ~FSR31Feature();
};
