# Diff report: cmd_22_cylonrow

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 30 (raw 119), actual states: 30 (raw 119)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +106915 cycles (+6.682 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 22 CylonRow(2,140)"
- Baseline: cycle 1744094 (109.006 ms, raw seq 8); actual: cycle 1851009 (115.688 ms, raw seq 8)

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

- states compared: 30
- max |drift|: 106915 cycles (6.682 ms)
- final drift: +80255 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 22 CylonRow(2,140) | 100.0 | +106915 |

## Canonical hash
- baseline: `d3bc49efa17dd832114977879d694d955adda534d74a815684b3c059edc11571`
- actual:   `d3bc49efa17dd832114977879d694d955adda534d74a815684b3c059edc11571`
- MATCH

