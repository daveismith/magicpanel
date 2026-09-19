# Diff report: cmd_36_quadrant_type2

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 22 (raw 167), actual states: 22 (raw 167)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +53890 cycles (+3.368 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 36 Quadrant(5,2) TR,TL,BL,BR"
- Baseline: cycle 1790615 (111.913 ms, raw seq 10); actual: cycle 1844505 (115.282 ms, raw seq 10)

```
    BASELINE                ACTUAL
    ....####                ....####
    ....####                ....####
    ....####                ....####
    ....####                ....####
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 22
- max |drift|: 67062 cycles (4.191 ms)
- final drift: -48509 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 36 Quadrant(5,2) TR,TL,BL,BR | 100.0 | +53890 |

## Canonical hash
- baseline: `5335489f389c584d822e6c55d511ef3060f7ff8c468ea97be6d6ca8b180818b6`
- actual:   `5335489f389c584d822e6c55d511ef3060f7ff8c468ea97be6d6ca8b180818b6`
- MATCH

