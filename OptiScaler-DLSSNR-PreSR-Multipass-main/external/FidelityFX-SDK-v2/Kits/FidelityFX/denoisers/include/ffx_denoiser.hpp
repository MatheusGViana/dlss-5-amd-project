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

//------------------------------------------------------------------------------
// FFX Includes
//------------------------------------------------------------------------------

// ffx::InitHelper
// ffx::struct_type
#include "../../api/include/ffx_api.hpp"

//------------------------------------------------------------------------------
// FFX Denoiser Includes
//------------------------------------------------------------------------------

// FFX_API_CONFIGURE_DESC_TYPE_DENOISER_KEYVALUE
// FFX_API_CREATE_CONTEXT_DESC_TYPE_DENOISER
// FFX_API_DISPATCH_DESC_TYPE_DENOISER
// FFX_API_DISPATCH_DESC_TYPE_DENOISER_AMBIENT_OCCLUSION
// FFX_API_DISPATCH_DESC_TYPE_DENOISER_DEBUG_VIEW
// FFX_API_DISPATCH_DESC_TYPE_DENOISER_DIRECT_DIFFUSE
// FFX_API_DISPATCH_DESC_TYPE_DENOISER_DIRECT_SPECULAR
// FFX_API_DISPATCH_DESC_TYPE_DENOISER_DOMINANT_LIGHT
// FFX_API_DISPATCH_DESC_TYPE_DENOISER_INDIRECT_DIFFUSE
// FFX_API_DISPATCH_DESC_TYPE_DENOISER_INDIRECT_SPECULAR
// FFX_API_DISPATCH_DESC_TYPE_DENOISER_SPECULAR_OCCLUSION
// FFX_API_QUERY_DESC_TYPE_DENOISER_GET_DEFAULT_KEYVALUE
// FFX_API_QUERY_DESC_TYPE_DENOISER_GPU_MEMORY_USAGE
// ffxConfigureDescDenoiserKeyValue
// ffxCreateContextDescDenoiser
// ffxDispatchDescDenoiser
// ffxDispatchDescDenoiserAmbientOcclusion
// ffxDispatchDescDenoiserDebugView
// ffxDispatchDescDenoiserDirectDiffuse
// ffxDispatchDescDenoiserDirectSpecular
// ffxDispatchDescDenoiserDominantLight
// ffxDispatchDescDenoiserIndirectDiffuse
// ffxDispatchDescDenoiserIndirectSpecular
// ffxDispatchDescDenoiserSpecularOcclusion
// ffxQueryDescDenoiserGetDefaultKeyValue
// ffxQueryDescDenoiserGetGPUMemoryUsage
#include "ffx_denoiser.h"

//------------------------------------------------------------------------------
// External Includes
//------------------------------------------------------------------------------

// uint64_t
#include <stdint.h>
// std::integral_constant
#include <type_traits>

//------------------------------------------------------------------------------
// FFX Denoiser Defines
//------------------------------------------------------------------------------

#define FFX_DEFINE_DESC_TYPE_TO_ID_MAPPING(ffxType, ffxId)                     \
    template<>                                                                 \
    struct struct_type< ffxType >                                              \
        : std::integral_constant< uint64_t, ffxId >                            \
    {}

#define FFX_DEFINE_HELPER_DESC_TYPE_TO_DESC_TYPE_MAPPING(helperType, ffxType)  \
    struct helperType                                                          \
        : InitHelper< ffxType >                                                \
    {}

#define FFX_DEFINE_DESC_MAPPING(helperType, ffxType, ffxId)                    \
    FFX_DEFINE_DESC_TYPE_TO_ID_MAPPING(ffxType, ffxId);                        \
    FFX_DEFINE_HELPER_DESC_TYPE_TO_DESC_TYPE_MAPPING(helperType, ffxType)

//------------------------------------------------------------------------------
// FFX Denoiser Declarations
//------------------------------------------------------------------------------

// Helper types for header initialization. Api definition is in .h file.

namespace ffx
{
    //--------------------------------------------------------------------------
    // FFX Denoiser Descriptions: Create Context
    //--------------------------------------------------------------------------

    FFX_DEFINE_DESC_MAPPING(   CreateContextDescDenoiser,
                            ffxCreateContextDescDenoiser,
                            FFX_API_CREATE_CONTEXT_DESC_TYPE_DENOISER);

