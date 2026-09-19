# Diff report: cmd_27_flashv

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 18 (raw 263), actual states: 18 (raw 263)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #17 is +10632 cycles (+0.664 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 27 FlashV(8,200)"
- Baseline: cycle 54910919 (3431.932 ms, raw seq 262); actual: cycle 54921551 (3432.597 ms, raw seq 262)

```
    BASELINE                ACTUAL
    ........                ........
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

- states compared: 18
- max |drift|: 10632 cycles (0.664 ms)
- final drift: +10632 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 27 FlashV(8,200) | 100.0 | -6910 |

## Canonical hash
- baseline: `9595cdb38693da3ebafb05f447b63e304c8920ee9bda14bdac4a378c95fedcf3`
- actual:   `9595cdb38693da3ebafb05f447b63e304c8920ee9bda14bdac4a378c95fedcf3`
- MATCH

