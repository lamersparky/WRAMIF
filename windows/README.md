# WRAMIF native Windows edition

The native Windows C source is `wramif_win.c`. It builds a Win32 GUI by default and accepts `--cli` for command-line use. Microsoft Visual C/C++ is used by `.github/workflows/windows-native.yml` to build `WRAMIF.exe`.

## Manual build (x64 Native Tools Command Prompt)

```bat
cl.exe /nologo /utf-8 /O2 /W4 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /D_WIN32_WINNT=0x0601 windows\wramif_win.c /Fe:WRAMIF.exe /link /SUBSYSTEM:WINDOWS bcrypt.lib shell32.lib ole32.lib user32.lib gdi32.lib
```

The app uses Win32 folder dialogs, Windows CNG/BCrypt for SHA-256, and PowerShell to run the generated conversion jobs. Install a native Windows FFmpeg build and put `ffmpeg.exe` on `PATH` before running a generated package.

## CLI

```powershell
.\WRAMIF.exe --cli --target "C:\Music" --output "C:\Music-Recoded" --format mp3 --no-duplicates
.\WRAMIF.exe --help
```

With no arguments, or with `--gui`, the Matrix-styled graphical interface starts. The CLI generates a `.bat` launcher and `.ps1` worker under `%LOCALAPPDATA%\WRAMIF\scripts`; it does not invoke FFmpeg itself.

## Current feature parity

This first native build supports folder scanning, recursive/non-recursive mode, full SHA-256 duplicate detection, choosing an output format, and Windows script generation. It does not yet implement the Linux edition's SQLite resume database, per-track title/artist/album/lyrics/image prompts, local GPG key generation, or GPG script signing. Script history is kept in `%LOCALAPPDATA%\WRAMIF\history.tsv`. The inventory uses an in-memory path list, so the Linux edition remains the recommended choice for unusually large collections until the Windows scanner has a database-backed implementation.
