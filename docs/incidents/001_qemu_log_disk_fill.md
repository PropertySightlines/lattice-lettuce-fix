# INC-001: QEMU Debug Log Fills Disk (294GB)

**Date:** 2026-02-20
**Severity:** P1 (development blocked)
**Root cause:** Unbounded QEMU log file with interrupt tracing enabled

## What Happened

A QEMU process running the kernel benchmark suite was left active for ~1.5 hours.
QEMU's `-d int` debug flag writes a trace line for every hardware interrupt serviced.
The VirtIO-Net driver triggers IRQ 0x2B at a high rate during the polling loop, which
produced millions of trace lines per second. The log file `qemu.log` grew to **294GB**,
filling the entire 460GB disk to 100% and blocking all subsequent builds.

## Timeline

1. `runner_qemu.py build` was invoked but the QEMU process was not terminated.
2. Over ~90 minutes, `qemu.log` grew from 0 to 294GB.
3. A subsequent `runner_qemu.py build` failed with `OSError: [Errno 28] No space left on device`.
4. `du -d1` on the project directory showed 294GB but visible subdirectories totalled only ~416MB.
5. `find -size +100M` located the single `qemu.log` file as the sole offender.
6. `pkill -f qemu-system && rm qemu.log` freed all 294GB instantly.

## Why It Happened

- The QEMU command includes `-d int` for interrupt debugging. This flag is useful during
  driver development but produces enormous output under sustained interrupt load.
- The runner script has a timeout, but if the user runs QEMU manually or the timeout
  mechanism fails, the process persists indefinitely.
- `qemu.log` was not in `.gitignore` (it is now implicitly ignored by not being tracked,
  but there was no size guard).

## Fixes Applied

1. **Log guard in `runner_qemu.py`**: Before launching QEMU, the runner now deletes any
   stale `qemu.log` exceeding 100MB. After QEMU exits, the log is truncated to prevent
   accumulation across runs.
2. **Stale process guard**: The runner now kills any existing `qemu-system` processes
   before launching a new instance.
3. **`.gitignore` entry**: `qemu.log` is explicitly added to `.gitignore`.

## Prevention

- Never leave a QEMU process running with `-d int` unattended.
- Consider using `-d int -D /dev/null` if interrupt tracing is not needed.
- The `runner_qemu.py` guards will catch this automatically going forward.
