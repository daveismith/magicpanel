# Diff report: cmd_30_oneloop

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 42 (raw 71), actual states: 42 (raw 71)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +84187 cycles (+5.262 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 30 OneLoop(2)"
- Baseline: cycle 1751307 (109.457 ms, raw seq 7); actual: cycle 1835494 (114.718 ms, raw seq 7)

```
    BASELINE                ACTUAL
    ........                ........
    .#......                .#......
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
- max |drift|: 86784 cycles (5.424 ms)
- final drift: +59726 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 30 OneLoop(2) | 100.0 | +84187 |

## Canonical hash
- baseline: `3b8913f64f33eaf67d57a6c7f2f3ce314eaee1b0a8f8d30ac286aa166ae34a68`
- actual:   `3b8913f64f33eaf67d57a6c7f2f3ce314eaee1b0a8f8d30ac286aa166ae34a68`
- MATCH

