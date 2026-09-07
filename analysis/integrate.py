from pathlib import Path
r=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler')
p=r/'shaders/dlssnr/DlssNr_Dx12.cpp';s=p.read_text();s=s.replace('#include "pch.h"','#include "pch.h"\n#include <dlssnr/amd/AmdBridge.h>',1)
needle='    // forcePost is supplied only for a native RR feature'
pos=s.index(needle,s.index('void EvaluateInternal('));s=s[:pos]+'''    // The AMD runtime has a separate HIP pipeline. It consumes the colour input
    // before SR and must never fall through to the NVIDIA NGX backend.
    if (DlssNr::AmdBridge::HasFiles())
    {
        if (!beforeUpscale || !cfg.DlssNrRunBeforeSr.value_or_default() || forcePost)
            return;
        if (DlssNr::AmdBridge::Before(cmdList, params, timingQueue))
            return;
    }

'''+s[pos:];p.write_text(s)
p=r/'inputs/NVNGX_DLSS_Dx12.cpp';s=p.read_text().replace('#include "pch.h"','#include "pch.h"\n#include <dlssnr/amd/AmdBridge.h>',1)
s=s.replace('            LOG_DEBUG("Native DLSS EvaluateFeature result:', '            DlssNr::AmdBridge::Restore(InParameters);\n            LOG_DEBUG("Native DLSS EvaluateFeature result:',1)
s=s.replace('    // Same pass, for OptiScaler\'s own upscalers rather than native DLSS.','    DlssNr::AmdBridge::Restore(InParameters);\n\n    // Same pass, for OptiScaler\'s own upscalers rather than native DLSS.',1);p.write_text(s)
p=r/'dlssnr/DlssNr_Menu.cpp';s=p.read_text().replace('#include "pch.h"','#include "pch.h"\n#include "amd/AmdBridge.h"',1)
needle='        HelpMarker("Adds a Neural Rendering';pos=s.find(needle)
if pos<0:
 pos=s.index('        HelpMarker(',s.index('void RenderMenu'))
s=s[:pos]+'''        if (DlssNr::AmdBridge::HasFiles())
        {
            bool pre = config->DlssNrRunBeforeSr.value_or_default();
            if (ImGui::Checkbox("AMD: apply before Super Resolution", &pre))
                config->DlssNrRunBeforeSr = pre;
            int passes = (int) config->DlssNrPasses.value_or_default();
            if (ImGui::SliderInt("AMD neural passes", &passes, 1, 3))
                config->DlssNrPasses = (uint32_t) passes;
            float tone = config->DlssNrLocalTone.value_or_default();
            float structure = config->DlssNrLocalStructure.value_or_default();
            float skin = config->DlssNrSkinStructure.value_or_default();
            if (ImGui::SliderFloat("AMD tone (first pass)", &tone, 0, 2)) config->DlssNrLocalTone = tone;
            if (ImGui::SliderFloat("AMD structure", &structure, 0, 2)) config->DlssNrLocalStructure = structure;
            if (ImGui::SliderFloat("AMD skin structure", &skin, 0, 2)) config->DlssNrSkinStructure = skin;
            ImGui::TextWrapped("%s", DlssNr::AmdBridge::Status().c_str());
            ImGui::TextWrapped("AMD HIP backend. Each pass owns independent temporal history. More passes increase GPU time and memory. Restart the game after a backend failure.");
            return;
        }

'''+s[pos:];p.write_text(s)
p=r/'OptiScaler.vcxproj';s=p.read_text();pos=s.index('  <ItemGroup>');s=s[:pos]+'''  <ItemGroup>
    <ClCompile Include="dlssnr\\amd\\AmdPreSr.cpp"><PrecompiledHeader>NotUsing</PrecompiledHeader></ClCompile>
    <ClCompile Include="dlssnr\\amd\\AmdBridge.cpp" />
    <ClInclude Include="dlssnr\\amd\\AmdPreSr.h" />
    <ClInclude Include="dlssnr\\amd\\AmdBridge.h" />
  </ItemGroup>
'''+s[pos:];s=s.replace('winhttp.lib;WindowsApp.lib;','bcrypt.lib;winhttp.lib;WindowsApp.lib;');p.write_text(s)
