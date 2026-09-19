# Diff report: cmd_32_onetest

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 66 (raw 87), actual states: 66 (raw 87)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +99386 cycles (+6.212 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 32 OneTest(30)"
- Baseline: cycle 1743765 (108.985 ms, raw seq 7); actual: cycle 1843151 (115.197 ms, raw seq 7)

```
    BASELINE                ACTUAL
    .......#                .......#
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

- states compared: 66
- max |drift|: 115881 cycles (7.243 ms)
- final drift: -86428 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 32 OneTest(30) | 100.0 | +99386 |

## Canonical hash
- baseline: `cf8766b9df2258603b16f2af0888bce5fd111dbfe1648a2e0aa6c774f2a983d0`
- actual:   `cf8766b9df2258603b16f2af0888bce5fd111dbfe1648a2e0aa6c774f2a983d0`
- MATCH

