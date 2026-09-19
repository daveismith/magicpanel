# Diff report: reg_random_show

**FAIL**

- Baseline firmware flash/elf sha256: `a160129b16f27cf186477b1f56fbba69e64dce4e0e4d8aca8492c6c08cbaeff5`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 42 (raw 71), actual states: 42 (raw 71)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +83725 cycles (+5.233 ms) from baseline (tolerance 8000)
- Affected pattern: during "reg 0x20 START [40]"
- Baseline: cycle 1773793 (110.862 ms, raw seq 7); actual: cycle 1857518 (116.095 ms, raw seq 7)

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
- max |drift|: 86391 cycles (5.399 ms)
- final drift: +58233 cycles

| marker | at ms | drift at next state |
|---|---|---|
| reg 0x20 START [40] | 100.0 | +83725 |

## Canonical hash
- baseline: `3b8913f64f33eaf67d57a6c7f2f3ce314eaee1b0a8f8d30ac286aa166ae34a68`
- actual:   `3b8913f64f33eaf67d57a6c7f2f3ce314eaee1b0a8f8d30ac286aa166ae34a68`
- MATCH

