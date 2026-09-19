# Diff report: cmd_10_tracedown

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 42 (raw 167), actual states: 42 (raw 167)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +106889 cycles (+6.681 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 10 TraceDown(5,1) fill"
- Baseline: cycle 1744103 (109.006 ms, raw seq 8); actual: cycle 1850992 (115.687 ms, raw seq 8)

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

- states compared: 42
- max |drift|: 110958 cycles (6.935 ms)
- final drift: +5674 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 10 TraceDown(5,1) fill | 100.0 | +106889 |

## Canonical hash
- baseline: `46ed23260765241e4a3ba1e6134da14f048b653923e2fd7f4e6d301243a00dcf`
- actual:   `46ed23260765241e4a3ba1e6134da14f048b653923e2fd7f4e6d301243a00dcf`
- MATCH

