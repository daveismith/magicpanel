# Diff report: i2c_switch_no_resume

**FAIL**

- Baseline firmware flash/elf sha256: `9b5cea576d356c1b1e2b66cd60841a63b48cd0a2235f7f3161dbae8ea3829dfc`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 21 (raw 311), actual states: 21 (raw 311)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #1 is +61039 cycles (+3.815 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 5 Toggle(10)"
- Baseline: cycle 1792368 (112.023 ms, raw seq 14); actual: cycle 1853407 (115.838 ms, raw seq 14)

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

- states compared: 21
- max |drift|: 61039 cycles (3.815 ms)
- final drift: +2911 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 5 Toggle(10) | 100.0 | +61039 |
| cmd 26 FlashAll(8,200) | 2100.0 | +61011 |

## Canonical hash
- baseline: `353e90f09971279def371de0df093de5fb8694aecbdf5177aa9662eeb0a03332`
- actual:   `353e90f09971279def371de0df093de5fb8694aecbdf5177aa9662eeb0a03332`
- MATCH

