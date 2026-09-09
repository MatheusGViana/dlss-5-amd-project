#pragma once
#include "AmdPreSr.h"
#include <memory>
namespace AmdPreSr
{
// Called only at the owning backend's quiescent frame boundary. The caller
// retains this object and its resources until its GPU completion fence retires.
class RtgiNative
{
    struct Impl;
    std::unique_ptr<Impl> p;

  public:
    RtgiNative(ID3D12Device*, const std::filesystem::path&);
    ~RtgiNative();
    ID3D12Resource* Record(ID3D12GraphicsCommandList*, const Frame&, const RtgiSettings&);
    void ResetHistory();
};
} // namespace AmdPreSr
