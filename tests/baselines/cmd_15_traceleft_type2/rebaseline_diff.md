# Diff report: cmd_15_traceleft_type2

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 42 (raw 407), actual states: 42 (raw 407)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #6 is +9184 cycles (+0.574 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 15 TraceLeft(5,2) single column"
- Baseline: cycle 18467697 (1154.231 ms, raw seq 62); actual: cycle 18476881 (1154.805 ms, raw seq 62)

```
    BASELINE                ACTUAL
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    ..#.....                ..#.....
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 42
- max |drift|: 15221 cycles (0.951 ms)
- final drift: +15212 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 15 TraceLeft(5,2) single column | 100.0 | -6670 |

## Canonical hash
- baseline: `ec9e150ad5c630447c9cd11c7f9732e831d21d09b7a8e9f18a32932c93621831`
- actual:   `ec9e150ad5c630447c9cd11c7f9732e831d21d09b7a8e9f18a32932c93621831`
- MATCH

