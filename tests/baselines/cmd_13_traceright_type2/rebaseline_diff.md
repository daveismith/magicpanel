# Diff report: cmd_13_traceright_type2

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 42 (raw 407), actual states: 42 (raw 407)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +8399 cycles (+0.525 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 13 TraceRight(5,2) single column"
- Baseline: cycle 1844938 (115.309 ms, raw seq 14); actual: cycle 1853337 (115.834 ms, raw seq 14)

```
    BASELINE                ACTUAL
    #.......                #.......
    #.......                #.......
    #.......                #.......
    #.......                #.......
    #.......                #.......
    #.......                #.......
    #.......                #.......
    #.......                #.......
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 42
- max |drift|: 14987 cycles (0.937 ms)
- final drift: +277 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 13 TraceRight(5,2) single column | 100.0 | +8399 |

## Canonical hash
- baseline: `7dae725acf190acb16936ea8c876ed74bf3ab0cce007ece7fc52197c911be174`
- actual:   `7dae725acf190acb16936ea8c876ed74bf3ab0cce007ece7fc52197c911be174`
- MATCH

