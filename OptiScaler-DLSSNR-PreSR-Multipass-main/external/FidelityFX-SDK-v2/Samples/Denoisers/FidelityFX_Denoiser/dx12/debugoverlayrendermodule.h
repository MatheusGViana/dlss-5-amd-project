// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2026 Advanced Micro Devices, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

#include "denoiserrendermodule.h"

#include <cauldron2/dx12/framework/render/rendermodule.h>
#include <cauldron2/dx12/framework/core/framework.h>

namespace cauldron
{
    class Texture;
    class ParameterSet;
    class RootSignature;
    class PipelineObject;
}  // namespace cauldron

class DebugOverlayRenderModule final : public cauldron::RenderModule
{
public:
    DebugOverlayRenderModule()
        : RenderModule(L"DebugOverlayRenderModule")
    {
    }
    ~DebugOverlayRenderModule() override;

    void Init(const json& initData);
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

private:
    const cauldron::Texture* m_pColorTarget = nullptr;

    cauldron::RootSignature* m_pDebugOverlayRootSignature = nullptr;
    cauldron::PipelineObject* m_pDebugOverlayPipeline = nullptr;
    cauldron::ParameterSet* m_pDebugOverlayParameterSet = nullptr;

    const cauldron::Texture* m_pDenoiserDebugView = nullptr;
    DenoiserRenderModule* m_pDenoiserRenderModule = nullptr;
};
