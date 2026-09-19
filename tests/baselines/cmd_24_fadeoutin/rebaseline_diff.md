# Diff report: cmd_24_fadeoutin

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 30 (raw 341), actual states: 30 (raw 341)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #4 is +8916 cycles (+0.557 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 24 FadeOutIn(1) out then in"
- Baseline: cycle 9492178 (593.261 ms, raw seq 58); actual: cycle 9501094 (593.818 ms, raw seq 58)

```
    BASELINE                ACTUAL
    ###..###                ###..###
    ######.#                ######.#
    ########                ########
    #.##.#.#                #.##.#.#
    ...#####                ...#####
    ########                ########
    #####..#                #####..#
    #.##.###                #.##.###
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 30
- max |drift|: 49301 cycles (3.081 ms)
- final drift: +5438 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 24 FadeOutIn(1) out then in | 100.0 | +703 |

## Canonical hash
- baseline: `bf5689a836b0085cf62dd7617495d886b04e29fb656bd647b75f5efbedaf2173`
- actual:   `bf5689a836b0085cf62dd7617495d886b04e29fb656bd647b75f5efbedaf2173`
- MATCH

