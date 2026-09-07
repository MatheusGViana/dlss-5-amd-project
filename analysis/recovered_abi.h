// Documentation of the observed x64 layout, NOT a supported runtime API.
// Binary SHA256: 106223723fd9266c44d38dc2fb77933948ab37803f46bfcea2bae3a0a474ac84
// Derived from the three FSR wrappers and reads in RVA 0xA0B0.
// Names are analyst-assigned. No calls into the DLL are made by this file.
#pragma once
#include <cstddef>
#include <cstdint>

namespace amd_nr_analysis {
struct ObservedFramePacket {
    std::uint64_t commandList;  // 0x00 ID3D12GraphicsCommandList*
    std::uint64_t colour;       // 0x08 ID3D12Resource*, original hooks use FSR output
    std::uint32_t colourState;  // 0x10 FFX state bits, NOT raw D3D12 state bits
    std::uint32_t padding14;
    std::uint64_t motion;       // 0x18
    std::uint32_t motionState;  // 0x20
    std::uint32_t padding24;
    std::uint64_t depth;        // 0x28
    std::uint32_t depthState;   // 0x30
    std::uint32_t padding34;
    std::uint64_t exposure;     // 0x38
    std::uint32_t exposureState;// 0x40
    float motionScaleX;        // 0x44
    float motionScaleY;        // 0x48
    std::uint32_t padding4c;
};
static_assert(sizeof(ObservedFramePacket) == 0x50);
static_assert(offsetof(ObservedFramePacket, colour) == 0x08);
static_assert(offsetof(ObservedFramePacket, motion) == 0x18);
static_assert(offsetof(ObservedFramePacket, depth) == 0x28);
static_assert(offsetof(ObservedFramePacket, exposure) == 0x38);
static_assert(offsetof(ObservedFramePacket, motionScaleX) == 0x44);
static_assert(offsetof(ObservedFramePacket, motionScaleY) == 0x48);
// The observed packet does not convey renderSize, jitter or a camera-cut reset.
// Do not interpret the apparent void return of RVA 0xA0B0 as acceptance/completion.
}
