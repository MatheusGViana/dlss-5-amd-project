# v2.5 timeout investigation

Evidence from logs 1: RX 9060 XT, 1208x680 then 896x504, three ~3.6-second capture stalls. Typical inference is 16-31 ms. The 154-timeout screenshot has unidentified hardware/game. The NBA report concerns RX 9070 XT, a capture synchronization issue and typed depth format 20. These are not assumed to share a cause.

Reproduction: corrected the smoke input gradient so high resolutions cannot overflow half-float values. The prior runtime at 2048x2048 completed neural inference in 125-172 ms but exhausted its 262144-iteration GPU shader ceiling (native log: host watchdog fired 0 times). Frame 1 retained the entire input; subsequent Record entered recovery. At 1024x1024 the same test passed.

Change: private shader hard ceiling 2097152; per-record allowance clamp(262144 + ceil(width*height/2), 262144, 2097152). The base margin accounts for worker launch as well as inference. Native staging initialization can temporarily supply its initial cap on recreation; the hard shader ceiling still applies. Private hashes and installer checks regenerated together. Host watchdog, history reset and current-input fallback are retained. This permits longer legitimate processing; it does not reduce inference cost.

Status: count successful neural frames independently of recorded frames, do not mark native timeout jobs completed, retain last successful completion time, include pending native timeout counters without double counting.

Validation artifacts:
- smoke-timeout-2048-baseline.txt: reproduced old failure.
- smoke-timeout-2048-lost-notify.txt: high resolution, forced lost notification exits GPU wait in 313 ms locally, current input untouched, recovery without restart; 7 successful frames and 1 forced timeout.
- smoke-timeout-multipass-fixed.txt: 8 successful frames, alternating 512/1024, 1/2/3 passes, settings changes and render queue switches, zero timeout events.
- smoke-timeout-tone-regression.txt: three passes, forced timeout, exposure and motion guide conversion, tone preserved, zero non-finite output and tone comparison errors.

A trial moving notification after ExecuteCommandLists was not retained: it changed the watchdog injection behavior and did not establish a capture-stall fix. The ~3.6-second capture stalls and NBA typed-depth path remain unresolved. No game/GPU-wide guarantee is made. Build v2.5 is a targeted fix for the independently reproduced iteration-ceiling failure.

RTGI continuation: radiance_test.cpp executes initialization plus four propagation passes with UAV barriers, reads all 15 slices and compares SH/radiance with independent CPU calculations. Six constant-scene cases passed, including odd dimensions, normal axes and sky rejection. Full trace/temporal/composition/game integration remains outstanding.
