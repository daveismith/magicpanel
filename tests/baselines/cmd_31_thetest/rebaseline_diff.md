# Diff report: cmd_31_thetest

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 129 (raw 135), actual states: 129 (raw 135)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +99384 cycles (+6.212 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 31 TheTest(30)"
- Baseline: cycle 1743767 (108.985 ms, raw seq 7); actual: cycle 1843151 (115.197 ms, raw seq 7)

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

- states compared: 129
- max |drift|: 125247 cycles (7.828 ms)
- final drift: -78876 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 31 TheTest(30) | 100.0 | +99384 |

## Canonical hash
- baseline: `ba29d07437361da77324c8353c4c6178e48ff6cc5cf903818783abc32cb77284`
- actual:   `ba29d07437361da77324c8353c4c6178e48ff6cc5cf903818783abc32cb77284`
- MATCH

