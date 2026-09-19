# Diff report: mode_3_twoloop

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 91 (raw 285), actual states: 91 (raw 285)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #42 is +8406 cycles (+0.525 ms) from baseline (tolerance 8000)
- Affected pattern: during "gpio C2=0"
- Baseline: cycle 82488182 (5155.511 ms, raw seq 136); actual: cycle 82496588 (5156.037 ms, raw seq 136)

```
    BASELINE                ACTUAL
    ........                ........
    .#......                .#......
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ......#.                ......#.
    ........                ........
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 91
- max |drift|: 17117 cycles (1.070 ms)
- final drift: +17117 cycles

| marker | at ms | drift at next state |
|---|---|---|
| gpio C1=0 | 0.0 | +915 |
| gpio C2=0 | 0.0 | +915 |

## Canonical hash
- baseline: `be4aa9152a736562c2bae0277c6ae6cc4fa4fa19369108195b98c8c718c9786a`
- actual:   `be4aa9152a736562c2bae0277c6ae6cc4fa4fa19369108195b98c8c718c9786a`
- MATCH