    //--------------------------------------------------------------------------
    // FFX Denoiser Descriptions: Configure
    //--------------------------------------------------------------------------

    FFX_DEFINE_DESC_MAPPING(   ConfigureDescDenoiserKeyValue,
                            ffxConfigureDescDenoiserKeyValue,
                            FFX_API_CONFIGURE_DESC_TYPE_DENOISER_KEYVALUE);

    //--------------------------------------------------------------------------
    // FFX Denoiser Descriptions: Dispatch
    //--------------------------------------------------------------------------

    FFX_DEFINE_DESC_MAPPING(   DispatchDescDenoiser,
                            ffxDispatchDescDenoiser,
                            FFX_API_DISPATCH_DESC_TYPE_DENOISER);
    FFX_DEFINE_DESC_MAPPING(   DispatchDescDenoiserDebugView,
                            ffxDispatchDescDenoiserDebugView,
                            FFX_API_DISPATCH_DESC_TYPE_DENOISER_DEBUG_VIEW);
    FFX_DEFINE_DESC_MAPPING(   DispatchDescDenoiserAmbientOcclusion,
                            ffxDispatchDescDenoiserAmbientOcclusion,
                            FFX_API_DISPATCH_DESC_TYPE_DENOISER_AMBIENT_OCCLUSION);
    FFX_DEFINE_DESC_MAPPING(   DispatchDescDenoiserDirectDiffuse,
                            ffxDispatchDescDenoiserDirectDiffuse,
                            FFX_API_DISPATCH_DESC_TYPE_DENOISER_DIRECT_DIFFUSE);
    FFX_DEFINE_DESC_MAPPING(   DispatchDescDenoiserDirectSpecular,
                            ffxDispatchDescDenoiserDirectSpecular,
                            FFX_API_DISPATCH_DESC_TYPE_DENOISER_DIRECT_SPECULAR);
    FFX_DEFINE_DESC_MAPPING(   DispatchDescDenoiserDominantLight,
                            ffxDispatchDescDenoiserDominantLight,
                            FFX_API_DISPATCH_DESC_TYPE_DENOISER_DOMINANT_LIGHT);
    FFX_DEFINE_DESC_MAPPING(   DispatchDescDenoiserIndirectDiffuse,
                            ffxDispatchDescDenoiserIndirectDiffuse,
                            FFX_API_DISPATCH_DESC_TYPE_DENOISER_INDIRECT_DIFFUSE);
    FFX_DEFINE_DESC_MAPPING(   DispatchDescDenoiserIndirectSpecular,
                            ffxDispatchDescDenoiserIndirectSpecular,
                            FFX_API_DISPATCH_DESC_TYPE_DENOISER_INDIRECT_SPECULAR);
    FFX_DEFINE_DESC_MAPPING(   DispatchDescDenoiserSpecularOcclusion,
                            ffxDispatchDescDenoiserSpecularOcclusion,
                            FFX_API_DISPATCH_DESC_TYPE_DENOISER_SPECULAR_OCCLUSION);

    //--------------------------------------------------------------------------
    // FFX Denoiser Descriptions: Query
    //--------------------------------------------------------------------------

    FFX_DEFINE_DESC_MAPPING(   QueryDescDenoiserGetDefaultKeyValue,
                            ffxQueryDescDenoiserGetDefaultKeyValue,
                            FFX_API_QUERY_DESC_TYPE_DENOISER_GET_DEFAULT_KEYVALUE);
    FFX_DEFINE_DESC_MAPPING(   QueryDescDenoiserGetGPUMemoryUsage,
                            ffxQueryDescDenoiserGetGPUMemoryUsage,
                            FFX_API_QUERY_DESC_TYPE_DENOISER_GPU_MEMORY_USAGE);

}  // namespace ffx

//------------------------------------------------------------------------------
// FFX Denoiser Defines
//------------------------------------------------------------------------------

#undef FFX_DEFINE_DESC_MAPPING
#undef FFX_DEFINE_HELPER_DESC_TYPE_TO_DESC_TYPE_MAPPING
#undef FFX_DEFINE_DESC_TYPE_TO_ID_MAPPING
