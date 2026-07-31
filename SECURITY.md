# Security policy

## Supported versions

| Version | Supported |
|---------|-----------|
| Latest GitHub Release ([Releases](https://github.com/gianlucamazza/xbox_bitcoind/releases)) | Yes |
| `main` branch (pre-release) | Best-effort |
| Older tags | No |

This project runs only on **Xbox Developer Mode** (sideloaded UWP). It is **not** a
production banking wallet, Microsoft Store app, or hardened multi-tenant service.

## Scope

In scope (project glue):

- UWP host app (`uwp/`), deploy/ops scripts (`scripts/`), packaging, CI
- Misconfiguration that exposes local RPC or credentials **via this repo’s defaults**

Out of scope (report upstream or treat as platform risk):

- Vulnerabilities in [Bitcoin Core](https://github.com/bitcoin/bitcoin/security) itself
- Xbox OS / Dev Mode / Device Portal platform issues
- Physical access to an unlocked Dev Mode console
- Running with `listen=1`, public RPC, or non-default configs you choose

## Reporting a vulnerability

**Please do not open a public GitHub issue for security-sensitive reports.**

Prefer, in order:

1. **[GitHub Security Advisories](https://github.com/gianlucamazza/xbox_bitcoind/security/advisories/new)** (private) for this repository  
2. Or contact the repository owner via the email listed on their GitHub profile  

Include:

- Affected version / commit / package revision if known  
- Reproduction steps on Dev Mode (or host scripts)  
- Impact assessment (e.g. RPC exposure, path traversal in deploy helpers)  

You should receive an acknowledgement when practical. There is no formal bug bounty.

## Hardening notes for operators

- Keep Device Portal credentials out of the repo; use `xbox-env` outside git  
- Prefer `listen=0` and loopback RPC (project defaults)  
- Use soft stop (`deploy.sh stop-app`); avoid shipping production keys on Dev Mode  
- Treat the console as a trusted LAN device  

See also [docs/ops.md](docs/ops.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
