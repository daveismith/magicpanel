# Diff report: gpio_rotary_transit

**FAIL**

- Baseline firmware flash/elf sha256: `9b5cea576d356c1b1e2b66cd60841a63b48cd0a2235f7f3161dbae8ea3829dfc`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 34 (raw 105), actual states: 34 (raw 105)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #19 is +106878 cycles (+6.680 ms) from baseline (tolerance 8000)
- Affected pattern: during "gpio C0=0"
- Baseline: cycle 32769975 (2048.123 ms, raw seq 60); actual: cycle 32876853 (2054.803 ms, raw seq 60)

```
    BASELINE                ACTUAL
    ########                ########
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 34
- max |drift|: 108301 cycles (6.769 ms)
- final drift: -72590 cycles

| marker | at ms | drift at next state |
|---|---|---|
| gpio C1=0 | 0.0 | +351 |
| gpio C2=0 | 0.0 | +351 |
| gpio C2=1 | 2000.0 | +328 |
| gpio C1=1 | 2005.0 | +328 |
| gpio C0=0 | 2010.0 | +328 |

## Canonical hash
- baseline: `687a7570d46d6ddbe7e5d178e0dc039557c3d7c15e5a1a9af2f79533d869261e`
- actual:   `687a7570d46d6ddbe7e5d178e0dc039557c3d7c15e5a1a9af2f79533d869261e`
- MATCH

