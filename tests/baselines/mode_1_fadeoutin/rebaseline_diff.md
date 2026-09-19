# Diff report: mode_1_fadeoutin

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 64 (raw 715), actual states: 64 (raw 715)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #4 is -13136 cycles (-0.821 ms) from baseline (tolerance 8000)
- Affected pattern: during "gpio C2=0"
- Baseline: cycle 8353691 (522.106 ms, raw seq 60); actual: cycle 8340555 (521.285 ms, raw seq 60)

```
    BASELINE                ACTUAL
    ###.##.#                ###.##.#
    #..####.                #..####.
    .##.##.#                .##.##.#
    ########                ########
    #.######                #.######
    ######.#                ######.#
    .###.##.                .###.##.
    ########                ########
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 64
- max |drift|: 79618 cycles (4.976 ms)
- final drift: +4589 cycles

| marker | at ms | drift at next state |
|---|---|---|
| gpio C2=0 | 0.0 | +915 |

## Canonical hash
- baseline: `70831b41349b0674cb8c8a1d8cdadbd7ee7cb46f0ae035651b4314f0c5739a5b`
- actual:   `70831b41349b0674cb8c8a1d8cdadbd7ee7cb46f0ae035651b4314f0c5739a5b`
- MATCH

