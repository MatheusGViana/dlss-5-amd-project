# Tester log review / v2.14

- nb2k26: actual executable folder NBA 2K27; OptiScaler build 20260907_184739, Radeon RX 9060 XT. Not identified as v2.13. Native log has standalone hook startup followed by another load; cannot conclude duplicate active processing without module inventory. Exposure and motion vary substantially, but this alone does not prove a bug. No validated flicker fix for this game.
- cyber2: actual Spider-Man 2. Encoded mean repeatedly zero, inferred exposure ~10000; investigate colour capture/exposure with matching OptiScaler and bridge logs. Do not assume Cyberpunk or invent missing 4x4 motion vectors.
- slide neuro: full-size motion format 10 (RGBA16F) rejected by the resampler whitelist. Added RGBA16F/RGBA32F support; XY is read by the existing shader. Synthetic GPU test covers RGBA16F with nonzero XY, scale/extent changes, multiple passes and optional effect.
- transient small guides: preflight rejection before GPU recording now returns without permanent backend failure; next valid input may recover. No fabricated vectors.
- skin: bridge writes separate structure and skin native fields, but the runtime's global structure behavior is not a skin exclusion contract. Independent skin suppression is unresolved; no skin-segmentation claim.
- intensity: v2.13 protection deliberately suppresses reduced-resolution edits at high contrast. Retained because user reports flicker stopped. Recovering strength without flicker remains unresolved.

The crash reported during slider interaction is not declared fixed. Need matching runtime/module version and crash stack for further attribution.
