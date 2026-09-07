#include <pch.h>
#include "FSR2Feature.h"

#include <State.h>

double FSR2Feature::GetDeltaTime()
{
    double currentTime = Util::MillisecondsNow();
    double deltaTime = (currentTime - _lastFrameTime);
    _lastFrameTime = currentTime;
    return deltaTime;
}

FSR2Feature::~FSR2Feature()
{
    if (!IsInited())
        return;

    if (!State::Instance().isShuttingDown)
    {
        auto errorCode = ffxFsr2ContextDestroy(&_context);
        free(_contextDesc.callbacks.scratchBuffer);
    }

    SetInit(false);
}
