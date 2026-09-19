# Mutants (harness self-test)

`make_mutants.py` defines deliberately broken variants of the dev sketch `MagicPanel.ino`, one
minimal change each. `tests/test_mutants.py` generates them from the current sketch at test time
and proves the comparator detects each one and that `diff_report.md` names the affected
pattern. **They are never the firmware.** To inspect them: `python3 tests/mutants/make_mutants.py /tmp/mutants`.

- `cross_pixel`: one pixel changed in the Cross pattern (must fail `cmd_20_cross`)
- `toggle_faster`: Toggle half-period 20 ms faster (must fail `cmd_05_toggle` on timing only)
- `intensity_14`: device 0 one brightness step lower at boot (must fail `power_on_default`, `cmd_20_cross`)
- `swap_symbol_cross`: commands 20 and 33 swapped (must fail `cmd_20_cross`, `cmd_33_symbol`)
