"""tools/check_version.py: release tags must match the firmware version (RELEASING.md)."""
from __future__ import annotations
import sys

import pytest
from conftest import REPO

sys.path.insert(0, str(REPO / "tools"))
import check_version  # noqa: E402


def sketch(tmp_path, major=0, minor=11, patch=0):
    p = tmp_path / "MagicPanel.ino"
    p.write_text(f"#define FW_MAJOR    {major}\n#define FW_MINOR    {minor}\n#define FW_PATCH    {patch}\n")
    return p


@pytest.mark.parametrize("tag,docs,pre", [("v0.11.0", "0.11", "false"), ("v0.11.0-rc1", "0.11-rc", "true")])
def test_matching_tags(tmp_path, tag, docs, pre):
    assert check_version.check(tag, sketch(tmp_path)) == {"version": "0.11.0", "docs_version": docs, "prerelease": pre}


@pytest.mark.parametrize("tag", ["v0.11.1", "v0.12.0", "0.11.0", "v0.11", "v0.11.0+build"])
def test_rejected_tags(tmp_path, tag):
    with pytest.raises(SystemExit):
        check_version.check(tag, sketch(tmp_path))


def test_dev_sketch_has_a_version():
    assert check_version.firmware_version() == (0, 11, 0)
