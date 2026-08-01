# Upgrades

Two distinct upgrade paths: the **Bitcoin Core pin** (source tree the app embeds)
and the **app package on the console** (MSIX revision). They move independently.

## Bitcoin Core pin bump (single source of truth)

This section replaces the partial lists that used to live in
`config/bitcoin-core.pin` and [ci.md](ci.md) — follow it end to end.

1. Edit `config/bitcoin-core.pin`: new `TAG` + **peeled** `COMMIT`
   (`git rev-parse vX.Y.Z^{}` on the Core repo).
2. `python3 scripts/generate-version-header.py` — regenerates
   `uwp/xbb_version.generated.h` (ci-linux fails if it drifts from the pin).
3. Re-fetch the tree: `./scripts/fetch-bitcoin-core.sh` (or `.ps1`).
   The fetch **resets and cleans** a previously patched tree (build dirs are kept),
   so the stale patch marker disappears with it.
4. Re-apply and re-verify the patch set: `./scripts/apply-uwp-patches.sh`.
   This is the real risk of a bump — `patches/uwp/0001…0010` carry blob hashes from
   the previous tag; expect context conflicts in high-churn files
   (`src/CMakeLists.txt` from 0008, `src/common/args.cpp` from 0005). Fix patches,
   regenerate with `git diff` from the patched tree, and update
   `patches/uwp/README.md` (header names the pin).
5. Linux sanity: `CI_SKIP_TESTS=1 ./scripts/build-linux-smoke.sh`.
6. Windows: re-run the MSVC baseline (`.\scripts\build-msvc-baseline.ps1`) and the
   UWP core build (`.\scripts\build-uwp.ps1 -CoreOnly -Force`); record notable
   findings under `docs/research/spikes/`.
7. Commit pin + regenerated header + any patch updates. Push → CI rebuilds with new
   cache keys; the `patch-check` job in `build-uwp.yml` validates the patch set on
   the PR itself.
8. Update docs that quote the Core version if the major changed (README badge line,
   `third_party/README.md`).

## App package upgrade on console (MSIX)

Order matters: the datadir survives an in-place **update**, but an **uninstall
wipes `LocalState`** (chainstate + blocks → full re-IBD).

1. Download the release assets and verify them:
   `gh release download vX.Y.Z && sha256sum -c SHA256SUMS`
   (optionally `gh attestation verify <msix> -R gianlucamazza/xbox_bitcoind`).
2. Soft-stop the node: `./scripts/deploy.sh stop-app` (wait for clean exit — see
   [persistence.md](persistence.md)).
3. Install the new package **without uninstalling**:
   `./scripts/deploy.sh path/to/xbox_bitcoind_*.msix`
   (keep `Dependencies/x64/` next to the `.msix` for VCLibs on first install).
4. Dev Home → package → **App type → Game** (re-check after every reinstall;
   an update usually preserves it, a remove+add does not).
5. Start and verify tip conservation:
   `./scripts/deploy.sh start-app && ./scripts/node-status.sh` — height must resume
   at or above the pre-stop tip (no `nBestHeight=0` in the log tail).
6. Never uninstall to "downgrade" a revision — revisions only move forward
   (releases use `10000 + run_number` precisely so they sort above dev builds).

Verification for either path: `./scripts/health-check.sh` (exit 0), then
`./scripts/soft-stop-test.sh` for the suspend/resume cycle.
