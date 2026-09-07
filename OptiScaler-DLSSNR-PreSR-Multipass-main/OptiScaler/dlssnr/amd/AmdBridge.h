#pragma once
#include <d3d12.h>
#include <nvsdk_ngx.h>
#include <string>
namespace DlssNr::AmdBridge
{
bool HasFiles();
bool Before(ID3D12GraphicsCommandList*, NVSDK_NGX_Parameter*, ID3D12CommandQueue*);
void Restore(NVSDK_NGX_Parameter*);
void InvalidateHistory();
std::string Status();
} // namespace DlssNr::AmdBridge
