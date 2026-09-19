# Diff report: cmd_35_quadrant_type1

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 22 (raw 167), actual states: 22 (raw 167)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +69051 cycles (+4.316 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 35 Quadrant(5,1) TL,TR,BR,BL"
- Baseline: cycle 1782984 (111.436 ms, raw seq 10); actual: cycle 1852035 (115.752 ms, raw seq 10)

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
- max |drift|: 72073 cycles (4.505 ms)
- final drift: -63679 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 35 Quadrant(5,1) TL,TR,BR,BL | 100.0 | +69051 |

## Canonical hash
- baseline: `a0bcb23b10b360863441a405aa4a29ab1fc196023d07a730557b3318f5977fdb`
- actual:   `a0bcb23b10b360863441a405aa4a29ab1fc196023d07a730557b3318f5977fdb`
- MATCH

