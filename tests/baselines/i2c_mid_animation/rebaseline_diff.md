# Diff report: i2c_mid_animation

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `b1c590f3881bd7a42986efaa85009567aec39c0ecc7b4c11beaa25ac0660079f`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 23 (raw 351), actual states: 7 (raw 95)
- Timing tolerance: 8000 cycles

## RESULT: FAIL — display sequence diverges at state #6 (content differs)

- Affected pattern: during "cmd 20 Cross" (sent at 2100.0 ms)
- Baseline state #6: cycle 82041446 (5127.590 ms, raw seq 102)
- Actual state #6: cycle 81960006 (5122.500 ms, raw seq 94)

### State at divergence (baseline | actual)
```
    BASELINE                ACTUAL
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ########                ........   <-- differs
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
```

### Preceding 3 states (identical in both)
```
  #3  baseline cycle 18101689 (1131.356 ms, raw seq 46)  |  actual cycle 18102129 (1131.383 ms, raw seq 46)
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    intensity 15/15         intensity 15/15
  #4  baseline cycle 26226355 (1639.147 ms, raw seq 62)  |  actual cycle 26226769 (1639.173 ms, raw seq 62)
    ........                ........
    ........                ........
    ........                ........
    ........                ........
    ########                ########
    ########                ########
    ########                ########
    ########                ########
    intensity 15/15         intensity 15/15
  #5  baseline cycle 33837373 (2114.836 ms, raw seq 82)  |  actual cycle 33837961 (2114.873 ms, raw seq 82)
    ........                ........
    .#....#.                .#....#.
    ..#..#..                ..#..#..
    ...##...                ...##...
    ...##...                ...##...
    ..#..#..                ..#..#..
    .#....#.                .#....#.
    ........                ........
    intensity 15/15         intensity 15/15
```

### Following 3 states
```
  #7  baseline cycle 90226581 (5639.161 ms, raw seq 118)  |  actual absent
    ........                        
    ........                        
    ........                        
    ........                        
    ########                        
    ########                        
    ########                        
    ########                        
    intensity 15/15         (absent)
  #8  baseline cycle 98351170 (6146.948 ms, raw seq 134)  |  actual absent
    ########                        
    ########                        
    ########                        
    ########                        
    ........                        
    ........                        
    ........                        
    ........                        
    intensity 15/15         (absent)
  #9  baseline cycle 106475854 (6654.741 ms, raw seq 150)  |  actual absent
    ........                        
    ........                        
    ........                        
    ........                        
    ########                        
    ########                        
    ########                        
    ########                        
    intensity 15/15         (absent)
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 6
- max |drift|: 588 cycles (0.037 ms)
- final drift: +588 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 5 Toggle(10) | 100.0 | +365 |
| cmd 20 Cross | 2100.0 | +588 |

## Canonical hash
- baseline: `71cc7f78cdb79222414367d31b8ea8ad92ca1f4eca893dbbe0e26d7fd6ef348e`
- actual:   `15a16e90ae896f5fcd9ca6ae525ca753eb1c9af02e525834bbadba060ca01f7f`
- MISMATCH

