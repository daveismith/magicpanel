# Diff report: gpio_switch_mode

**FAIL**

- Baseline firmware flash/elf sha256: `9b5cea576d356c1b1e2b66cd60841a63b48cd0a2235f7f3161dbae8ea3829dfc`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 51 (raw 423), actual states: 51 (raw 423)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #22 is -14433 cycles (-0.902 ms) from baseline (tolerance 8000)
- Affected pattern: during "gpio C1=1"
- Baseline: cycle 40483948 (2530.247 ms, raw seq 112); actual: cycle 40469515 (2529.345 ms, raw seq 112)

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

- states compared: 51
- max |drift|: 80471 cycles (5.029 ms)
- final drift: +13605 cycles

| marker | at ms | drift at next state |
|---|---|---|
| gpio C1=0 | 0.0 | +351 |
| gpio C2=0 | 0.0 | +351 |
| gpio C1=1 | 2000.0 | +240 |

## Canonical hash
- baseline: `f478147c1f5794561e8c88a66713faa4f76bd09f239995f5fd9831e81199373b`
- actual:   `f478147c1f5794561e8c88a66713faa4f76bd09f239995f5fd9831e81199373b`
- MATCH

