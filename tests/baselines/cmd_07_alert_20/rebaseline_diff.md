# Diff report: cmd_07_alert_20

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `ac17cdd1967340dfa79e7eb532007891c6bf18e90930b6eb60cf8a32b11c516a`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 41 (raw 647), actual states: 41 (raw 647)
- Timing tolerance: 8000 cycles

## Display sequence: MATCH (all states identical, ignoring timing)

## Timing: FAIL — state #13 is +8012 cycles (+0.501 ms) from baseline (tolerance 8000)
- Affected pattern: during "cmd 7 Alert(20)"
- Baseline: cycle 56443849 (3527.741 ms, raw seq 214); actual: cycle 56451861 (3528.241 ms, raw seq 214)

```
    BASELINE                ACTUAL
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 41
- max |drift|: 24038 cycles (1.502 ms)
- final drift: +24038 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 7 Alert(20) | 100.0 | +829 |

## Canonical hash
- baseline: `8500e26d65a73f89cb979e149f4f93ac69726ca80fd6ddf100150d6391e625af`
- actual:   `8500e26d65a73f89cb979e149f4f93ac69726ca80fd6ddf100150d6391e625af`
- MATCH

