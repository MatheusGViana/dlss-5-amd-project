from pathlib import Path
r=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler')
p=r/'dlssnr/DlssNr_Menu.cpp';s=p.read_text();s=s.replace('''            bool lighting=config->AmdNeuralLighting.value_or_default();
            if(ImGui::Checkbox("Neural lighting",&lighting))config->AmdNeuralLighting=lighting;
            if(lighting)neuralSlider("Neural lighting strength",config->AmdNeuralLightingStrength,0,1);''','''            neuralSlider("Lightning Strength",config->AmdNeuralLightingStrength,0,1);''').replace('config->AmdNeuralLighting=false;','config->AmdNeuralLighting=true;');p.write_text(s)
p=r/'Config.h';s=p.read_text().replace('AmdNeuralLighting { false }','AmdNeuralLighting { true }');p.write_text(s)
p=r/'dlssnr/amd/AmdBridge.cpp';s=p.read_text().replace('s.toneChannels=cfg.AmdNeuralLighting.value_or_default();','s.toneChannels=cfg.AmdNeuralLightingStrength.value_or_default()>0;');p.write_text(s)
for n in ['PACKAGE_AMD.ps1','analysis/INSTALAR_AMD.ps1']:
 p=Path(n);s=p.read_text();s='\n'.join(x for x in s.split('\n') if 'COLETAR_LOGS' not in x)
 if n=='PACKAGE_AMD.ps1':s=s.replace('v2.19','v2.20').replace('README-v219','README-v220').replace('AmdNeuralLighting=false','AmdNeuralLighting=true');s='\n'.join(x for x in s.split('\n') if 'analysis/DIAGNOSTICO_AMD.ps1' not in x)
 p.write_text(s)
