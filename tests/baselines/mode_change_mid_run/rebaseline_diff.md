# Diff report: mode_change_mid_run

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `b1c590f3881bd7a42986efaa85009567aec39c0ecc7b4c11beaa25ac0660079f`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 30 (raw 471), actual states: 21 (raw 327)
- Timing tolerance: 8000 cycles

## RESULT: FAIL — display sequence diverges at state #21 (actual run is MISSING states)

- Affected pattern: during "gpio C1=1" (sent at 5000.0 ms)
- Baseline state #21: cycle 83080067 (5192.504 ms, raw seq 342)
- Actual state #21: absent

### State at divergence (baseline | actual)
```
    BASELINE                ACTUAL
    ########                        
    ########                        
    ########                        
    ########                        
    ########                        
    ########                        
    ########                        
    ########                        
    intensity 15/15         (absent)
```

### Preceding 3 states (identical in both)
```
  #18  baseline cycle 73113568 (4569.598 ms, raw seq 294)  |  actual cycle 73441614 (4590.101 ms, raw seq 294)
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
  #19  baseline cycle 76435815 (4777.238 ms, raw seq 310)  |  actual cycle 76763905 (4797.744 ms, raw seq 310)
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    intensity 15/15         intensity 15/15
  #20  baseline cycle 79757887 (4984.868 ms, raw seq 326)  |  actual cycle 80085964 (5005.373 ms, raw seq 326)
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

### Following 3 states
```
  #22  baseline cycle 86402215 (5400.138 ms, raw seq 358)  |  actual absent
    ........                        
    ........                        
    ........                        
    ........                        
    ........                        
    ........                        
    ........                        
    ........                        
    intensity 15/15         (absent)
  #23  baseline cycle 89724490 (5607.781 ms, raw seq 374)  |  actual absent
    ########                        
    ########                        
    ########                        
    ########                        
    ########                        
    ########                        
    ########                        
    ########                        
    intensity 15/15         (absent)
  #24  baseline cycle 93046539 (5815.409 ms, raw seq 390)  |  actual absent
    ........                        
    ........                        
    ........                        
    ........                        
    ........                        
    ........                        
    ........                        
    ........                        
    intensity 15/15         (absent)
```

## Timing: FAIL — state #1 is +328572 cycles (+20.536 ms) from baseline (tolerance 8000)
- Affected pattern: during "gpio C1=0"
- Baseline: cycle 16392689 (1024.543 ms, raw seq 22); actual: cycle 16721261 (1045.079 ms, raw seq 22)

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

- states compared: 21
- max |drift|: 328572 cycles (20.536 ms)
- final drift: +328077 cycles

| marker | at ms | drift at next state |
|---|---|---|
| gpio C1=0 | 1000.0 | +328572 |
| gpio C1=1 | 5000.0 | +328077 |

## Canonical hash
- baseline: `38b899eef0660a5a907c7b96e0dcc0fc6674adb492a6f544ce600ebf8c2b6ed9`
- actual:   `44fab54dd348fd63766e940414029ef3e4493a733f6ed19633e30ab854fbc9eb`
- MISMATCH

