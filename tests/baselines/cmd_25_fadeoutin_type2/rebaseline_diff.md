# Diff report: cmd_25_fadeoutin_type2

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 16 (raw 171), actual states: 16 (raw 171)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #4 is +8860 cycles (+0.554 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 25 FadeOutIn(2) out only"
- Baseline: cycle 9492234 (593.265 ms, raw seq 58); actual: cycle 9501094 (593.818 ms, raw seq 58)

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

- states compared: 16
- max |drift|: 27316 cycles (1.707 ms)
- final drift: +18550 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 25 FadeOutIn(2) out only | 100.0 | +701 |

## Canonical hash
- baseline: `51f4528bf8896b8c03378e7b0a1bccbd5d5296cfc79a84ecee80cdd0371313c6`
- actual:   `51f4528bf8896b8c03378e7b0a1bccbd5d5296cfc79a84ecee80cdd0371313c6`
- MATCH

