# Mutant sketches (harness self-test)

Deliberately broken copies of `MagicPanel_v010_5.ino`, one minimal change each, used by `tests/test_mutants.py` to prove the comparator detects regressions and that `diff_report.md` names the affected pattern. **They are never the firmware.** Regenerate with `python3 tests/mutants/make_mutants.py`.

- `cross_pixel`: One pixel changed in the Cross pattern: row 3 B00011000 -> B00111000 (expected to fail: cmd_20_cross)
- `toggle_faster`: Toggle half-period 20 ms faster: delay(500) -> delay(480) (both halves) (expected to fail: cmd_05_toggle)
- `intensity_14`: Device 0 intensity one step lower at boot: lc.setIntensity(0,15) -> 14 (expected to fail: power_on_default, cmd_20_cross)
- `swap_symbol_cross`: Commands 20 and 33 swapped: cmd 20 now shows the AI Symbol and cmd 33 the Cross (expected to fail: cmd_20_cross, cmd_33_symbol)
