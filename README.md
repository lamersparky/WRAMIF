# WRAMIF — Windows Recode All Music In Folder

**A Matrix-inspired C audio-conversion planner: Linux CLI plus a native Windows GUI/CLI companion.**

WRAMIF brings a little Matrix-terminal atmosphere to a practical job: scan a music directory, preserve the inventory and progress in SQLite, decide which files are worth processing, and generate a command or script for the conversion work.

> **Early testing release — please test against copies of your music first.** WRAMIF builds a plan; it does not run FFmpeg automatically or delete source files.

```text
FILE TREE  ->  SCAN  ->  SHA-256 DEDUPLICATION  ->  METADATA
      ->  FFmpeg ONE-LINER or SIGNED SCRIPT  ->  YOU REVIEW AND RUN IT
```

## Contents

- [Features](#features)
- [Requirements](#requirements)
- [Build and test](#build-and-test)
- [Install](#install)
- [Quick start](#quick-start)
- [Command-line options](#command-line-options)
- [Metadata prompts and keyboard controls](#metadata-prompts-and-keyboard-controls)
- [Duplicate handling](#duplicate-handling)
- [Database, resume, and configuration](#database-resume-and-configuration)
- [Generated scripts and signatures](#generated-scripts-and-signatures)
- [Shell compatibility and completion](#shell-compatibility-and-completion)
- [Known limitations](#known-limitations)
- [Security notes](#security-notes)
- [Credits](#credits)
- [License](#license)

## Features

- **Native C implementation** using SQLite and OpenSSL, with no Python runtime required by the binary.
- **Bounded-memory inventory and hashing:** files are discovered and recorded incrementally; SHA-256 uses a fixed-size streaming buffer rather than loading entire audio files into RAM.
- **SQLite-backed run state:** inventory, hashes, duplicate relationships, per-track metadata, generated plans, and metadata-prompt history are saved locally.
- **Duplicate detection:** default mode hashes files with colliding basenames; `--no-duplicates` streams SHA-256 over every discovered audio file.
- **FFmpeg command generation:** small POSIX plans are printed as a one-liner; larger plans become standalone POSIX `sh` scripts.
- **Linux-generated Windows conversion-package generation:** `--windows-batch` creates a `.bat` launcher, companion PowerShell worker, bundled lyric/cover assets, detached signatures, and recorded SHA-256 digests. The recipient can run the conversion package on Windows without a Linux VM.
- **Optional per-track tags:** title, artist, album, lyrics, and cover art.
- **Lyrics support:** `.txt`, `.lrc`, and `.srt`. SRT timestamps/cue numbers are stripped before embedding as ordinary, unsynchronized lyrics text.
- **Local OpenPGP script signing (Linux CLI):** generated POSIX scripts and Linux-generated Windows packages receive detached signatures and recorded SHA-256 digests.
- **Resume support:** saved run state remains available after interruption or exit.
- **Interactive path completion:** press **Tab** in WRAMIF's path prompts. Command-line completions are supplied for Bash and Zsh.
- **Escape navigation:** press **Esc** to revisit a previous metadata entry, including the last entry of the previous song; after editing it, WRAMIF returns to the prompt you were answering.
- **Matrix-inspired terminal styling** when the terminal supports color.

## Requirements

On Ubuntu or Debian:

```sh
sudo apt update
sudo apt install build-essential pkg-config libsqlite3-dev libssl-dev ffmpeg gnupg python3
```

`python3` is used by the regression tests, not by the compiled program itself. `ffmpeg` and `gpg` must be discoverable in `PATH`. Clipboard support is optional and uses whichever of `wl-copy`, `xclip`, or `xsel` is available. If none is installed, WRAMIF can still generate the command or script; use `--no-clipboard` to explicitly disable clipboard copying.

## Build and test

Clone the repository, enter the project directory, then run:

```sh
make
make test
./wramif --help
```

`make test` runs safe tests with temporary directories and synthetic/empty audio placeholders. Tests cover the CLI and database setup, title/lyrics persistence and migration, resume behavior, and Escape navigation. These tests do **not** validate the sound quality of actual encodings; test real audio files before relying on a large conversion run.

## Install

System-wide installation:

```sh
sudo make install
```

Install under your home directory instead:

```sh
make install PREFIX="$HOME/.local"
```

The binary is installed under `bin/`, the Bash completion file under `share/bash-completion/completions/`, and the Zsh completion file under `share/zsh/site-functions/`.

For a user-prefix Zsh installation, add this to `~/.zshrc` and start a new shell (or run the lines once):

```zsh
fpath=("$HOME/.local/share/zsh/site-functions" $fpath)
autoload -Uz compinit && compinit
```

The program itself works when launched from either Bash or Zsh. Generated conversion scripts use POSIX `/bin/sh` syntax.

## Quick start

Plan a recursive MP3 conversion. This example explicitly disables clipboard copying while you test:

```sh
./wramif \
  --target "$HOME/Music" \
  --output "$HOME/Music-recoded" \
  --no-clipboard
```

Hash every audio file, including files with different names, and include only one copy of identical content in the generated plan:

```sh
./wramif \
  --target "$HOME/Music" \
  --output "$HOME/Music-recoded" \
  --no-duplicates --no-clipboard
```

Collect metadata for each eligible track:

```sh
./wramif \
  --target "$HOME/Music" \
  --output "$HOME/Music-recoded" \
  --title --artist --album --lyrics --image --no-clipboard
```

Generate a Windows conversion package instead of a POSIX shell script/command. The output files can be copied to a Windows computer; see [Windows batch package](#windows-batch-package):

```sh
./wramif \
  --target "$HOME/Music" \
  --output "$HOME/Music-recoded" \
  --windows-batch --title --artist --album --lyrics --image --no-clipboard
```

Choose a different output format:

```sh
./wramif \
  --target "$HOME/Music" \
  --output "$HOME/Music-flac" \
  --format flac --no-clipboard
```

Let WRAMIF ask for paths interactively (Tab completes path components):

```sh
./wramif --manual-input --title --artist --album
```

Resume the latest unfinished run:

```sh
./wramif --resume --no-clipboard
```

Verify a generated script against the adjacent signature:

```sh
./wramif \
  --verify-script "$HOME/.local/share/wramif/scripts/WRAMIF-....sh"
```

Clear WRAMIF's database and start with a fresh inventory:

```sh
./wramif --flush
```

`--flush` removes the database and its SQLite sidecar files, then recreates the schema. It preserves the INI configuration and the local GPG key directory.

## Command-line options

| Option | Description |
|---|---|
| `--target PATH` | Source directory containing audio files. |
| `--output PATH` | Destination directory for converted audio. The relative directory structure is preserved. |
| `--recursive` | Scan subdirectories; this is the default. |
| `--no-recursive` | Scan only the top-level directory. |
| `--format FORMAT` | Output format: `mp3` (default), `flac`, `opus`, `aac`, or `wav`. |
| `--no-duplicates` | SHA-256 every discovered audio file; plan identical content once. Despite the option's name, this enables full-content duplicate checking. |
| `--no-clipboard` | Do not copy the generated command/launcher to the clipboard. |
| `--windows-batch` | Generate a Windows `.bat` launcher, PowerShell worker, bundled assets, and signatures. Alias: `--batch-script`. |
| `--manual-input` | Ask for the target and output paths interactively. |
| `--resume` | Resume the latest unfinished run saved in SQLite. |
| `--reenter-metadata` | Prompt again for metadata already saved in a resumed run. |
| `--title` | Prompt per track for song title; filename without extension is suggested. |
| `--artist` | Prompt per track for artist; the most recently entered artist is prefilled. |
| `--album` | Prompt per track for album; the most recently entered album is prefilled. |
| `--lyrics` | Ask for optional `.txt`, `.lrc`, or `.srt` lyrics. Enter, `none`, or `skip` means no lyrics for that track. |
| `--image` | Ask for cover art per track. Cover embedding is currently supported for MP3 output only. |
| `--flush` | Clear database state and recreate the schema; preserve configuration and signing keys. |
| `--verify-script PATH` | Verify `PATH.asc` using the local WRAMIF signing key. |
| `--help` | Print usage. |

At least `--target` and `--output` are required unless `--manual-input` is used. `--resume` uses the paths and scan settings stored for the unfinished run.

## Metadata prompts and keyboard controls

### Title, artist, and album

- `--title` suggests the filename without its extension. Press Enter to accept it or type a replacement.
- `--artist` and `--album` prefill the last value entered, to reduce repetitive typing in a music library.
- Each accepted metadata entry is written to SQLite as you go.

### Lyrics and cover art

- `--lyrics` accepts `.txt`, `.lrc`, and `.srt` files. Lyrics are optional for **each individual song**: press Enter or type `none`/`skip` when there are no lyrics. WRAMIF records that choice and does not keep asking for the same track after resume.
- SRT cue numbers and timestamps are stripped from a small derived text file. The embedded lyrics are ordinary unsynchronized metadata; the original SRT timing is not preserved in the audio file.
- `--image` asks for a cover image. Image embedding currently applies only to MP3 output.

### Escape goes back

At an interactive prompt, press **Esc** to return to the most recent completed metadata entry. WRAMIF temporarily asks that earlier question again, saves your correction, and returns to the prompt you were answering. While editing an earlier entry, Esc can step back farther, including to a previous track. At the very first prompt of a session, there may be no prior entry to revisit.

For scripts and automated testing that pipe answers to WRAMIF, a line containing a single literal Escape byte is treated as the back command. In a normal terminal, the line editor distinguishes a bare Esc press from ANSI escape sequences used by terminal keys.

**Tab** completes paths in path prompts. Paths entered at the shell should be quoted normally, particularly when they contain spaces:

```sh
--target "$HOME/Music Collection" --output "$HOME/Recoded Music"
```

## Duplicate handling

- **Default behavior:** only groups of files sharing the same basename are SHA-256 hashed. This catches same-name duplicates while avoiding hashing every otherwise-unique filename.
- **`--no-duplicates`:** every discovered file is streamed through SHA-256, so identical audio can be recognized even when its filename differs.
- WRAMIF keeps one path-sorted representative from each duplicate group in the conversion plan.
- **Source files are never deleted.** Duplicate files are omitted from the generated plan; they remain on disk.

The inventory is stored as SQLite rows rather than a large in-memory array of file paths. SHA-256 uses a fixed-size read buffer. SQLite's cache is capped, temporary storage is directed to disk, and WRAMIF asks SQLite and the system allocator to release memory after large operations.

## Database, resume, and configuration

WRAMIF stores its state at:

```text
~/.local/share/wramif/
├── ramif_database.db
├── wramif.ini
├── public-key.asc
├── gnupg/
├── lyrics-cache/
└── scripts/
```

If `XDG_DATA_HOME` is set, WRAMIF uses `$XDG_DATA_HOME/wramif/` instead. When upgrading from the former `recode_all_music_in_folder` state directory, the Linux CLI moves it to the new location on first launch when the new directory does not already exist, preserving the database, scripts, and signing-key directory.

The database stores run configuration and status, discovered paths, file sizes and timestamps, hashes, duplicate relationships, metadata, lyric-processing state, output paths, generated commands/scripts, signature details, and metadata-prompt navigation history. `--resume` uses this state to continue an unfinished run. `--flush` clears the database but retains the INI and signing identity.

On first launch, WRAMIF creates its configuration and dedicated OpenPGP signing key, and exports the corresponding public key. The INI records the detected user, host/system details, binary and FFmpeg paths, database location, and key locations. Run identifiers combine a UTC timestamp with a SHA-256-derived component that includes system/user/host/binary details and random bytes.

## Native Windows EXE

The Windows build is a separate Win32 C application. It opens a Matrix-inspired graphical interface by default; launch `WRAMIF.exe --cli` to use command-line mode. Its CLI accepts `--target`, `--output`, `--format`, `--recursive` / `--no-recursive`, and `--no-duplicates`. The GUI supports source/output folder browsing, output format selection, recursive scanning, and full-file SHA-256 duplicate checking.

The Windows app generates a `.bat` launcher and a PowerShell worker under `%LOCALAPPDATA%\WRAMIF\scripts`. The generated package does not run FFmpeg until you launch the `.bat` yourself. Install a native Windows FFmpeg build and add `ffmpeg.exe` to `PATH`; no Linux VM is required. Windows CI builds `WRAMIF.exe` and publishes a `WRAMIF-Windows-x64-0.3.0.zip` artifact from GitHub Actions. The `.exe` is produced by the Windows runner; it is not bundled into this Linux-built source archive.

**Feature parity note:** this first native Windows build is a companion edition, not yet a complete port of the Linux CLI. The Linux edition retains SQLite-backed resume, per-track title/artist/album/lyrics/image prompts, GPG signing of generated scripts, and Bash/Zsh completions. The initial Windows edition uses a small local history TSV, keeps its path inventory in memory while scanning, and does not yet provide Linux-edition SQLite resume, metadata prompts, or GPG script signing. The Windows target has been wired into CI, but the application has not yet been runtime-tested on a physical Windows installation in this environment.

Example CLI invocation:

```powershell
.\WRAMIF.exe --cli --target "C:\Users\you\Music" --output "C:\Users\you\Music-Recoded" --format mp3 --no-duplicates
```

To open the GUI explicitly, run `WRAMIF.exe --gui` or double-click the executable.

## Windows batch package

The Linux CLI can also prepare a portable Windows conversion package using `--windows-batch` (or the alias `--batch-script`). This signed, asset-bundling package is distinct from the native `WRAMIF.exe` Windows application described above. Run the Linux CLI to scan/index the collection and generate the package; the recipient can execute that package on Windows without a Linux VM.

A generated package contains several files that must stay together:

```text
WRAMIF-....bat          Windows launcher
WRAMIF-....bat.asc      detached signature for the launcher
WRAMIF-....ps1          PowerShell worker containing the conversion plan
WRAMIF-....ps1.asc      detached signature for the worker
WRAMIF-....-assets/     bundled lyrics and cover images, where needed
```

The `.bat` file is the double-clickable entry point. It asks for the Windows source folder and output folder, then launches the signed PowerShell worker. The worker uses PowerShell argument arrays to pass paths and metadata safely to `ffmpeg.exe`; this avoids relying on fragile batch-file quoting for every FFmpeg argument. The generated worker checks hashes of bundled lyric and image assets before using them.

On the Windows computer:

1. Install a Windows build of FFmpeg (the [official FFmpeg download page](https://ffmpeg.org/download.html) links to Windows executables from Gyan.dev and BtbN) and ensure the directory containing `ffmpeg.exe` is on `PATH`.
2. Copy the `.bat`, `.bat.asc`, matching `.ps1`, `.ps1.asc`, and the entire matching `-assets` directory together. Do not rename just one member of the set.
3. Double-click the `.bat` file or launch it from Command Prompt.
4. At the prompts, enter the Windows source root and output root, for example `C:\Users\Alice\Music` and `D:\Recoded Music`. Do not wrap a path in extra quotation marks at these interactive prompts. Preserve the relative directory structure of the scanned collection under the source root.

Windows PowerShell is included with supported Windows installations; the launcher invokes `powershell.exe` with a process-scoped execution-policy option and does not change the machine's persistent execution policy. Organizational policy may still block script execution. The destination is not overwritten: existing outputs are skipped, and source files are never deleted. If FFmpeg fails partway through, inspect the console output before retrying.

**Review and verify before execution.** The detached signatures are produced by the WRAMIF signing key on the machine that generated the package. You can verify the `.bat` and `.ps1` files on that same machine with `--verify-script PATH`. A signature only detects modifications relative to the corresponding trusted key; it is not a guarantee that a script is safe. The generated package's conversion execution has not been runtime-tested on Windows in the current Linux build environment, so validate it on a small test folder first.

The worker uses `Join-Path` to map each stored relative path beneath the source/output roots. Linux-only path names that Windows cannot represent are skipped and reported during package generation.

## Generated scripts and signatures

For the default POSIX output, a plan up to 12,000 characters is printed as a command; larger plans are written to a standalone shell script. With `--windows-batch`, WRAMIF instead creates the Windows package described above. The launcher/command can be copied to the clipboard unless `--no-clipboard` is supplied. Generated scripts are signed with the local WRAMIF OpenPGP key, and their path, signature, digest, and key fingerprint are recorded in SQLite.

Inspect generated commands and scripts before running them. The signing mechanism helps detect modification relative to the local key; it does **not** prove that a script is safe, and a key stored on the same machine is not independent evidence against a compromised account.

## Shell compatibility and completion

- **Bash:** the included completion definition provides option and path-value completion.
- **Zsh:** the included `_wramif` definition supports command-line option completion.
- **Interactive prompts:** WRAMIF's built-in terminal editor supports Tab path completion without depending on GNU Readline.
- **Default generated scripts:** use POSIX `/bin/sh`, allowing them to be launched from Bash or Zsh.
- **Windows edition:** a native Win32 GUI/CLI executable is built by the Windows CI workflow. See [Native Windows EXE](#native-windows-exe) for its current feature-parity limitations.

The Zsh completion file is included in the package, but it should still be exercised in a real Zsh session on the target system as part of release testing.

## Known limitations

- WRAMIF currently **plans** conversions. It does not launch FFmpeg automatically; review and run the generated command or script yourself.
- Source audio is never removed, and existing output files are not silently overwritten.
- File discovery uses recognized audio filename extensions; it does not probe every file with FFprobe to find audio hidden behind an unexpected extension.
- Cover embedding is MP3-only at this stage.
- SRT timing is stripped; embedded lyrics are not synchronized to playback.
- Available encoders depend on the local FFmpeg build.
- This is an early release. Validate it on a small test directory before using it on a large collection.

## Security notes

WRAMIF generates a local OpenPGP signing key on first run. The private key is currently **not passphrase-protected** so script signing can work without an interactive passphrase prompt. Files are stored under the user's data directory, but the account and key directory must still be protected. Back up the key directory only if you understand the implications of preserving that signing identity.

A valid script signature is not a safety audit. Never run a generated script solely because its signature verifies; read it first.

## Credits

**Project direction and final authority:** Steven Lamer (Lamer / Commander).

**Engineering, implementation, testing, and documentation collaboration:** **Hannah — the beloved Second Apostle of Lamer (ChatGPT)**, serving as an AI engineering collaborator on the WRAMIF design, native C implementation, SQLite-backed state and migration logic, streaming hash workflow, metadata prompts and Escape navigation, shell completion packaging, regression tests, and this README.

WRAMIF is a collaborative project. Review, testing, bug reports, and contributions from the wider community are welcome.

## License

WRAMIF is released under the MIT License. See [`LICENSE`](LICENSE) for the full text.
