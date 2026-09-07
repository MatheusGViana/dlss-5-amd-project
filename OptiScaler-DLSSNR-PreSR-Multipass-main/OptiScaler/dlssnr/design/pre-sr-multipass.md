# Pre-SR placement and persistent multipass

This experimental branch adds two opt-in controls to the `[DlssNr]` section:

- `RunBeforeSR=true` runs Neural Rendering on the colour input immediately before Super Resolution.
  The default is `false`, preserving the v0.2.0 post-upscale seam. Ray Reconstruction/DLSSD is
  deliberately forced to remain post-upscale because its input contract is not compatible with the
  PR #6 pre-SR path.
- `Passes=N` selects one to three sequential model layers. The default is `1`.
- `Pass2Preset`, `Pass2Style`, `Pass3Preset`, and `Pass3Style` optionally select a different built-in
  profile for later layers. `auto` inherits pass 1. These are profiles inside one model runtime, not
  separate model DLLs.

## Multipass lifetime and data flow

Every layer owns a persistent NGX feature and temporal history. A missing feature is created in one
submission epoch and remains pending until that epoch changes; no Neural Rendering feature is
evaluated on the command-list recording that created it. Native DX12 uses the wrapped Present count,
while the DX11/Vulkan bridges supply their post-submit frame counter. At most one extra feature is
created per submitted frame. A failed extra creation is latched and the ready contiguous prefix
remains active, rather than reusing the main feature or retrying every frame.

Preset and style are read when a layer's feature is created. Pass 1 uses `Preset` and `Style`; later
passes inherit those values unless their override is set. Changing an active layer's profile parks the
whole generation for deferred release and rebuilds it through the same one-feature-per-submission
sequence. Changing an inactive layer does not disturb pass 1; its profile is read when that layer is
later enabled.

The frame is encoded once. Its base proxy remains immutable while model answers ping-pong through two
same-format, same-size resources:

`base -> A -> B -> A` (as needed for one, two, or three layers)

The final answer is composed once against the immutable base. This keeps matched-residual transfer
cumulative (`final - base`) without compounding colour/transfer settings. Local tone is applied by the
first model layer only. A camera cut resets every active layer; a newly created extra layer is also
reset on its first evaluation.

Features and scratch resources are parked for deferred release on tuning, raster, format, placement,
or pass-count changes so in-flight frame-generation work cannot retain freed objects.

## Resource-state rules

Post-SR output uses the existing output-arrival state. Pre-SR colour arrives and is returned as a
non-pixel shader resource. If Color has no UAV flag, composition writes to an owned scratch texture and
copies back instead of binding an illegal UAV.

## Guardrails

- Pass count is clamped to `1..3`; historical testing found later layers converged while cost and
  artifacts continued to grow.
- The driver-proxy backend remains single-pass and logs the effective fallback.
- Origin-zero padded or max-sized Color allocations are copied to an active-sized UAV texture before
  encode/model/resolve, then only the edited active rectangle is copied back. The game's padding and
  resource state are preserved. The compact texture shares the scratch set's deferred retirement;
  changes in active resolution rebuild NR features/history as usual. No shader or guide-coordinate
  resampling is introduced by the crop. The extra copies are inside NR's GPU timing interval.
- Non-zero colour offsets, partial/out-of-bounds active sizes and unsupported texture layouts still
  fall back post-SR. Both absent active-size values retain the resource-size interpretation.
- Placement is part of the rebuild key even when pre/post surfaces happen to share dimensions and
  format (for example DLAA).
- Working scales from 25% through 200% remain supported; the ping-pong resources use model-work size.
