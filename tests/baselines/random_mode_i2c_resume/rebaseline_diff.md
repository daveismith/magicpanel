# Diff report: random_mode_i2c_resume

**FAIL**

- Baseline firmware flash/elf sha256: `9b5cea576d356c1b1e2b66cd60841a63b48cd0a2235f7f3161dbae8ea3829dfc`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 10 (raw 103), actual states: 10 (raw 103)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #6 is +8709 cycles (+0.544 ms) from baseline (tolerance 8000)
- Affected pattern: during "gpio C1=0"
- Baseline: cycle 12415816 (775.989 ms, raw seq 62); actual: cycle 12424525 (776.533 ms, raw seq 62)

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

- states compared: 10
- max |drift|: 8862 cycles (0.554 ms)
- final drift: +627 cycles

| marker | at ms | drift at next state |
|---|---|---|
| gpio C0=0 | 0.0 | +351 |
| gpio C1=0 | 0.0 | +351 |
| cmd 20 Cross | 1000.0 | +542 |

## Canonical hash
- baseline: `6b88ad66b446910d232a7910c27ce87c1190778e52be019287bb7d1b46f86e11`
- actual:   `6b88ad66b446910d232a7910c27ce87c1190778e52be019287bb7d1b46f86e11`
- MATCH

