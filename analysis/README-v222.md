# OptiScaler AMD PreSR Multipass v2.22

Single-pass neural submission no longer waits on the game's submission thread for HIP inference. This lets the game issue subsequent queue signals needed by the capture. Borrowed textures, command-list identity and private runtime allocations remain protected until BOTH native completion and the D3D12 fence retire. Pending frames preserve the current game input. A completion exceeding five seconds stops new neural jobs and retains in-flight resources; it does not forcibly release GPU resources.

The new asynchronous completion path applies to ONE neural pass. Multiple passes retain the prior sequential completion path and are not recommended for this Spider-Man test.

The previous v2.21 did not resolve Spider-Man: the new logs still showed 3.4-3.8 second capture waits and an access violation at dxgi.dll+0x19354. The exact crash instruction has not been identified. This revision addresses a blocking submission dependency, not a proven universal crash fix.

GPU tests: eight frames with a queue dependency signalled only after Submitted returns, zero timeout and finite output; forced-timeout recovery; changing render queue, extents and scale. In-game Spider-Man validation remains necessary.

Use Setup.bat. Neural defaults to disabled. Keep one pass for testing. Retain -forceReflexMarkers for Spider-Man and Dxgi=false. Licenses retained.
