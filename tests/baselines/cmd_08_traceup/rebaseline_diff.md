# Diff report: cmd_08_traceup

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 42 (raw 167), actual states: 42 (raw 167)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is -105194 cycles (-6.575 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 8 TraceUp(5,1) fill"
- Baseline: cycle 1850145 (115.634 ms, raw seq 8); actual: cycle 1744951 (109.059 ms, raw seq 8)

```
    BASELINE                ACTUAL
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ########                ########
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 42
- max |drift|: 112041 cycles (7.003 ms)
- final drift: +6033 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 8 TraceUp(5,1) fill | 100.0 | -105194 |

## Canonical hash
- baseline: `bd421b80904e12c009d5d56f8ebff1f3964685839fae2a5194093313c0f3484d`
- actual:   `bd421b80904e12c009d5d56f8ebff1f3964685839fae2a5194093313c0f3484d`
- MATCH

