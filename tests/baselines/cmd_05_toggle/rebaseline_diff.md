# Diff report: cmd_05_toggle

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 22 (raw 327), actual states: 22 (raw 327)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +61446 cycles (+3.840 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 5 Toggle(10)"
- Baseline: cycle 1791961 (111.998 ms, raw seq 14); actual: cycle 1853407 (115.838 ms, raw seq 14)

```
    BASELINE                ACTUAL
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 22
- max |drift|: 61446 cycles (3.840 ms)
- final drift: -56821 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 5 Toggle(10) | 100.0 | +61446 |

## Canonical hash
- baseline: `5d7c387444a62a83ace150520045888bd934d4b52d7019d739548a4c8b6b019b`
- actual:   `5d7c387444a62a83ace150520045888bd934d4b52d7019d739548a4c8b6b019b`
- MATCH

