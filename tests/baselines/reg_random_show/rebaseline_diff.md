# Diff report: reg_random_show

**FAIL**

- Baseline firmware flash/elf sha256: `a160129b16f27cf186477b1f56fbba69e64dce4e0e4d8aca8492c6c08cbaeff5`
- Actual firmware flash/elf sha256: `8f59b259465268d73c9eb827777f27127c4d9b54cfbd4bb8e68c165e22344b9e`
- Mode: **settled** (states visible ≥ 10 ms; intermediate latches while a frame is clocked out are ignored)
- Baseline states: 42 (raw 71), actual states: 30 (raw 271)
- Timing tolerance: 8000 cycles

## RESULT: FAIL — display sequence diverges at state #1 (content differs)

- Affected pattern: during "reg 0x20 START [56]" (sent at 100.0 ms)
- Baseline state #1: cycle 1773793 (110.862 ms, raw seq 7)
- Actual state #1: cycle 1872756 (117.047 ms, raw seq 14)

### State at divergence (baseline | actual)
```
    BASELINE                ACTUAL
    ........                .......#   <-- differs
    .#......                .......#   <-- differs
    ........                .......#   <-- differs
    ........                .......#   <-- differs
    ........                .......#   <-- differs
    ........                .......#   <-- differs
    ........                .......#   <-- differs
    ........                .......#   <-- differs
    intensity 15/15         intensity 15/15
```

### Preceding 3 states (identical in both)
```
  #0  baseline cycle 215123 (13.445 ms, raw seq 6)  |  actual cycle 215215 (13.451 ms, raw seq 6)
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
  #2  baseline cycle 3495879 (218.492 ms, raw seq 8)  |  actual cycle 4235240 (264.702 ms, raw seq 22)
    ........                ......#.   <-- differs
    ..#.....                ......#.   <-- differs
    ........                ......#.   <-- differs
    ........                ......#.   <-- differs
    ........                ......#.   <-- differs
    ........                ......#.   <-- differs
    ........                ......#.   <-- differs
    ........                ......#.   <-- differs
    intensity 15/15         intensity 15/15
  #3  baseline cycle 5217758 (326.110 ms, raw seq 9)  |  actual cycle 6597800 (412.363 ms, raw seq 30)
    ........                .....#..   <-- differs
    ...#....                .....#..   <-- differs
    ........                .....#..   <-- differs
    ........                .....#..   <-- differs
    ........                .....#..   <-- differs
    ........                .....#..   <-- differs
    ........                .....#..   <-- differs
    ........                .....#..   <-- differs
    intensity 15/15         intensity 15/15
  #4  baseline cycle 6947179 (434.199 ms, raw seq 11)  |  actual cycle 8960202 (560.013 ms, raw seq 38)
    ........                ....#...   <-- differs
    ....#...                ....#...
    ........                ....#...   <-- differs
    ........                ....#...   <-- differs
    ........                ....#...   <-- differs
    ........                ....#...   <-- differs
    ........                ....#...   <-- differs
    ........                ....#...   <-- differs
    intensity 15/15         intensity 15/15
```

## Timing drift summary (actual − baseline, over the matched prefix)

- states compared: 1
- max |drift|: 92 cycles (0.006 ms)
- final drift: +92 cycles

| marker | at ms | drift at next state |
|---|---|---|
| reg 0x20 START [56] | 100.0 | n/a |

## Canonical hash
- baseline: `3b8913f64f33eaf67d57a6c7f2f3ce314eaee1b0a8f8d30ac286aa166ae34a68`
- actual:   `c9f9354bc568b4627300dc9381d2e26fc7bbc3112e2a5fe5281e2c37db7a103e`
- MISMATCH

