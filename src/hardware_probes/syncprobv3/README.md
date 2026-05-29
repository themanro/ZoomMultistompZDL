# SyncPrV3 — audio-context state-callback probe

The smallest possible test of TapeEcho3's tempo-sync algorithm. The
audio function on each block calls `state[31]` twice and `state[24]`
once (using the firmware function pointers at the fixed addresses
`c00b820c` and `c00d4b40`), computes a delay value, and turns it into
audible gain.

**No handler patches.** The LineSel handler does its normal job
(read SYNC knob, normalize, post to UI via `state[7]`). All the
sync-computation logic lives in the audio function.

## Why this approach

The BpmHunt probe series (v1..v4) tried to find a firmware-RAM
address that holds BPM. None was found — see
[docs/TEMPO-SYNC.md §4](../../../docs/TEMPO-SYNC.md). The static
re-trace of TapeEcho3 showed the reason: the host never exposes BPM
as a global. It folds tempo into `state[24]`'s output and into a
secondary firmware table at `0xc009fe90`.

So instead of finding BPM, we **call the same host functions
TapeEcho3 calls** and use whatever delay value the host returns.

## The algorithm (verbatim from `DLY_EP3_Calc_DelayTime`)

```c
typedef int (*ti_state_fn)(int a4, int b4);
ti_state_fn state31 = (ti_state_fn)0xc00b820cu;
ti_state_fn state24 = (ti_state_fn)0xc00d4b40u;

int slot = 0;                              // hardcoded — see "Slot" below
int sync_value = state31(slot, 6);         // 0..15, 0 = OFF
int delay;
if (sync_value == 0) {
    delay = state31(slot, 4) + 10;         // free-time path
} else {
    int byte = (slot - 1) & 0xff;
    int x = state31(byte, 0x0f3c);         // reads 0xc009fe90 + 44*byte
    int y = state24(x, 100);               // BPM-aware host math
    delay = y / 100;
}

float gain = (float)abs(delay) * (2.0f / 2000.0f);  // 0..2000 -> 0..2, clipped
```

## Slot

The probe **must** be loaded in slot 0 (left-most position in the
patch chain). `state31(slot=0, B4=6)` reads slot 0's `SYNC` knob
value. If the probe is in another slot, it will read that slot's
SYNC value via the wrong index — could be 0 / could be garbage.

A v4 of the probe could discover its own slot via a state callback
(if we can find one that returns "the current slot index"). For
now, hardcoded 0.

## Test plan

| step | action | expected if probe works |
|------|--------|-------------------------|
| 1 | Load `SyncPrV3.ZDL` into slot 0 | shows up under Filter, loads without freeze |
| 2 | Unbypass with `Sync` = OFF (0) | quiet/silent — delay = raw_TIME + 10, small number → small gain |
| 3 | Sweep `Sync` to 1 (first division) | gain rises — state[24] returns a host-computed delay value |
| 4 | At Sync = 1, tap tempo ~120 BPM, listen for ~3 seconds | a stable audible gain level |
| 5 | At Sync = 1, tap tempo ~60 BPM, listen for ~3 seconds | gain level CHANGES vs step 4 (the delay value is BPM-tracked) |
| 6 | Sweep Sync through 2..15 | each division should produce a different audible gain (different state[24] output for each division) |

The **critical observation** is step 5: if gain changes between BPM
60 and BPM 120 at the same SYNC setting, the host is folding tempo
into the returned delay value — meaning we've successfully imported
sync into a custom effect.

## Failure modes

* **Pedal freezes on load** — the audio context cannot call firmware
  functions at `c00b820c`/`c00d4b40`. Backup plan: move the algorithm
  into a custom handler (binary patch or compiled C handler).
* **Pedal freezes on Sync != 0** — state[24] fault. The args might be
  wrong (e.g., `B4 = 100` should be different from audio context).
* **Audio plays but gain is constant across BPM changes** — state[24]
  returned a constant. Either it early-exited (input was 0.0f as float)
  or it doesn't fold BPM in this calling shape. Investigation: try
  different `B4` values (TapeEcho3 used 100, but the host may need a
  different sample-rate-like constant in audio context).
* **Gain changes between SYNC values but not between BPMs** — the
  secondary table at `0xc009fe90` holds static division constants,
  and `state[24]` does the BPM math from elsewhere. We'd then need to
  find what `state[24]` reads internally.

## How it was built

```
python3 src/hardware_probes/syncprobv3/build.py
```

Uses unpatched `linesel_handlers.bin` — all the magic is in the C
source.
