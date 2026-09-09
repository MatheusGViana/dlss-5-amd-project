# OptiScaler AMD PreSR Multipass v2.24

Fixes private neural shader compilation when a game ships an old d3dcompiler_47.dll. Spider-Man Remastered includes version 6.3.9600.16384 (2013), which rejects the FP16 typed UAV load shader with X3676. The private backend now resolves D3DCompile explicitly from the Windows System32 compiler and binds its own import before initialization. Game files and game compiler imports are not replaced. The private runtime hash is verified before touching its known import slot.

Reproduction: placing the Spider-Man compiler beside the smoke executable caused X3676, disabled residual application, zero changed pixels on frame zero and unfinished inference. With the fix and the SAME old DLL still loaded, the residual pipeline compiled and eight frames completed with modified finite output and zero timeouts.

Further tests passed with the old compiler present: 1624x1624 without the debug layer, a queue dependency signalled after submission returns, and multiple passes with changing extents/queues/scale. In-game Spider-Man confirmation is still required; this is not a claim that all previous crashes are fixed.

Retains lifetime/timeout safeguards. Neural rendering defaults to false, Lightning Strength to 0.5. Use Setup.bat. For the Spider-Man test use one neural pass and the existing -forceReflexMarkers launcher. No FPS cap is required by this change. Licenses retained.
