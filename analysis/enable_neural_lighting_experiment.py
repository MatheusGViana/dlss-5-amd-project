from pathlib import Path
r=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler')
p=r/'dlssnr/amd/AmdPreSr.h';s=p.read_text().replace('    float modelScale = 1;', '    bool toneChannels = false;\n    float modelScale = 1;');p.write_text(s)
p=r/'dlssnr/amd/AmdPreSr.cpp';s=p.read_text().replace('At<float>(r, 0x76e38) = cfg.skin;', 'At<float>(r, 0x76e38) = cfg.skin;\n            At<UINT>(r,0x76e44)=cfg.toneChannels?1u:0u;').replace('const bool settingsChanged = cfg.modelScale', 'const bool settingsChanged = cfg.toneChannels != p->lastSettings.toneChannels || cfg.modelScale');p.write_text(s)
p=r/'Config.h';s=p.read_text().replace('    CustomOptional<float> AmdNrScale', '    CustomOptional<bool> AmdNeuralLighting { false };\n    CustomOptional<float> AmdNeuralLightingStrength { .5f };\n    CustomOptional<float> AmdNrScale');p.write_text(s)
p=r/'Config.cpp';s=p.read_text().replace('            AmdNrScale.set_from_config', '            AmdNeuralLighting.set_from_config(readBool("DlssNr", "AmdNeuralLighting"));\n            AmdNeuralLightingStrength.set_from_config(readFloat("DlssNr", "AmdNeuralLightingStrength"));\n            AmdNrScale.set_from_config').replace('    ini.SetValue("DlssNr", "AmdModelScale",','    ini.SetValue("DlssNr", "AmdNeuralLighting", GetBoolValue(Instance()->AmdNeuralLighting.value_for_config()).c_str());\n    ini.SetValue("DlssNr", "AmdNeuralLightingStrength", GetFloatValue(Instance()->AmdNeuralLightingStrength.value_for_config()).c_str());\n    ini.SetValue("DlssNr", "AmdModelScale",');p.write_text(s)
p=r/'dlssnr/amd/AmdBridge.cpp';s=p.read_text().replace('    s.tone = 0;','    s.toneChannels=cfg.AmdNeuralLighting.value_or_default();\n    s.tone=s.toneChannels ? std::clamp(cfg.AmdNeuralLightingStrength.value_or_default(),0.f,1.f) : 0.f;');p.write_text(s)
p=r/'dlssnr/DlssNr_Menu.cpp';s=p.read_text();a='            float structure = config->DlssNrLocalStructure';s=s.replace(a,'''            bool lighting=config->AmdNeuralLighting.value_or_default();
            if(ImGui::Checkbox("Neural lighting (experimental)",&lighting))config->AmdNeuralLighting=lighting;
            if(lighting){
                float strength=config->AmdNeuralLightingStrength.value_or_default();
                if(ImGui::SliderFloat("Neural lighting strength",&strength,0,1))config->AmdNeuralLightingStrength=strength;
            }
'''+a,1);s=s.replace('config->AmdNrScale = 1.0f;','config->AmdNrScale = 1.0f;\n                    config->AmdNeuralLighting=false;\n                    config->AmdNeuralLightingStrength=.5f;');p.write_text(s)
