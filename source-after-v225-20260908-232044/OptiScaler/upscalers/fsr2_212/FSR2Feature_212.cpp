#include <pch.h>
#include <Config.h>
#include "FSR2Feature_212.h"

double FSR2Feature212::GetDeltaTime()
{
    double currentTime = Util::MillisecondsNow();
    double deltaTime = (currentTime - _lastFrameTime);
    _lastFrameTime = currentTime;
    return deltaTime;
}

FSR2Feature212::~FSR2Feature212()
{
    if (!IsInited())
        return;

    if (!State::Instance().isShuttingDown)
    {
        auto errorCode = Fsr212::ffxFsr2ContextDestroy212(&_context);
        free(_contextDesc.callbacks.scratchBuffer);
    }

    SetInit(false);
}
