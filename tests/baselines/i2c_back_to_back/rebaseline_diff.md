# Diff report: i2c_back_to_back

**FAIL**

- Baseline firmware flash/elf sha256: `3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a`
- Actual firmware flash/elf sha256: `b1c590f3881bd7a42986efaa85009567aec39c0ecc7b4c11beaa25ac0660079f`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 4 (raw 55), actual states: 2 (raw 31)
- Timing tolerance: 8000 cycles

## RESULT: FAIL — display sequence diverges at state #1 (content differs)

- Affected pattern: during "cmd 0 allOFF" (sent at 100.0 ms)
- Baseline state #1: cycle 1965753 (122.860 ms, raw seq 18)
- Actual state #1: cycle 1979460 (123.716 ms, raw seq 30)

### State at divergence (baseline | actual)
```
    BASELINE                ACTUAL
    ........                ........
    .#....#.                ........   <-- differs
    ..#..#..                ........   <-- differs
    ...##...                ........   <-- differs
    ...##...                ........   <-- differs
    ..#..#..                ........   <-- differs
    .#....#.                ........   <-- differs
    ........                ........
    intensity 15/15         intensity 15/15
```

### Preceding 3 states (identical in both)
```
  #0  baseline cycle 214208 (13.388 ms, raw seq 6)  |  actual cycle 214772 (13.423 ms, raw seq 6)
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
  #2  baseline cycle 50346676 (3146.667 ms, raw seq 42)  |  actual absent
    .....#..                        
    ......#.                        
    ########                        
    ........                        
    ###..###                        
    ..#..#..                        
    ..#..#..                        
    .#....#.                        
    intensity 15/15         (absent)
  #3  baseline cycle 98468729 (6154.296 ms, raw seq 54)  |  actual absent
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

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 1
- max |drift|: 564 cycles (0.035 ms)
- final drift: +564 cycles

| marker | at ms | drift at next state |
|---|---|---|
| cmd 33 Symbol AI logo | 100.0 | n/a |
| cmd 20 Cross | 100.0 | n/a |
| cmd 0 allOFF | 100.0 | n/a |

## Canonical hash
- baseline: `dade3ab4abb4b4401ec995ca9470f0d6981214f1f9db6c662987533b5e757ec8`
- actual:   `087cb54423a6f88ea633c00c090d4e83e28fbce02bbf299f35d665f11acaa471`
- MISMATCH

