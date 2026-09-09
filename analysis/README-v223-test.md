# Spider-Man v2.23 test build

Installed test DLL only; public package remains v2.22.

v2.22 still fails in Spider-Man. Captures took 4.4-6.1 seconds; the asynchronous guard prevents repeat freezes but disables neural processing. The GPU smoke at 1624x1624 completes without this delay, so pixel count alone does not explain it.

This candidate isolates a pending neural command list into its own ExecuteCommandLists call, preserving prefix/target/suffix order and executing each list once. It retains asynchronous single-pass completion, resource lifetime guards and the five-second stop. This tests the batch scheduling hypothesis; it is NOT an in-game verified fix. New logs identify isolated batches, queue type and separate native/fence completion values when stopped.

Release build passed. Eight GPU frames at 1624x1624 with a queue dependency signalled only after Submitted returns passed, no timeouts, finite output. This harness does not reproduce the Spider-Man render batch.
