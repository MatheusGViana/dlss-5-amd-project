from pathlib import Path
p=Path('OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler/dlssnr/DlssNr_Menu.cpp');s=p.read_text();s=s.replace('            ImGui::TextDisabled("NR resolution applies when you release the slider.");\n','').replace('Neural lighting (experimental)','Neural lighting')
a=s.index('            int passes = (int) config->DlssNrPasses');b=s.index('            if (ImGui::TreeNode("Experimental"))',a)
s=s[:a]+'''            // Stage costly neural parameter edits in ImGui state. Keep rendering
            // with the committed parameters until release/text-edit completion.
            auto neuralSlider = [](const char* label, auto& option, float lo, float hi) {
                auto storage=ImGui::GetStateStorage();
                const ImGuiID id=ImGui::GetID(label);
                const ImGuiID activeId=id ^ 0x6e72534cu;
                float value=storage->GetBool(activeId,false)?storage->GetFloat(id):option.value_or_default();
                ImGui::SliderFloat(label,&value,lo,hi);
                const bool active=ImGui::IsItemActive();
                const bool commit=ImGui::IsItemDeactivatedAfterEdit();
                storage->SetFloat(id,value);storage->SetBool(activeId,active);
                if(commit)option=value;
            };
            static int passes = 1;
            static bool editingPasses = false;
            if(!editingPasses)passes=int(config->DlssNrPasses.value_or_default());
            ImGui::SliderInt("AMD neural passes", &passes, 1, 3);
            editingPasses=ImGui::IsItemActive();
            if(ImGui::IsItemDeactivatedAfterEdit())config->DlssNrPasses=uint32_t(passes);
            bool lighting=config->AmdNeuralLighting.value_or_default();
            if(ImGui::Checkbox("Neural lighting",&lighting))config->AmdNeuralLighting=lighting;
            if(lighting)neuralSlider("Neural lighting strength",config->AmdNeuralLightingStrength,0,1);
            neuralSlider("AMD structure",config->DlssNrLocalStructure,0,2);
            neuralSlider("AMD character structure",config->DlssNrSkinStructure,0,2);
'''+s[b:];p.write_text(s)
