# Diff report: cmd_23_eyescan

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 37 (raw 231), actual states: 37 (raw 231)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +106908 cycles (+6.682 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 23 EyeScan(2,100)"
- Baseline: cycle 1622078 (101.380 ms, raw seq 8); actual: cycle 1728986 (108.062 ms, raw seq 8)

```
    BASELINE                ACTUAL
    ########                ########
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 37
- max |drift|: 110083 cycles (6.880 ms)
- final drift: -524 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 23 EyeScan(2,100) | 100.0 | +106908 |

## Canonical hash
- baseline: `f10b2f89db49cfe5af73f034bd0c23be99eeb7ad27ffae979ddb0129e556785a`
- actual:   `f10b2f89db49cfe5af73f034bd0c23be99eeb7ad27ffae979ddb0129e556785a`
- MATCH

