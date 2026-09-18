"""The frozen reference must never change: sketch and ELF hashes are pinned, and the settled-mode
comparison must treat the dev sketch's build and the specimen identically when they are the same."""
from __future__ import annotations
from conftest import mplib


def test_specimen_sketch_hash_is_frozen():
    assert mplib.sha256_file(mplib.SPECIMEN_SKETCH) == mplib.SPECIMEN_SKETCH_SHA256, \
        "MagicPanel_v010_5.ino changed; it is the immutable reference. Edit MagicPanel.ino instead."


def test_specimen_elf_hash_is_frozen():
    assert mplib.sha256_file(mplib.SPECIMEN_ELF) == mplib.SPECIMEN_ELF_SHA256


def test_reference_build_reproduces_specimen_flash(reference):
    import json
    elf, meta = reference
    assert json.loads(meta.read_text())["elf"]["flash_sha256"] == mplib.SPECIMEN_FLASH_SHA256


def test_settled_filter_drops_only_fast_intermediate_states():
    recs = mplib.read_jsonl(mplib.BASELINE_DIR / "cmd_20_cross" / "display.jsonl")
    settled = mplib.settled_display(recs)
    assert 0 < len(settled) < len(recs)
    cross = ["00000000", "01000010", "00100100", "00011000", "00011000", "00100100", "01000010", "00000000"]
    assert cross in [r["grid"] for r in settled], "the fully clocked-out Cross frame must survive"
    # no settled state is followed within settle_ms by another (except the very last raw state)
    for i in mplib.settled_indices(recs)[:-1]:
        assert recs[i + 1]["cycle"] - recs[i]["cycle"] >= mplib.DEFAULT_SETTLE_MS * mplib.CYCLES_PER_MS
