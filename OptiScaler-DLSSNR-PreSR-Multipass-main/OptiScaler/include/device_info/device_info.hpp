// From https://github.com/GPUOpen-Tools/device_info licensed under MIT

//==============================================================================
/// Copyright Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief device_info library interface.
//==============================================================================

#ifndef DEVICE_INFO_HPP_
#define DEVICE_INFO_HPP_

#include <cstdint>
#include <limits>
#include <optional>

namespace device_info
{
/// Specifies the ASIC type.
enum class AsicType : std::int8_t
{
    kUndefinedAsic = -1, ///< Undefined ASIC
    kTahitiPro,          ///< TAHITI GPU PRO
    kTahitiXt,           ///< TAHITI GPU XT
    kPitcairnPro,        ///< PITCAIRN GPU PRO
    kPitcairnXt,         ///< PITCAIRN GPU XT
    kCapeVerdePro,       ///< CAPE VERDE PRO GPU
    kCapeVerdeXt,        ///< CAPE VERDE XT GPU
    kOland,              ///< OLAND GPU (mobile is MARS)
    kHainan,             ///< HAINAN GPU
    kBonaire,            ///< BONAIRE GPU (mobile is SATURN)
    kHawaii,             ///< HAWAII GPU
    kKalindi,            ///< KB APU
    kSpectre,            ///< KV APU SPECTRE
    kSpectreSl,          ///< KV APU SPECTRE SL
    kSpectreLite,        ///< KV APU SPECTRE LITE
    kSpooky,             ///< KV APU SPOOKY
    kIceland,            ///< ICELAND GPU
    kTonga,              ///< TONGA GPU
    kCarrizo,            ///< CZ APU
    kCarrizoEmb,         ///< CZ APU EMBEDDED
    kFiji,               ///< FIJI GPU
    kStoney,             ///< STONEY APU
    kEllesmere,          ///< ELLESMERE GPU
    kBaffin,             ///< BAFFIN GPU
    kGfx8_0_4,           ///< GFX8_0_4 GPU
    kVegaM1,             ///< VegaM GPU
    kVegaM2,             ///< VegaM GPU
    kGfx9_0_0,           ///< GFX9_0_0 GPU
    kGfx9_0_2,           ///< GFX9_0_2 APU
    kGfx9_0_4,           ///< GFX9_0_4 GPU
    kGfx9_0_6,           ///< GFX9_0_6 GPU
    kGfx9_0_9,           ///< GFX9_0_9 GPU
    kGfx9_0_A,           ///< GFX9_0_A GPU
    kGfx9_0_C,           ///< GFX9_0_C GPU
    kGfx9_4_2,           ///< GFX9_4_2 GPU
    kGfx9_5_0,           ///< GFX9_5_0 GPU
    kGfx10_1_0,          ///< GFX10_1_0 GPU
    kGfx10_1_0Xl,        ///< GFX10_1_0_XL GPU
    kGfx10_1_2,          ///< GFX10_1_2 GPU
    kGfx10_1_2X,         ///< GFX10_1_2_X GPU
    kGfx10_1_2Xt,        ///< GFX10_1_2_XT GPU
    kGfx10_1_1,          ///< GFX10_1_1 GPU
    kGfx10_3_0,          ///< GFX10_3_0 GPU
    kGfx10_3_0Xt,        ///< GFX10_3_0_XT GPU
    kGfx10_3_0Xtx,       ///< GFX10_3_0_XTX GPU
    kGfx10_3_1,          ///< GFX10_3_1 GPU
    kGfx10_3_2,          ///< GFX10_3_2 GPU
    kGfx10_3_2Xt,        ///< GFX10_3_2_XT GPU
    kGfx10_3_3,          ///< GFX10_3_3 APU
    kGfx10_3_4,          ///< GFX10_3_4 GPU
    kGfx10_3_5,          ///< GFX10_3_5 APU
    kGfx10_3_6,          ///< GFX10_3_6 APU
    kGfx11_0_0,          ///< GFX11_0_0 GPU
    kGfx11_0_0Xt,        ///< GFX11_0_0_XT GPU
    kGfx11_0_0Gre,       ///< GFX11_0_0_GRE GPU
    kGfx11_0_0M,         ///< GFX11_0_0_M GPU
    kGfx11_0_1,          ///< GFX11_0_1 GPU
    kGfx11_0_1Xt,        ///< GFX11_0_1_XT GPU
    kGfx11_0_2,          ///< GFX11_0_2 GPU
    kGfx11_0_2Xt,        ///< GFX11_0_2_XT GPU
    kGfx11_0_3,          ///< GFX11_0_3 APU
    kGfx11_0_3A,         ///< GFX11_0_3A APU
    kGfx11_0_3B,         ///< GFX11_0_3B APU
    kGfx11_5_0,          ///< GFX11_5_0 APU
    kGfx11_5_1,          ///< GFX11_5_1 APU
    kGfx11_5_2,          ///< GFX11_5_2 APU
    kGfx11_5_3,          ///< GFX11_5_3 APU
    kGfx11_5_3A,         ///< GFX11_5_3A APU
    kGfx12_0_0,          ///< GFX12_0_0 GPU
    kGfx12_0_0Xt,        ///< GFX12_0_0_XT GPU
    kGfx12_0_1Gre,       ///< GFX12_0_1_GRE GPU
    kGfx12_0_1,          ///< GFX12_0_1 GPU
    kGfx12_0_1Xt,        ///< GFX12_0_1_XT GPU
    kTotalAsics          ///< Total number of ASICs (must be last)
};

/// Specifies the hardware generation.
enum class HwGeneration : std::uint8_t
{
    kUndefinedGeneration,        ///< Undefined hw generation
    kNvidia,                     ///< Nvidia GPU (Not used; kept for backwards compatibility)
    kIntel,                      ///< Intel GPU (Not used; kept for backwards compatibility)
    kSouthernIsland,             ///< GFX IP 6
    kFirstAmd = kSouthernIsland, ///< First AMD generation
    kSeaIsland,                  ///< GFX IP 7
    kVolcanicIsland,             ///< GFX IP 8
    kGfx9,                       ///< GFX IP 9
    kGfx10,                      ///< GFX IP 10
    kGfx10_3,                    ///< GFX IP 10.3
    kGfx11,                      ///< GFX IP 11
    kCdna,                       ///< MI-100
    kCdna2,                      ///< MI-200
    kCdna3,                      ///< MI-300
    kGfx12,                      ///< GFX IP 12
    kCdna4,                      ///< MI-350
    kGfx11_5,                    ///< GFX IP 11.5
    kTotalHwGenerations          ///< Total number of hardware generations (must be last)
};

/// Card info.
struct CardInfo
{
    AsicType asic_type;         ///< ASIC type.
    uint32_t device_id;         ///< Numeric device id.
    uint32_t revision_id;       ///< Numeric revision id.
    HwGeneration generation;    ///< Hardware generation.
    bool is_apu;                ///< Flag indicating whether or not the device is an APU.
    const char* gfx_target;     ///< GFX target name used by the compiler and driver (e.g. "gfx1100").
    const char* marketing_name; ///< Marketing style device name.
};

/// Device info.
struct DeviceInfo
{
    uint8_t num_shader_engines;   ///< Number of shader engines.
    uint8_t max_wave_per_simd;    ///< Number of wave slots per SIMD.
    uint8_t clocks_per_primitive; ///< Number of clocks it takes to process a primitive.
    uint8_t num_sq_counters;      ///< Max number of SQ counters.
    uint8_t num_prim_pipes;       ///< Number of primitive pipes.
    uint8_t wave_size;            ///< Wavefront size.
    uint8_t num_sh_per_se;        ///< Number of shader arrays per Shader Engine.
    uint8_t num_cus;              ///< Number of Compute Units.
    uint8_t num_simd_per_cu;      ///< Number of SIMDs per Compute Unit.
    uint16_t num_vgpr_per_simd;   ///< Number of VGPRs per SIMD.
};

/// Get total number of SIMDs.
///
/// @param [in] info Device info struct.
///
/// @return Total number of SIMDs.
[[nodiscard]] constexpr uint32_t TotalSimds(const DeviceInfo& info) noexcept
{
    return info.num_simd_per_cu * info.num_cus;
}

/// Get total number of shader arrays.
///
/// @param [in] info Device info struct.
///
/// @return Total number of shader arrays.
[[nodiscard]] constexpr uint32_t TotalShaderArrays(const DeviceInfo& info) noexcept
{
    return info.num_sh_per_se * info.num_shader_engines;
}

/// Get total number of VGPRs.
///
/// @param [in] info Device info struct.
///
/// @return Total number of VGPRs.
[[nodiscard]] constexpr uint32_t TotalVgprs(const DeviceInfo& info) noexcept
{
    return TotalSimds(info) * info.num_vgpr_per_simd;
}

constexpr uint32_t kRevisionIdAny =
    std::numeric_limits<uint32_t>::max(); ///< Ignore revision id when looking up device ID.
constexpr uint32_t kAmdVendorId = 0x1002; ///< The AMD vendor ID.

/// Adapter ID struct used for looking up card info.
/// Revision ID is optional, pass kRevisionIdAny if revision ID is not important.
struct AdapterId
{
    uint32_t vendor_id;   ///< Vendor ID.
    uint32_t device_id;   ///< Device ID.
    uint32_t revision_id; ///< Revision ID.
};

/// Get card info for a device. Binary search is used so lookup time is roughly O(log(n)).
///
/// @param [in] id Adapter ID containing vendor ID, device ID, and revision ID (revision ID is optional, pass
/// kRevisionIdAny if revision ID is not important).
///
/// @return Optional card info if device is found.
[[nodiscard]] std::optional<CardInfo> GetCardInfo(const AdapterId& id) noexcept;

/// Get device info from card info. Search is constant time since it's a direct index lookup.
///
/// @param [in] info Card info struct.
///
/// @return Optional device info if device is found.
[[nodiscard]] std::optional<DeviceInfo> GetDeviceInfo(const CardInfo& info) noexcept;

/// Get total LDS size in bytes.
///
/// @param [in] gen Hardware generation.
/// @param [in] info Device info struct needed to calculate total LDS size.
///
/// @return Total LDS size in bytes if found.
[[nodiscard]] std::optional<uint32_t> GetTotalLdsSizeInBytes(HwGeneration gen, const DeviceInfo& info) noexcept;
} // namespace device_info

#endif
