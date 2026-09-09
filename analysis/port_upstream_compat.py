from pathlib import Path
r=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler');u=Path('Update/OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler')
(r/'proxies/Streamline_Proxy.h').write_bytes((u/'proxies/Streamline_Proxy.h').read_bytes())
p=r/'hooks/Streamline_Hooks.cpp';s=p.read_text();s=s.replace('    state.numFramesToGenerateMax = 1;\n    state.bIsVsyncSupportAvailable = sl::Boolean::eTrue;','    if(state.structVersion >= 2) {\n        state.numFramesToGenerateMax = 1;\n        state.bIsVsyncSupportAvailable = sl::Boolean::eTrue;\n    }')
s=s.replace('        state.numFramesToGenerateMax = 1;\n\n        LOG_DEBUG','        if(originalStructVersion >= 2) state.numFramesToGenerateMax = 1;\n\n        LOG_DEBUG')
s=s.replace('result = o_slDLSSGGetState(viewport, dynamic_cast<sl::DLSSGState&>(newState), options);','result = o_slDLSSGGetState(viewport, dynamic_cast<sl::DLSSGState&>(newState), options);\n        if(result != sl::Result::eOk) return result;')
s=s.replace('result = o_slDLSSGGetState(viewport, state, options);','result = o_slDLSSGGetState(viewport, state, options);\n        if(result != sl::Result::eOk) return result;')
s=s.replace('FGDLSSGOverrideInterpolationCount = state.dlssgMfgMax.value();','FGDLSSGOverrideInterpolationCount.set_volatile_value(state.dlssgMfgMax.value());').replace('FGDLSSGOverrideInterpolationCount = optiState.dlssgMfgMax.value();','FGDLSSGOverrideInterpolationCount.set_volatile_value(optiState.dlssgMfgMax.value());');p.write_text(s)
