# PCSX2 integration investigation

Target: C:/Users/mathe/Desktop/ps2, pcsx2-qt.exe 2.9.34.0. Existing OptiScaler injection displays its menu, but the inspected log has no SR evaluation stream. Loading bundled FFX/XeSS libraries alone is not an SR invocation.

Current AMD Frame contract requires colour, motion and depth; Backend::Record rejects absent inputs. A swapchain backbuffer alone does not satisfy this contract. Do not claim neural support by supplying constant depth/zero motion.

Present integration candidate: menu/menu_overlay_dx.cpp has the current swapchain queue and backbuffers. Its recording path is conditional on RenderMenu(), so attaching an effect inside that condition would incorrectly disable processing when the menu closes. A separate per-swapchain owner is needed, with allocator/descriptor/texture lifetime fenced per frame, resize and device-loss handling, and processing before the OptiScaler menu. It will still include any HUD already composited by the emulator.

Stage 1: independent final-image capture/compose with explicit enable, initially pass-through, tested with menu hidden and visible, resize, pause/resume, fullscreen and swapchain destruction. Restrict initial scope to pcsx2-qt.exe and DX12; do not change native SR integrations.
Stage 2: reuse colour-only appearance/tonemap processing as a separately named spatial effect, not neural rendering. Avoid temporal history and fabricated geometry in this stage.
Stage 3: investigate an actual emulator adapter providing aligned colour/depth and estimated motion, or an independently validated RGB-only inference mode. Multiple PS2 render targets, interlacing, UI, game changes and replayed/duplicate frames need explicit treatment. The present backend has no established RGB-only contract.

No PCSX2 binary or installed DLL was modified in this investigation. No working emulator neural build has been produced.

Reference renderer source: https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/DX12/GSDevice12.cpp
