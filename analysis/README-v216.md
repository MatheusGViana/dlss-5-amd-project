# OptiScaler AMD PreSR Multipass v2.16

NR resolution changes apply in the current game session when the slider is released or its text edit is completed. No restart is required for this control. The bridge waits 300 ms for stable resolution settings and then resumes neural processing; the current game image is preserved during this transition. Save settings only to retain the selected scale across game restarts.

Retains v2.14 RGBA16F/RGBA32F motion resampling support and nonfatal rejection of transient undersized guides. A rejected small guide must not permanently disable the backend. Retains the v2.15 native automatic character mask and accurate character-structure label. Global structure is not a skin exclusion control.

Retains reduced-scale highlight protection from v2.13. It may weaken neural edits at high-contrast edges; equal quality/intensity at 25% is not claimed. 100% bypasses that reduced-scale composition.

Validation: synthetic backend test with scale/extent changes, RGBA16F motion, typeless depth, multipass and Experimental enabled passed. Release build completed. In-game transition behavior still requires testing; the previously reported capture-related crash is not declared fixed.

Install: close the game, extract all files and run INSTALAR_AMD.bat. Existing installation files are backed up. Keep all dependencies together. Diagnostics remain in amd_presr.log, amd_bridge.log, dlssnr_on_amd.log and OptiScaler.log.
