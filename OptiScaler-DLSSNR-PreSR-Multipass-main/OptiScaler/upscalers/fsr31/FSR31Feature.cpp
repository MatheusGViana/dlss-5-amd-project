#include <pch.h>
#include <Config.h>
#include "FSR31Feature.h"

double FSR31Feature::GetDeltaTime()
{
    double currentTime = Util::MillisecondsNow();
    double deltaTime = (currentTime - _lastFrameTime);
    _lastFrameTime = currentTime;
    return deltaTime;
}

FSR31Feature::FSR31Feature(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters)
    : IFeature(InHandleId, InParameters)
{
    _initParameters = SetInitParameters(InParameters);
    _lastFrameTime = Util::MillisecondsNow();
}

FSR31Feature::~FSR31Feature()
{
    if (!IsInited())
        return;

    SetInit(false);
}
