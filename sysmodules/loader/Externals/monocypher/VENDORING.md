# Vendored Monocypher

Unmodified copy of [Monocypher](https://monocypher.org/) **4.0.3** (2026-06-15),
dual-licensed CC0-1.0 / BSD-2-Clause (see `LICENCE.md`).

## Source

`monocypher-4.0.3.tar.gz`, byte-identical from both
<https://monocypher.org/download/monocypher-4.0.3.tar.gz> and the GitHub release
<https://github.com/LoupVaillant/Monocypher/releases/tag/4.0.3>.

Tarball SHA-512 (matches the published `monocypher-4.0.3.tar.gz.sha512`):

```
40904ada5c7ee4f7741733e38b69a30a4b0561cbffba5ffe7c2dce16136d540251ec0d9056ff606510d3b5b708fb8a40db7e0870d4a0b2dc17ba2bfb880f8965
```

## Files

Copied flat into this directory (no `optional/` subdirectory):

| File | Path in tarball | SHA-256 |
| :--- | :--- | :--- |
| `monocypher.c` | `src/` | `57eb914fc88136119bd41655cccb8c250048bf54d470540625186f8ab16f64be` |
| `monocypher.h` | `src/` | `c494da712122da7ff679fdcf318a5317e84972b6c950fe9d896212947797facd` |
| `monocypher-ed25519.c` | `src/optional/` | `60fce3578fb00b00da96490653d993c4cb427b1e1be38183285c66e04d22cc18` |
| `monocypher-ed25519.h` | `src/optional/` | `abc4fad381879f5c29176ebe014b9189956b3dfe0a3e36459b6990bc57212380` |
| `LICENCE.md` | `./` | `5f8360e4c06ddcc584bdb4b210c6af824c4bb301e6a9a521869b6d90795ca4b3` |

## Rules

- **Do not edit these files.** Unused functions are already dropped by the
  linker (`--gc-sections` in `3dsx.specs`).
- Monocypher's default `crypto_eddsa_*` is EdDSA over BLAKE2b, **not** Ed25519.
  Use `crypto_ed25519_*` from `monocypher-ed25519.h`.
- Both `.c` files are compiled with `-O2` regardless of `BUILD_FOR_GDB`
  (`sysmodules/loader/Makefile`).

## Updating

1. Download the tarball from both sources, check they are identical and that
   the SHA-512 matches the published checksum.
2. Replace the five files, update the hashes above, and rebuild.
