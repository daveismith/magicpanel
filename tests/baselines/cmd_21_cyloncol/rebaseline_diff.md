# Diff report: cmd_21_cyloncol

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 30 (raw 271), actual states: 30 (raw 271)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #6 is +8862 cycles (+0.554 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 21 CylonCol(2,140)"
- Baseline: cycle 13655218 (853.451 ms, raw seq 62); actual: cycle 13664080 (854.005 ms, raw seq 62)

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

- states compared: 30
- max |drift|: 10558 cycles (0.660 ms)
- final drift: -4309 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 21 CylonCol(2,140) | 100.0 | -6661 |

## Canonical hash
- baseline: `c9f9354bc568b4627300dc9381d2e26fc7bbc3112e2a5fe5281e2c37db7a103e`
- actual:   `c9f9354bc568b4627300dc9381d2e26fc7bbc3112e2a5fe5281e2c37db7a103e`
- MATCH

