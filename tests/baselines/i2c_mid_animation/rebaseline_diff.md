# Diff report: i2c_mid_animation

**FAIL**

- Baseline firmware flash/elf sha256: `b1c590f3881bd7a42986efaa85009567aec39c0ecc7b4c11beaa25ac0660079f`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 7 (raw 95), actual states: 7 (raw 95)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +61081 cycles (+3.818 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 5 Toggle(10)"
- Baseline: cycle 1792326 (112.020 ms, raw seq 14); actual: cycle 1853407 (115.838 ms, raw seq 14)

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

- states compared: 7
- max |drift|: 61081 cycles (3.818 ms)
- final drift: +695 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 5 Toggle(10) | 100.0 | +61081 |
| cmd 20 Cross | 2100.0 | +507 |

## Canonical hash
- baseline: `15a16e90ae896f5fcd9ca6ae525ca753eb1c9af02e525834bbadba060ca01f7f`
- actual:   `15a16e90ae896f5fcd9ca6ae525ca753eb1c9af02e525834bbadba060ca01f7f`
- MATCH

