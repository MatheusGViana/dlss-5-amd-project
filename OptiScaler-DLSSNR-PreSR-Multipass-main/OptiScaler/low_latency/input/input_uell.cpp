#include "pch.h"
#include "input_uell.h"

void InputUeLowLatency::init()
{
    if (inited)
        return;

    // if (auto dx11device = State::Instance().currentD3D11Device; dx11device)
    //{
    //     device = dx11device;
    //     inited = true;
    // }

    if (auto dx12device = State::Instance().currentD3D12Device; !inited && dx12device)
    {
        device = dx12device;
        inited = true;
    }

    if (inited)
    {
        LOG_DEBUG("Inited UeLowLatency");

        // TODO: not fully filled out
        SleepMode sleepMode {};
        sleepMode.low_latency_enabled = true;
        sleepMode.minimum_interval_us = 0;

        auto result = InputCommon::set_sleep_mode(inputContext, device, &sleepMode);

        if (result == InputResult::Ok || result == InputResult::UsingDifferentInput)
            return;
        else
            LOG_ERROR("set_sleep_mode result: {}", magic_enum::enum_name(result));
    }
}

void InputUeLowLatency::tickStart(int64_t frameId, float DeltaSeconds, bool bIdleMode)
{
    if (!inited)
        InputUeLowLatency::init();

    auto result = InputCommon::sleep(inputContext, device, frameId);

    if (result == InputResult::UsingDifferentInput)
        return;

    if (result != InputResult::Ok)
    {
        LOG_ERROR("sleep result: {}", magic_enum::enum_name(result));
        return;
    }

    MarkerParams markerParams {};
    markerParams.frame_id = frameId;
    markerParams.marker_type = MarkerType::SIMULATION_START;

    result = InputCommon::set_marker(inputContext, device, markerParams);

    if (result != InputResult::Ok)
    {
        LOG_ERROR("set_marker result: {}", magic_enum::enum_name(result));
        return;
    }
}

void InputUeLowLatency::tickEnd(int64_t frameId, float DeltaSeconds, bool bIdleMode)
{
    MarkerParams markerParams {};
    markerParams.frame_id = frameId;
    markerParams.marker_type = MarkerType::SIMULATION_END;

    auto result = InputCommon::set_marker(inputContext, device, markerParams);

    if (result != InputResult::Ok)
    {
        LOG_ERROR("set_marker result: {}", magic_enum::enum_name(result));
        return;
    }
}
