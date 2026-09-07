from pathlib import Path
r=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler')
for name in ['upscalers/IFeature_Dx11wDx12.cpp','upscalers/IFeature_VkwDx12.cpp']:
 p=r/name;s=p.read_text();s=s.replace('#include "pch.h"','#include "pch.h"\n#include <dlssnr/amd/AmdBridge.h>',1);s=s.replace('        dx12EvalResult = dx12Feature->Evaluate(cmdList, InParameters);','        dx12EvalResult = dx12Feature->Evaluate(cmdList, InParameters);\n        DlssNr::AmdBridge::Restore(InParameters);',1);p.write_text(s)
