# Drivers

**The Mission:** Minimal, robust interfaces to essential hardware, prioritized for diagnostics and timing.

## Invariants

### 1. The Yield-Check instrumentation
Driver loops that wait for hardware (like UART buffers) must be verifiable.
In `serial.salt`:
```salt
while !is_transmit_empty() {
    # Implicitly safe because it does not allocate or contend
}
```

### 2. Calibration
The Programmable Interval Timer (PIT) is explicitly calibrated to 100Hz to guarantee a stable scheduler tick, even if we run in cooperative mode most of the time.

## Components

| File | Role | Key Register |
|------|------|--------------|
| [`serial.salt`](./serial.salt) | **UART Output.** The voice of the kernel. | `0x3F8` (COM1 Data) |
| [`pit.salt`](./pit.salt) | **System Timer.** The heartbeat. | `0x43`, `0x40` |

## Entry Points
- **Logging:** `serial.print(msg)` is the primary way to debug the kernel.
