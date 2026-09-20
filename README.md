# Magic Panel firmware + simulation regression harness

📖 **User documentation: <https://davidiansmith.ca/magicpanel/>** — flashing, wiring, the pattern
gallery and the I2C interface, one version per release
([dev](https://davidiansmith.ca/magicpanel/dev/) tracks `main`).

- `MagicPanel.ino` — the sketch under development. Edit this one.
- `MagicPanel_v010_5.ino` — the frozen reference (IA-PARTS Magic Panel FX v010.5); hash-guarded, never modified.
- **[CLAUDE.md](CLAUDE.md)** — rules and workflow for agents working on the sketch.
- `MagicPanel_v010_5.ino.elf` — the precompiled reference build; `make firmware` reproduces its flash image exactly.
- **[harness/README.md](harness/README.md)** — how to set up, run, read diff reports, add scenarios, re-baseline.
- [docs/firmware-map.md](docs/firmware-map.md) — what the firmware does (pins, MAX7221 chain, I2C protocol, patterns, risks).
- [docs/decisions.md](docs/decisions.md) — assumptions and non-obvious choices.
- [docs/i2c-protocol.md](docs/i2c-protocol.md) — the I2C register interface for controller authors.
- **User documentation site** (the link above): `manual/` + `mkdocs.yml`, built with Zensical and
  published per release with mike (`make docs-setup docs-gen docs-serve`). Releases:
  [RELEASING.md](RELEASING.md).

```sh
make setup && make test
```
