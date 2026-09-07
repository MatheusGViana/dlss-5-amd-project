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

#include "debugoverlayrendermodule.h"

#include "Cauldron2/dx12/framework/render/parameterset.h"
#include "Cauldron2/dx12/framework/render/pipelineobject.h"
#include "Cauldron2/dx12/framework/render/texture.h"

using namespace cauldron;

struct DebugOverlayConstants
{
    float input_debug_view_size[2];
    float output_target_size[2];
};

DebugOverlayRenderModule::~DebugOverlayRenderModule()
{
}

void DebugOverlayRenderModule::Init(const json& initData)
{
    m_pColorTarget = GetFramework()->GetColorTargetForCallback(GetName());

    m_pDenoiserRenderModule = static_cast<DenoiserRenderModule*>(GetFramework()->GetRenderModule("DenoiserRenderModule"));
    if (!m_pDenoiserRenderModule)
        return;

    m_pDenoiserDebugView = GetFramework()->GetRenderTexture(L"DenoiserDebugViewTarget");

    SamplerDesc linearSampler = {};

    RootSignatureDesc debugOverlayRootSignatureDesc;
    debugOverlayRootSignatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    debugOverlayRootSignatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1); // Debug view
    debugOverlayRootSignatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1); // Output color
    debugOverlayRootSignatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, 1, &linearSampler);
    m_pDebugOverlayRootSignature = RootSignature::CreateRootSignature(L"DebugOverlayRootSignature", debugOverlayRootSignatureDesc);
    if (!m_pDebugOverlayRootSignature)
        return;

    PipelineDesc debugOverlayPipelineDesc;
    debugOverlayPipelineDesc.SetRootSignature(m_pDebugOverlayRootSignature);
    ShaderBuildDesc debugOverlayDesc = ShaderBuildDesc::Compute(L"debug_overlay.hlsl", L"main", ShaderModel::SM6_6, nullptr);
    debugOverlayPipelineDesc.AddShaderDesc(debugOverlayDesc);
    m_pDebugOverlayPipeline = PipelineObject::CreatePipelineObject(L"DebugOverlayPipeline", debugOverlayPipelineDesc);
    if (!m_pDebugOverlayPipeline)
        return;

    m_pDebugOverlayParameterSet = ParameterSet::CreateParameterSet(m_pDebugOverlayRootSignature);
    m_pDebugOverlayParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(DebugOverlayConstants), 0);
    m_pDebugOverlayParameterSet->SetTextureSRV(m_pDenoiserDebugView, ViewDimension::Texture2D, 0);
    m_pDebugOverlayParameterSet->SetTextureUAV(m_pColorTarget, ViewDimension::Texture2D, 0);

    SetModuleEnabled(true);
    SetModuleReady(true);
}

void DebugOverlayRenderModule::Execute(double deltaTime, cauldron::CommandList* pCmdList)
{
    if (m_pDenoiserRenderModule && m_pDenoiserRenderModule->IsDebugViewEnabled())
    {
        const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

        DebugOverlayConstants constants = {};
        constants.input_debug_view_size[0] = static_cast<float>(m_pDenoiserDebugView->GetDesc().Width);
        constants.input_debug_view_size[1] = static_cast<float>(m_pDenoiserDebugView->GetDesc().Height);
        constants.output_target_size[0] = static_cast<float>(resInfo.DisplayWidth);
        constants.output_target_size[1] = static_cast<float>(resInfo.DisplayHeight);

        BufferAddressInfo constantsBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(constants), &constants);
        m_pDebugOverlayParameterSet->UpdateRootConstantBuffer(&constantsBufferInfo, 0);

        m_pDebugOverlayParameterSet->Bind(pCmdList, m_pDebugOverlayPipeline);
        SetPipelineState(pCmdList, m_pDebugOverlayPipeline);
        const uint32_t numGroupsX = (resInfo.DisplayWidth + 7) / 8;
        const uint32_t numGroupsY = (resInfo.DisplayHeight + 7) / 8;
        Dispatch(pCmdList, numGroupsX, numGroupsY, 1);
    }
}
