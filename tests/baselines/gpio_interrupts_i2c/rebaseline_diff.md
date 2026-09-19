# Diff report: gpio_interrupts_i2c

**FAIL**

- Baseline firmware flash/elf sha256: `9b5cea576d356c1b1e2b66cd60841a63b48cd0a2235f7f3161dbae8ea3829dfc`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 25 (raw 380), actual states: 26 (raw 380)
- Timing tolerance: 8000 cycles

## RESULT: FAIL — display sequence diverges at state #5 (content differs)

- Affected pattern: during "gpio C1=0" (sent at 2000.0 ms)
- Baseline state #5: cycle 32712243 (2044.515 ms, raw seq 86)
- Actual state #5: cycle 32407409 (2025.463 ms, raw seq 70)

### State at divergence (baseline | actual)
```
    BASELINE                ACTUAL
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    intensity 15/15         intensity 15/15
```

### Preceding 3 states (identical in both)
```
  #2  baseline cycle 9977484 (623.593 ms, raw seq 30)  |  actual cycle 9978114 (623.632 ms, raw seq 30)
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    intensity 15/15         intensity 15/15
  #3  baseline cycle 18102590 (1131.412 ms, raw seq 46)  |  actual cycle 18102994 (1131.437 ms, raw seq 46)
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
  #4  baseline cycle 26227277 (1639.205 ms, raw seq 62)  |  actual cycle 26227810 (1639.238 ms, raw seq 62)
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    intensity 15/15         intensity 15/15
```

### Following 3 states
```
  #6  baseline cycle 36034337 (2252.146 ms, raw seq 102)  |  actual cycle 32712927 (2044.558 ms, raw seq 86)
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    intensity 15/15         intensity 15/15
  #7  baseline cycle 39356466 (2459.779 ms, raw seq 118)  |  actual cycle 36035225 (2252.202 ms, raw seq 102)
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    intensity 15/15         intensity 15/15
  #8  baseline cycle 42678515 (2667.407 ms, raw seq 134)  |  actual cycle 39357433 (2459.840 ms, raw seq 118)
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    ........                ########   <-- differs
    intensity 15/15         intensity 15/15
```

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

- states compared: 5
- max |drift|: 61039 cycles (3.815 ms)
- final drift: +533 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 5 Toggle(10) | 100.0 | +61039 |
| gpio C1=0 | 2000.0 | n/a |

## Canonical hash
- baseline: `62039155562b7e7dfaea66b18b85c7af5f2a411afae70d1ffb3568a4aa8f10ee`
- actual:   `9b388a6554cef71bedd8b8aebd30b50e3324a62ea01a0a76c6692bbe2c0c636c`
- MISMATCH

