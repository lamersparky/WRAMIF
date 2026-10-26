## 0.3.0 — Native Windows GUI/CLI edition and WRAMIF project rename

- Renamed the project, Linux executable, C source, completions, persistent-state directory, download package, and generated script naming to WRAMIF / `wramif`.
- Added a native Win32 Windows `WRAMIF.exe` source with a Matrix-inspired GUI by default and `--cli` mode for terminal workflows.
- Windows edition scans audio files, supports recursive or top-level scanning, streamed BCrypt SHA-256 duplicate detection, selectable output format, and generation of a `.bat` launcher plus PowerShell worker.
- Added a Windows CI build workflow using the Microsoft C toolchain, producing a `WRAMIF-Windows-x64-0.3.0.zip` artifact.
- Retained Linux as a CLI-only build; its SQLite-backed inventory, resume, metadata prompts, GPG script signing, and Bash/Zsh completions remain available.
- Documented that the first native Windows edition is a functional companion but does not yet have full parity with the Linux SQLite/resume/metadata/GPG features.

## 0.2.0 — Windows conversion-package generation

- Added documented `--windows-batch` / `--batch-script` Windows package workflow: `.bat` launcher, PowerShell conversion worker, bundled assets, detached signatures, and asset SHA-256 checks.
- Added automated regression coverage for Windows package generation, lyric/image bundling, SQLite records, and detached signature verification.
- Added Windows options to Bash and Zsh completion definitions.
- Clarified that Linux can generate Windows conversion packages and that the recipient can run the package without a Linux VM.

## 0.1.0 — Initial public-testing package

- Native C command-line program with SQLite-backed run state and incremental audio inventory.
- Streaming SHA-256 duplicate detection with a basename-only default and full-content mode via `--no-duplicates`.
- FFmpeg one-liner generation and standalone POSIX shell-script generation for larger plans.
- Optional title, artist, album, lyrics, and MP3 cover-art prompts.
- Per-track optional lyrics behavior for TXT/LRC/SRT inputs, with SRT timing stripped before embedding as ordinary lyrics text.
- Local OpenPGP signing and SHA-256 records for generated scripts.
- First-run INI configuration, resume support, and `--flush`.
- Built-in Tab path completion and Bash/Zsh option-completion files.
- Escape-key navigation to revisit previous metadata entries, including entries from the preceding track.
- Regression tests for CLI/database setup, metadata persistence and migration, resumability, and Escape navigation.
