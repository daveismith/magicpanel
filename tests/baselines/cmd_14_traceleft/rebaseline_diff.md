# Diff report: cmd_14_traceleft

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 42 (raw 407), actual states: 42 (raw 407)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #5 is +8887 cycles (+0.555 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 14 TraceLeft(5,1) fill"
- Baseline: cycle 15143353 (946.460 ms, raw seq 46); actual: cycle 15152240 (947.015 ms, raw seq 46)

```
    BASELINE                ACTUAL
    ...#####                ...#####
    ...#####                ...#####
    ...#####                ...#####
    ...#####                ...#####
    ...#####                ...#####
    ...#####                ...#####
    ...#####                ...#####
    ...#####                ...#####
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 42
- max |drift|: 15545 cycles (0.972 ms)
- final drift: +7997 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 14 TraceLeft(5,1) fill | 100.0 | -6668 |

## Canonical hash
- baseline: `7643d488ca096b1ae47b3f70b3997bf8f3d0f27b9c75480a09f402a032d1285e`
- actual:   `7643d488ca096b1ae47b3f70b3997bf8f3d0f27b9c75480a09f402a032d1285e`
- MATCH

