# Releasing

How a firmware release is cut and how its documentation is published. The pipeline is in
`.github/workflows/release.yml` and `docs.yml`; the reasoning is in `docs/decisions.md` (D-23).

## Versions

| Thing | Example | Where it comes from |
|---|---|---|
| Firmware version | `0.11.0` | `FW_MAJOR/FW_MINOR/FW_PATCH` in `MagicPanel.ino`, readable over I2C |
| Release tag | `v0.11.0` | must equal the firmware version (checked by `tools/check_version.py`) |
| Docs version | `0.11` | `MAJOR.MINOR`: a patch release updates its minor's docs in place |
| Pre-release | `v0.12.0-rc1` → docs `0.12-rc` | published, but `latest` does not move |

The site has one directory per docs version on the `gh-pages` branch (managed by mike), plus:

- `latest`: the newest release; the site root redirects here;
- `dev`: built from `main` on every push, marked as unreleased.

## Cutting a release

1. On a branch, bump `FW_MAJOR/FW_MINOR/FW_PATCH` in `MagicPanel.ino` and add a line to its
   release history comment. Change `docs/i2c-protocol.md` and `docs/magicpanel_i2c.h` only if the
   protocol changed (then bump `PROTO_MINOR` or `PROTO_MAJOR` too).
2. In `CHANGELOG.md`, move the `Unreleased` entries under a new `## [X.Y.Z]` heading.
3. If any sequence's timing changed, run `.venv/bin/python tools/measure_lengths.py --write`.
4. Run `make test` and `make docs-gen docs-build`, then open a PR and merge it.
5. Tag the merge commit and push the tag:

   ```sh
   git tag vX.Y.Z && git push origin vX.Y.Z
   ```

The release workflow then:

1. checks that the tag matches the firmware version;
2. builds the firmware and runs the full test suite;
3. generates and publishes the docs as `X.Y`, and moves `latest` to it (not for pre-releases);
4. creates the GitHub Release with the `CHANGELOG.md` section as notes and these assets:
   `MagicPanel-vX.Y.Z.hex`, `.elf`, `SHA256SUMS`, `magicpanel_i2c.h` and an offline copy of the
   docs (`make docs-offline`: plain `.html` links that work from disk; the pattern players fall
   back to the GIFs there).

## Trying the pipeline

Push a pre-release tag such as `v0.11.0-rc1`. It publishes docs `0.11-rc` and a pre-release
without touching `latest`. To remove it afterwards:

```sh
.venv/bin/mike delete --push 0.11-rc
gh release delete v0.11.0-rc1 --cleanup-tag
```

## Previewing locally

```sh
make docs-setup                 # once
make docs-gen docs-serve        # live preview of the current tree
.venv/bin/mike deploy --alias-type redirect --update-aliases 0.11 latest && .venv/bin/mike deploy dev   # local gh-pages only (no --push)
make docs-preview-versions      # the version switcher over the local gh-pages branch
```

Delete the local `gh-pages` branch afterwards if you don't want it (`git branch -D gh-pages`).

## GitHub Pages setup (once)

After the first deploy has created the `gh-pages` branch, set *Settings → Pages → Source* to
**Deploy from a branch**, branch `gh-pages`, folder `/ (root)`.

## Moving to a custom domain

1. In *Settings → Pages → Custom domain*, enter the domain (for example
   `magicpanel.example.org`). GitHub commits a `CNAME` file to the root of `gh-pages`; mike leaves
   root files alone, so it survives later deploys.
2. At your DNS provider, add a `CNAME` record from that name to `daveismith.github.io`. For an
   apex domain, use GitHub's `A`/`AAAA` records instead.
3. Once the certificate is issued, tick *Enforce HTTPS*.
4. Set `site_url` in `mkdocs.yml` to the new address, then merge. The next `dev` deploy and every
   release after it use it.

Version paths (`/0.11/…`) stay the same, and the old `github.io` addresses redirect
automatically.
