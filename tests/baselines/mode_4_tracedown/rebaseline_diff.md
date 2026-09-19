# Diff report: mode_4_tracedown

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 89 (raw 339), actual states: 89 (raw 339)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +107781 cycles (+6.736 ms) from baseline (tolerance 8000)
- Affected pattern: during "gpio C0=0"
- Baseline: cycle 476184 (29.762 ms, raw seq 8); actual: cycle 583965 (36.498 ms, raw seq 8)

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

- states compared: 89
- max |drift|: 118961 cycles (7.435 ms)
- final drift: -31889 cycles

| marker | at ms | drift at next state |
|---|---|---|
| gpio C0=0 | 0.0 | +915 |

## Canonical hash
- baseline: `e38a7fa7d86a6d3273bc93f26458459c568de8682837aa953fdfa65d3a79e542`
- actual:   `e38a7fa7d86a6d3273bc93f26458459c568de8682837aa953fdfa65d3a79e542`
- MATCH

