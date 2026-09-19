# Diff report: cmd_12_traceright

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 42 (raw 407), actual states: 42 (raw 407)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +8430 cycles (+0.527 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 12 TraceRight(5,1) fill"
- Baseline: cycle 1844935 (115.308 ms, raw seq 14); actual: cycle 1853365 (115.835 ms, raw seq 14)

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
- max |drift|: 15290 cycles (0.956 ms)
- final drift: +8429 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 12 TraceRight(5,1) fill | 100.0 | +8430 |

## Canonical hash
- baseline: `78e8058cb07eab58c2900caeb958edd97b0282221278ad93e0edce7359df1fb6`
- actual:   `78e8058cb07eab58c2900caeb958edd97b0282221278ad93e0edce7359df1fb6`
- MATCH

