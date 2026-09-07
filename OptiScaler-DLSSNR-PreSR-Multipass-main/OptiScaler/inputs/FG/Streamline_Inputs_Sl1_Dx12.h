#pragma once
#include "SysUtils.h"

#include <sl1.h>
#include <d3d12.h>
#include <framegen/IFGFeature_Dx12.h>

#include <mutex>
#include <unordered_map>
#include <vector>

class Sl1_Inputs_Dx12
{
  private:
    struct CachedTag
    {
        sl1::Resource resource {};
        sl1::Extent extent {};
        bool hasExtent = false;
        sl1::BufferType type {};
        uint32_t id = 0;
    };

    bool infiniteDepth = false;
    bool hasLastConstants = false;
    sl1::Constants lastConstants {};

    std::mutex _mutex;
    std::unordered_map<uint64_t, CachedTag> _tags {};

    std::mutex _frameBoundaryMutex;
    bool _isFrameFinished = true;

    uint32_t _currentIndex = UINT32_MAX;
    uint32_t _lastPresentFrameId = UINT32_MAX;
    uint32_t _frameIdIndex[BUFFER_COUNT] = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };

    uint64_t mvsWidth = 0;
    uint32_t mvsHeight = 0;

    static uint64_t TagKey(uint32_t id, sl1::BufferType type);
    static bool IsTrue(sl1::Boolean value) { return value == sl1::eTrue; }

    void CheckForFrame(IFGFeature_Dx12* fg, uint32_t frameIndex);
    int IndexForFrameId(uint32_t frameIndex) const;

    bool applyConstants(const sl1::Constants& values, uint32_t frameIndex, uint32_t id);
    bool reportCachedResource(const CachedTag& tag, ID3D12GraphicsCommandList* cmdBuffer, uint32_t frameIndex);

  public:
    bool setConstants(const sl1::Constants& constants, uint32_t frameIndex, uint32_t id);
    bool setTag(const sl1::Resource* resource, sl1::BufferType type, uint32_t id, const sl1::Extent* extent);
    bool evaluateFeature(sl1::CommandBuffer* cmdBuffer, sl1::Feature feature, uint32_t frameIndex, uint32_t id);
    bool evaluateState();
    void markPresent(uint64_t frameIndex);
};
