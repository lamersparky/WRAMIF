# Contributing to WRAMIF

Thanks for helping test and improve `wramif` (WRAMIF). The project is in early testing, so reproducible bug reports and small focused patches are especially useful.

## Before opening a bug report

1. Include the WRAMIF version (`./wramif --help` currently identifies supported options; the source version is `0.3.0`).
2. Describe the command you ran, expected behavior, and actual behavior.
3. Include relevant terminal output and the Linux distribution, compiler, and library versions.
4. Run `make clean && make && make test` and report which step fails.
5. Use a small synthetic directory where possible. Do not attach private music, lyrics, cover art, personal paths, or the WRAMIF database unless you have reviewed and intentionally sanitized them.

## Development checks

```sh
make clean
make
make test
```

The build uses C11 and enables GCC/Clang warnings. Keep the inventory and duplicate-hashing paths bounded-memory, prefer incremental SQLite operations, and release temporary allocations promptly. Tests must not modify or delete a user's music or automatically execute generated conversion commands.

## Pull requests

- Keep changes focused and explain the problem they solve.
- Add or update regression tests for bug fixes and behavior changes.
- Update `README.md` and `CHANGELOG.md` when user-facing behavior changes.
- Be explicit about untested platforms or edge cases; do not describe a test as passing unless it was run.

## Safety expectations

WRAMIF should preserve source files, avoid silently overwriting outputs, and leave FFmpeg execution under the user's control. Generated scripts and a valid signature must never be described as proof that a script is safe.


### Windows package changes

Changes to `--windows-batch` should include coverage in `tests/windows-batch.sh`. That test verifies package structure, bundled lyric/cover assets, SQLite records, and GPG signatures without executing FFmpeg. The actual `.bat`/PowerShell workflow should also be tested on a Windows machine before claiming native Windows runtime validation.
