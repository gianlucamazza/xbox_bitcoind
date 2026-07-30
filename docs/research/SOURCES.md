# Research sources

Collected during initial online research (2026-07-30). Not exhaustive.

## Xbox / Dev Mode / UWP

- https://docs.microsoft.com/en-us/windows/uwp/xbox-apps/
- https://docs.microsoft.com/en-us/windows/uwp/xbox-apps/devkit-activation
- https://xboxoneresearch.github.io/wiki/development/setup-dev-mode/
- https://ku.nz/blog/xbox.html
- https://www.howtogeek.com/703443/how-to-put-your-xbox-series-x-or-s-into-developer-mode/
- https://www.techrepublic.com/article/xbox-series-s-and-x-developer-mode-3-things-you-can-do-with-it-and-3-you-cant/
- https://linustechtips.com/topic/1401417-microsoft-cracks-down-on-xbox-dev-mode-homebrew/
- https://xboxdevstore.github.io/ (homebrew catalog context)

## Microsoft Store policy (crypto)

- https://learn.microsoft.com/en-us/windows/apps/publish/store-policies  
  - §10.2.6 mining ban; wallet/view rules; company account for financial crypto features

## Bitcoin Core build / node ops

- https://github.com/bitcoin/bitcoin
- https://github.com/bitcoin/bitcoin/blob/master/doc/build-windows-msvc.md
- https://github.com/bitcoin/bitcoin/blob/master/doc/build-windows.md
- https://bitcoincore.org/ / https://bitcoin.org/en/full-node
- CMake migration notes (bitcoindev / community posts, 2024+)

## Historical / non-applicable prior art

- https://bitcointalk.org/index.php?topic=9489.0 (Xbox 360 mining discussion)
- https://github.com/Generalkidd/XNAMiner (XNA miner)

## Sibling projects (this machine)

- `/home/gianluca/Workspace/tooling/xllama` — LLM UWP on the **same Series S**; SSOT for measured UWP constraints and Device Portal scripts pattern
- `/home/gianluca/Workspace/tooling/xbox_lightning` — reserved for a later Lightning layer; out of scope for v1

## Local console baseline

- `docs/console.md` — live probe (OS build, IP via env, xllama package presence)
- `~/.config/xllama/xbox-env` — Device Portal credentials (not in git)
