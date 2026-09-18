# Magic Panel firmware + simulation regression harness

- `MagicPanel_v010_5.ino` — the firmware (IA-PARTS Magic Panel FX v010.5). Not modified by anything here.
- `MagicPanel_v010_5.ino.elf` — the precompiled reference build; `make firmware` reproduces its flash image exactly.
- **[harness/README.md](harness/README.md)** — how to set up, run, read diff reports, add scenarios, re-baseline.
- [docs/firmware-map.md](docs/firmware-map.md) — what the firmware does (pins, MAX7221 chain, I2C protocol, patterns, risks).
- [docs/decisions.md](docs/decisions.md) — assumptions and non-obvious choices.

```sh
make setup && make test
```
