# Diff report: cmd_38_quadrant_type4

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 22 (raw 167), actual states: 22 (raw 167)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +69052 cycles (+4.316 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 38 Quadrant(5,4) TL,BL,BR,TR"
- Baseline: cycle 1782995 (111.437 ms, raw seq 10); actual: cycle 1852047 (115.753 ms, raw seq 10)

```
    BASELINE                ACTUAL
    ####....                ####....
    ####....                ####....
    ####....                ####....
    ####....                ####....
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 22
- max |drift|: 73442 cycles (4.590 ms)
- final drift: +59258 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 38 Quadrant(5,4) TL,BL,BR,TR | 100.0 | +69052 |

## Canonical hash
- baseline: `01866caba9b57f62ec8e5c63745548d8ccdd1f72f63cbedb320dd1eda4707a0b`
- actual:   `01866caba9b57f62ec8e5c63745548d8ccdd1f72f63cbedb320dd1eda4707a0b`
- MATCH

