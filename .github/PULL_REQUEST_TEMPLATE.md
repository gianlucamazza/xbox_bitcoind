<!-- Keep changes focused; prefer small PRs. See CONTRIBUTING.md. -->

## What

<!-- One or two sentences: what changes and why. -->

## Checklist

- [ ] `CHANGELOG.md` **Unreleased** updated (user-facing behaviour or packaging changed)
- [ ] `docs/tracking.md` updated (an ops gate moved)
- [ ] No `third_party/bitcoin/`, certs, or secrets committed
- [ ] Host-side checks pass: `shellcheck -x -S style scripts/*.sh`,
      `./scripts/test-ui-layout.sh`, `./scripts/test-rpc-client.sh`,
      `./scripts/check-conf-sync.sh`
- [ ] Touching `patches/uwp/**` or the Core pin: aware this rebuilds Core in CI
      (patch-check must be green)

## Console verification (if applicable)

<!-- Package revision tested, node-status/soft-stop results. -->
