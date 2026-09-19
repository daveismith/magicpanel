# Diff report: reg_start_stop

**FAIL**

- Baseline firmware flash/elf sha256: `a160129b16f27cf186477b1f56fbba69e64dce4e0e4d8aca8492c6c08cbaeff5`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 16 (raw 159), actual states: 16 (raw 159)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #7 is +8883 cycles (+0.555 ms) from baseline (tolerance 8000)
- Affected pattern: during "reg 0x20 START [21, 0]"
- Baseline: cycle 29659001 (1853.688 ms, raw seq 86); actual: cycle 29667884 (1854.243 ms, raw seq 86)

```
    BASELINE                ACTUAL
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 16
- max |drift|: 9349 cycles (0.584 ms)
- final drift: -7393 cycles

| marker | at ms | drift at next state |
|---|---|---|
| reg 0x20 START [20] | 100.0 | +288 |
| reg 0x20 START [21, 0] | 1100.0 | -7246 |
| reg 0x21 STOP [1] | 3000.0 | -7393 |
| reg 0x21 STOP [0] | 4000.0 | -7393 |

## Canonical hash
- baseline: `85270f8e70b506818784ff742eeec2d08652c95212884eb300cf7a8726dbdaa9`
- actual:   `85270f8e70b506818784ff742eeec2d08652c95212884eb300cf7a8726dbdaa9`
- MATCH

