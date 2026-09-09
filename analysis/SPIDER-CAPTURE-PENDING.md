# Spider-Man capture still unresolved

v2.23 failed. The game submission contains one list (DIRECT queue), so batch splitting is not exercised. At the stop the D3D12 fence completed 1/1 but native worker completed 0/1. Worker later reports job 1 in 5890 ms, 5803.4 ms capture wait. Do not label v2.23 a fix.

Repeated 1624x1624 isolated GPU smoke without debug layer, including post-submission CPU signal dependency: 8 completed frames, zero timeouts, finite output; last frame 141 ms. This does not reproduce game GPU load or private interop behavior.

Next controlled in-game test: temporarily cap FramerateLimit=60, same DLL and one neural pass. INI backed up alongside game. This tests GPU scheduling contention only; it is not a claimed correction. Restore previous limit if no change. No new release generated.

60 FPS test: native capture waits 4308.4, 4543.0, 4509.3 ms. Lower than the previous single 5803.4 ms sample, but still unusable; cannot attribute this difference to the cap without controlled repeats. Game limit restored to its previous value. No fix established.
