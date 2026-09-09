# Continuity follow-up

Cyberpunk log after the 4 ms change: at least 360 recovered fence waits and two logged GPU skips (3619578 and 3629562), no pending-list skip in the inspected session tail. This supports a real skip path, not proof that every visual flash has the same cause.

Change: bounded wait for already submitted GPU work increased to 16 ms. Unsubmitted command lists still skip immediately to avoid recording/submission deadlock. This can increase CPU frame latency under load; it is not an inference optimization and cannot guarantee no skips on longer stalls.

UI section renamed Experimental; enable label is Enable effect; inspection label is Lighting. Original author/license notices remain. Existing configuration keys retained for compatibility.

Regression smoke: eight frames, 1-3 passes, queue changes, alternating extents, experimental effect enabled, zero non-finite output or timeout/skips. In-game validation still required.
