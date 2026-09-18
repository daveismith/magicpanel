# Diff report: i2c_garbage

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `a160129b16f27cf186477b1f56fbba69e64dce4e0e4d8aca8492c6c08cbaeff5`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 3 (raw 31), actual states: 17 (raw 263)
- Timing tolerance: 8000 cycles

## RESULT: FAIL — display sequence diverges at state #1 (content differs)

- Affected pattern: during "cmd 26 FlashAll (runs)" (sent at 4500.0 ms)
- Baseline state #1: cycle 11438829 (714.927 ms, raw seq 18)
- Actual state #1: cycle 72128811 (4508.051 ms, raw seq 22)

### State at divergence (baseline | actual)
```
    BASELINE                ACTUAL
    ........                ########   <-- differs
    .#....#.                ########   <-- differs
    ..#..#..                ########   <-- differs
    ...##...                ########   <-- differs
    ...##...                ########   <-- differs
    ..#..#..                ########   <-- differs
    .#....#.                ########   <-- differs
    ........                ########   <-- differs
    intensity 15/15         intensity 15/15
```

### Preceding 3 states (identical in both)
```
  #0  baseline cycle 214208 (13.388 ms, raw seq 6)  |  actual cycle 215123 (13.445 ms, raw seq 6)
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
```

### Following 3 states
```
  #2  baseline cycle 59561017 (3722.564 ms, raw seq 30)  |  actual cycle 75450846 (4715.678 ms, raw seq 38)
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
  #3  baseline absent  |  actual cycle 78772956 (4923.310 ms, raw seq 54)
                            ########
                            ########
                            ########
                            ########
                            ########
                            ########
                            ########
                            ########
    (absent)                intensity 15/15
  #4  baseline absent  |  actual cycle 82095065 (5130.942 ms, raw seq 70)
                            ........
                            ........
                            ........
                            ........
                            ........
                            ........
                            ........
                            ........
    (absent)                intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 1
- max |drift|: 915 cycles (0.057 ms)
- final drift: +915 cycles

| marker | at ms | drift at next state |
|---|---|---|
| byte 200: sets the register pointer | 100.0 | n/a |
| empty write (address only) | 300.0 | n/a |
| Cross to foreign address 0x15 | 500.0 | n/a |
| 2-byte legacy write [20, 5]: rejected, receiver stays responsive | 700.0 | n/a |
| cmd 26 FlashAll (runs) | 4500.0 | n/a |
| read 2 bytes from 0x14 | 8500.0 | n/a |

## Canonical hash
- baseline: `67e9608b297b55aabeca870bbcd24bfa107061cce740585968ec27751817a052`
- actual:   `48adfdd71294928dc461a0e7cc8038fbfc49dbc3d54a39d24e53e84c4d314bb6`
- MISMATCH

