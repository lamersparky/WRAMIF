# WRAMIF — Windows Recode All Music In Folder

**A Matrix-inspired Windows audio-conversion planner with a native GUI and command-line interface.**

WRAMIF helps you scan a music collection, identify duplicate audio files, choose an output format, and prepare a conversion package for FFmpeg. The native Windows edition opens a graphical interface by default, while command-line mode is available for users who prefer the terminal.

> **Early testing release:** Test with copies of a small music folder first. WRAMIF prepares conversion work for your review; it does not automatically start FFmpeg or delete your source music.

```text
MUSIC FOLDER  ->  SCAN  ->  SHA-256 DUPLICATE CHECK
      ->  CONVERSION PLAN  ->  REVIEW  ->  RUN THE GENERATED WINDOWS LAUNCHER
```

## Contents

- [Features](#features)
- [Download and run on Windows](#download-and-run-on-windows)
- [Requirements](#requirements)
- [Quick start](#quick-start)
- [Command-line options](#command-line-options)
- [How conversion works](#how-conversion-works)
- [Duplicate handling](#duplicate-handling)
- [Files and saved history](#files-and-saved-history)
- [Building from source](#building-from-source)
- [Linux edition and Windows package generation](#linux-edition-and-windows-package-generation)
- [Known limitations](#known-limitations)
- [Security notes](#security-notes)
- [Credits](#credits)
- [License](#license)

## Features

- **Native Windows application:** a Win32 C executable with a Matrix-inspired graphical interface and a command-line mode.
- **GUI by default:** double-click `WRAMIF.exe` to open the interface.
- **CLI option:** run `WRAMIF.exe --cli` for terminal-based operation.
- **Folder browsing:** select source and destination folders in the GUI.
- **Multiple output formats:** choose MP3, FLAC, Opus, AAC, or WAV, subject to the encoders available in your FFmpeg build.
- **Recursive scanning:** include subfolders or limit the scan to the selected folder.
- **SHA-256 duplicate checking:** identify identical audio content, including files with different names.
- **Conversion launcher generation:** creates a Windows `.bat` launcher and a PowerShell worker for the planned conversion.
- **Conservative file handling:** source files are not deleted, and existing destination files are skipped rather than silently overwritten.
- **Local history:** the Windows edition keeps a small local history file.
- **Native C implementation:** the compiled application does not require a Python runtime.

## Download and run on Windows

1. Download the Windows ZIP artifact or release package provided by the project.
2. Extract the ZIP to a folder you can write to.
3. Install FFmpeg for Windows and add the folder containing `ffmpeg.exe` to your `PATH`.
4. Double-click `WRAMIF.exe` to launch the GUI.

To use command-line mode, open Command Prompt or PowerShell in the extracted folder and run:

```powershell
.\WRAMIF.exe --cli --help
```

If Windows cannot find FFmpeg, check that `ffmpeg.exe` is installed and available from a new Command Prompt or PowerShell window:

```powershell
ffmpeg -version
```

The official [FFmpeg download page](https://ffmpeg.org/download.html) links to Windows builds, including builds from Gyan.dev and BtbN.

**Build status:** the project's Windows CI workflow builds `WRAMIF.exe` and publishes a `WRAMIF-Windows-x64-0.3.0.zip` artifact. The executable is produced by the Windows runner, not by the Linux source build. The current Windows edition has not yet been runtime-tested on a physical Windows installation in the development environment, so validate it with a small test folder before relying on it.

## Requirements

- A supported Windows installation.
- `WRAMIF.exe`.
- A native Windows build of FFmpeg, with `ffmpeg.exe` available on `PATH`.
- Write permission for the chosen output directory and WRAMIF's local application-data directory.

No Linux virtual machine is required to run the native Windows edition. PowerShell is used by the generated conversion worker and is included with supported Windows installations.

## Quick start

### Use the graphical interface

1. Double-click `WRAMIF.exe`.
2. Browse to the folder containing your music.
3. Choose an output folder and output format.
4. Configure recursive scanning and duplicate checking as needed.
5. Generate the conversion launcher.
6. Review the generated files and run the `.bat` launcher when you are ready.

### Use the command line

Open PowerShell in the directory containing `WRAMIF.exe`.

Scan a music folder and plan MP3 output:

```powershell
.\WRAMIF.exe --cli --target "C:\Users\Alice\Music" --output "D:\Recoded Music" --format mp3
```

Scan every audio file for duplicate content, including files with different names:

```powershell
.\WRAMIF.exe --cli --target "C:\Users\Alice\Music" --output "D:\Recoded Music" --format mp3 --no-duplicates
```

Choose FLAC output:

```powershell
.\WRAMIF.exe --cli --target "C:\Users\Alice\Music" --output "D:\Music FLAC" --format flac
```

Show available command-line options:

```powershell
.\WRAMIF.exe --cli --help
```

Quote paths that contain spaces, as shown in these examples.

## Command-line options

The native Windows CLI currently supports the following options:

| Option | Description |
|---|---|
| `--cli` | Run in command-line mode instead of opening the GUI. |
| `--gui` | Explicitly request the graphical interface. |
| `--target PATH` | Source directory containing audio files. |
| `--output PATH` | Destination directory for converted audio. Relative folder structure is preserved. |
| `--format FORMAT` | Output format: `mp3`, `flac`, `opus`, `aac`, or `wav`. |
| `--recursive` | Scan subdirectories. |
| `--no-recursive` | Scan only the top-level directory. |
| `--no-duplicates` | Enable full-file SHA-256 duplicate checking; identical content is planned once. |
| `--help` | Print command-line usage. |

Example:

```powershell
.\WRAMIF.exe --cli --target "C:\Users\Alice\Music" --output "D:\Recoded Music" --format mp3 --no-duplicates
```

**Note:** The Windows CLI is a companion edition and does not yet support every option available in the Linux CLI. See [Known limitations](#known-limitations).

## How conversion works

WRAMIF prepares the work; it does not automatically begin conversion just because you scan a folder.

The Windows edition generates a `.bat` launcher and a PowerShell worker under:

```text
%LOCALAPPDATA%\WRAMIF\scripts
```

The generated launcher is the entry point for the conversion plan. It prompts for the Windows source and destination folders, then starts the PowerShell worker. The worker passes paths and metadata to `ffmpeg.exe` using PowerShell argument arrays to reduce quoting problems.

Before running a generated launcher:

1. Review the files it generated.
2. Confirm FFmpeg is installed and accessible through `PATH`.
3. Start with a small test folder.
4. Check the console output for errors before retrying a failed conversion.

The destination's relative folder structure follows the scanned collection. Existing outputs are skipped, and source files are never deleted.

## Duplicate handling

The Windows edition supports full-file SHA-256 duplicate checking.

- Files with identical content can be recognized even if their filenames differ.
- One representative of identical content is included in the conversion plan.
- Duplicate files are omitted from the plan, not deleted from disk.

Use `--no-duplicates` in CLI mode to enable full-file duplicate checking.

## Files and saved history

The Windows application stores its scripts and local data beneath the current user's local application-data directory. Generated scripts are stored under:

```text
%LOCALAPPDATA%\WRAMIF\scripts
```

The Windows edition currently uses a small local history TSV rather than the SQLite-backed run database used by the Linux edition. Its path inventory is held in memory during scanning, and it does not yet provide the Linux edition's full resume and metadata workflow.

## Building from source

The native Windows executable is built by the project's Windows CI workflow. The CI workflow publishes a Windows x64 ZIP artifact containing the executable.

The Linux `make` build instructions below are for the separate Linux CLI edition; they do **not** produce `WRAMIF.exe`.

## Linux edition and Windows package generation

WRAMIF also has a Linux C CLI edition. It can generate a portable Windows conversion package using `--windows-batch` (alias: `--batch-script`). This is a separate workflow from running the native Windows GUI/CLI application.

The Linux CLI requires a C build toolchain, SQLite and OpenSSL development libraries, FFmpeg, GnuPG, and Python 3 for regression tests on Ubuntu/Debian:

```sh
sudo apt update
sudo apt install build-essential pkg-config libsqlite3-dev libssl-dev ffmpeg gnupg python3
make
make test
```

To have the Linux edition generate a Windows package:

```sh
./wramif \
  --target "$HOME/Music" \
  --output "$HOME/Music-recoded" \
  --windows-batch --no-clipboard
```

The generated package contains a `.bat` launcher, a PowerShell worker, detached signatures, and any required lyric or cover assets. Keep the generated files together when copying them to Windows. The recipient does not need a Linux VM to run the package, but does need FFmpeg installed on Windows.

This Linux-generated package supports metadata and signing features that are not yet part of the native Windows application. For the Windows-first workflow, most users should start with the native `WRAMIF.exe`.

## Known limitations

- **Early release:** test on a small folder before processing a large collection.
- **Planning and launch are separate:** WRAMIF prepares the conversion launcher; you decide when to run it.
- **Windows runtime validation:** the native Windows application and generated conversion workflow still need broader testing on real Windows installations.
- **Feature parity:** the native Windows edition does not yet include the Linux edition's SQLite-backed resume, per-track title/artist/album/lyrics/cover prompts, GPG script signing, or Bash/Zsh completion.
- **Cover art and lyrics:** metadata prompts and cover embedding are available in the Linux CLI workflow, not yet in the native Windows GUI/CLI.
- **Recognized extensions:** file discovery uses known audio filename extensions; it does not probe every file with FFprobe to detect audio hidden behind an unexpected extension.
- **Encoder availability:** available encoders and output capabilities depend on the installed FFmpeg build.
- **No source deletion:** source audio is not removed. Existing destination files are skipped rather than silently overwritten.

## Security notes

Only run conversion scripts after reviewing them. The generated PowerShell worker uses argument arrays for FFmpeg arguments, but you should still inspect the generated files before execution.

The Linux CLI's detached OpenPGP signatures help detect modifications relative to its signing key. A valid signature is not a safety audit and does not prove a script is harmless. The native Windows edition does not yet provide the Linux edition's GPG script-signing workflow.

## Credits

**Project direction and final authority:** Steven Lamer (Lamer / Commander).

**Engineering, implementation, testing, and documentation collaboration:** **Hannah — the Second Apostle of Lamer (ChatGPT)**, collaborating on the WRAMIF design, native C implementation, duplicate-checking workflow, conversion planning, tests, and documentation.

WRAMIF is a collaborative project. Review, testing, bug reports, and contributions from the wider community are welcome.

## License

WRAMIF is released under the MIT License. See [`LICENSE`](LICENSE) for the full text.
