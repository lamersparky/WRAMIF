#define _CRT_SECURE_NO_WARNINGS
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#define APP_NAME L"WRAMIF"
#define APP_VERSION L"0.3.0"
#define HASH_BYTES 32
#define HASH_HEX 65
#define READ_CHUNK (256u * 1024u)
#define IDC_SOURCE 1001
#define IDC_OUTPUT 1002
#define IDC_BROWSE_SOURCE 1003
#define IDC_BROWSE_OUTPUT 1004
#define IDC_FORMAT 1005
#define IDC_RECURSIVE 1006
#define IDC_HASH_ALL 1007
#define IDC_GENERATE 1008
#define IDC_STATUS 1009
#define IDC_MODE 1010

static HINSTANCE g_instance;
static HWND g_window, g_source, g_output, g_format, g_recursive, g_hash_all, g_status;
static HFONT g_font;
static HBRUSH g_background;

typedef struct {
    wchar_t *input;
    wchar_t *relative;
    unsigned char hash[HASH_BYTES];
    bool has_hash;
    bool duplicate;
} Job;

typedef struct {
    Job *items;
    size_t count;
    size_t capacity;
} JobList;

typedef struct {
    const wchar_t *target;
    const wchar_t *output;
    const wchar_t *format;
    bool recursive;
    bool hash_all;
} Options;

static wchar_t *wdup(const wchar_t *s) {
    size_t n;
    wchar_t *out;
    if (!s) s = L"";
    n = wcslen(s) + 1;
    out = (wchar_t *)malloc(n * sizeof(wchar_t));
    if (out) memcpy(out, s, n * sizeof(wchar_t));
    return out;
}

static wchar_t *wjoin(const wchar_t *a, const wchar_t *b) {
    size_t na = wcslen(a), nb = wcslen(b);
    bool separator = na && a[na - 1] != L'\\' && a[na - 1] != L'/';
    wchar_t *out = (wchar_t *)malloc((na + nb + (separator ? 2 : 1)) * sizeof(wchar_t));
    if (!out) return NULL;
    memcpy(out, a, na * sizeof(wchar_t));
    if (separator) out[na++] = L'\\';
    memcpy(out + na, b, (nb + 1) * sizeof(wchar_t));
    return out;
}

static const wchar_t *wbase(const wchar_t *path) {
    const wchar_t *a = wcsrchr(path, L'\\');
    const wchar_t *b = wcsrchr(path, L'/');
    const wchar_t *last = a > b ? a : b;
    return last ? last + 1 : path;
}

static bool has_audio_extension(const wchar_t *path) {
    static const wchar_t *exts[] = {
        L".mp3", L".flac", L".wav", L".ogg", L".oga", L".opus", L".m4a",
        L".aac", L".wma", L".aiff", L".aif", L".ape", L".wv", L".mpc",
        L".tta", L".dsf", L".dff", L".m4b", L".amr", L".3ga", L".caf",
        L".au", L".ra", L".rm", L".mpga", L".mp2", L".mid", L".midi"
    };
    const wchar_t *base = wbase(path);
    const wchar_t *dot = wcsrchr(base, L'.');
    size_t i;
    if (!dot) return false;
    for (i = 0; i < sizeof(exts) / sizeof(exts[0]); ++i)
        if (_wcsicmp(dot, exts[i]) == 0) return true;
    return false;
}

static int hash_file(const wchar_t *path, unsigned char digest[HASH_BYTES]) {
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    DWORD object_len = 0, result_len = 0;
    PUCHAR object = NULL;
    HANDLE file = INVALID_HANDLE_VALUE;
    unsigned char buffer[READ_CHUNK];
    DWORD got = 0;
    NTSTATUS status;
    int rc = -1;

    status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (status < 0) goto done;
    status = BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&object_len, sizeof(object_len), &result_len, 0);
    if (status < 0 || object_len == 0) goto done;
    object = (PUCHAR)malloc(object_len);
    if (!object) goto done;
    status = BCryptCreateHash(alg, &hash, object, object_len, NULL, 0, 0);
    if (status < 0) goto done;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE) goto done;
    for (;;) {
        if (!ReadFile(file, buffer, (DWORD)sizeof(buffer), &got, NULL)) goto done;
        if (got == 0) break;
        status = BCryptHashData(hash, buffer, got, 0);
        if (status < 0) goto done;
    }
    status = BCryptFinishHash(hash, digest, HASH_BYTES, 0);
    if (status < 0) goto done;
    rc = 0;
done:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    if (object) { SecureZeroMemory(object, object_len); free(object); }
    SecureZeroMemory(buffer, sizeof(buffer));
    return rc;
}

static bool ensure_hash(Job *job) {
    if (job->has_hash) return true;
    if (hash_file(job->input, job->hash) != 0) return false;
    job->has_hash = true;
    return true;
}

static bool list_add(JobList *list, const wchar_t *root, const wchar_t *path, bool hash_all) {
    Job job;
    size_t root_len = wcslen(root);
    const wchar_t *rel = path + root_len;
    (void)hash_all;
    memset(&job, 0, sizeof(job));
    while (*rel == L'\\' || *rel == L'/') ++rel;
    job.input = wdup(path);
    job.relative = wdup(rel);
    if (!job.input || !job.relative) { free(job.input); free(job.relative); return false; }
    if (list->count == list->capacity) {
        size_t next = list->capacity ? list->capacity * 2 : 128;
        Job *grown = (Job *)realloc(list->items, next * sizeof(Job));
        if (!grown) { free(job.input); free(job.relative); return false; }
        list->items = grown;
        list->capacity = next;
    }
    list->items[list->count++] = job;
    return true;
}

static bool scan_dir(JobList *list, const wchar_t *root, const wchar_t *dir, bool recursive) {
    wchar_t *pattern = wjoin(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE find;
    bool ok = true;
    if (!pattern) return false;
    find = FindFirstFileW(pattern, &fd);
    free(pattern);
    if (find == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    do {
        wchar_t *full;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        full = wjoin(dir, fd.cFileName);
        if (!full) { ok = false; break; }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recursive && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                ok = scan_dir(list, root, full, recursive);
        } else if (has_audio_extension(full)) {
            ok = list_add(list, root, full, false);
        }
        free(full);
        if (!ok) break;
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    return ok;
}

typedef struct { size_t index; } JobIndex;
static JobList *g_sort_list;

static int compare_job_hash(const void *pa, const void *pb) {
    const JobIndex *a = (const JobIndex *)pa, *b = (const JobIndex *)pb;
    const Job *ja = &g_sort_list->items[a->index], *jb = &g_sort_list->items[b->index];
    int cmp;
    if (ja->has_hash != jb->has_hash) return ja->has_hash ? -1 : 1;
    if (!ja->has_hash) return a->index < b->index ? -1 : (a->index > b->index ? 1 : 0);
    cmp = memcmp(ja->hash, jb->hash, HASH_BYTES);
    if (cmp != 0) return cmp;
    return a->index < b->index ? -1 : (a->index > b->index ? 1 : 0);
}

static int compare_job_basename(const void *pa, const void *pb) {
    const JobIndex *a = (const JobIndex *)pa, *b = (const JobIndex *)pb;
    int cmp = _wcsicmp(wbase(g_sort_list->items[a->index].input), wbase(g_sort_list->items[b->index].input));
    if (cmp != 0) return cmp;
    return a->index < b->index ? -1 : (a->index > b->index ? 1 : 0);
}

static JobIndex *make_indices(JobList *list) {
    JobIndex *indices;
    size_t i;
    if (!list->count) return NULL;
    indices = (JobIndex *)malloc(list->count * sizeof(JobIndex));
    if (!indices) return NULL;
    for (i = 0; i < list->count; ++i) indices[i].index = i;
    g_sort_list = list;
    return indices;
}

static void apply_full_dedupe(JobList *list) {
    JobIndex *indices;
    size_t i, start;
    for (i = 0; i < list->count; ++i) {
        if (!ensure_hash(&list->items[i])) {
            fwprintf(stderr, L"WRAMIF: could not hash file; keeping it in the plan: %ls\n", list->items[i].input);
        }
    }
    if (list->count < 2) return;
    indices = make_indices(list);
    if (!indices) { fwprintf(stderr, L"WRAMIF: out of memory during duplicate sorting; retaining all files.\n"); return; }
    qsort(indices, list->count, sizeof(JobIndex), compare_job_hash);
    start = 0;
    while (start < list->count) {
        size_t end = start + 1, k;
        while (end < list->count && list->items[indices[start].index].has_hash &&
              list->items[indices[end].index].has_hash &&
              memcmp(list->items[indices[start].index].hash,
              list->items[indices[end].index].hash, HASH_BYTES) == 0) ++end;
        for (k = start + 1; k < end; ++k)
            list->items[indices[k].index].duplicate = true;
        start = end;
    }
    free(indices);
}

static void apply_basename_dedupe(JobList *list) {
    JobIndex *indices;
    size_t i = 0;
    if (list->count < 2) return;
    indices = make_indices(list);
    if (!indices) { fwprintf(stderr, L"WRAMIF: out of memory during duplicate sorting; retaining all files.\n"); return; }
    qsort(indices, list->count, sizeof(JobIndex), compare_job_basename);
    while (i < list->count) {
        size_t end = i + 1, j, k;
        while (end < list->count && _wcsicmp(wbase(list->items[indices[i].index].input),
               wbase(list->items[indices[end].index].input)) == 0) ++end;
        if (end - i > 1) {
            for (j = i; j < end; ++j) {
                Job *current = &list->items[indices[j].index];
                if (!ensure_hash(current)) {
                    fwprintf(stderr, L"WRAMIF: could not hash same-name file; keeping it: %ls\n", current->input);
                    continue;
                }
                for (k = i; k < j; ++k) {
                    Job *prior = &list->items[indices[k].index];
                    if (!prior->duplicate && ensure_hash(prior) &&
                        memcmp(current->hash, prior->hash, HASH_BYTES) == 0) {
                        current->duplicate = true;
                        break;
                    }
                }
            }
        }
        i = end;
    }
    free(indices);
}

static void free_jobs(JobList *list) {
    size_t i;
    for (i = 0; i < list->count; ++i) {
        free(list->items[i].input);
        free(list->items[i].relative);
        SecureZeroMemory(list->items[i].hash, sizeof(list->items[i].hash));
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool valid_format(const wchar_t *f) {
    return _wcsicmp(f, L"mp3") == 0 || _wcsicmp(f, L"flac") == 0 ||
           _wcsicmp(f, L"opus") == 0 || _wcsicmp(f, L"aac") == 0 || _wcsicmp(f, L"wav") == 0;
}

static bool ps_quote(FILE *f, const wchar_t *s) {
    const wchar_t *p;
    if (fputwc(L'\'', f) == WEOF) return false;
    for (p = s; *p; ++p) {
        if (*p == L'\'' && fputwc(L'\'', f) == WEOF) return false;
        if (fputwc(*p, f) == WEOF) return false;
    }
    return fputwc(L'\'', f) != WEOF;
}

static bool make_local_dirs(wchar_t *path) {
    size_t i, n = wcslen(path);
    for (i = 3; i < n; ++i) {
        if (path[i] == L'\\' || path[i] == L'/') {
            wchar_t saved = path[i]; path[i] = L'\0';
            if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) { path[i] = saved; return false; }
            path[i] = saved;
        }
    }
    return CreateDirectoryW(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static wchar_t *get_scripts_dir(void) {
    wchar_t local[MAX_PATH + 1];
    wchar_t *app, *scripts;
    if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, local) != S_OK) {
        DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
        if (!n || n >= MAX_PATH) return NULL;
    }
    app = wjoin(local, L"WRAMIF");
    if (!app || !make_local_dirs(app)) { free(app); return NULL; }
    scripts = wjoin(app, L"scripts"); free(app);
    if (!scripts || !make_local_dirs(scripts)) { free(scripts); return NULL; }
    return scripts;
}

static void append_history(const wchar_t *scripts_dir, const wchar_t *bat, const wchar_t *ps, size_t count) {
    wchar_t *root = wdup(scripts_dir), *sep, *path;
    FILE *f;
    if (!root) return;
    sep = wcsrchr(root, L'\\');
    if (!sep) sep = wcsrchr(root, L'/');
    if (sep) *sep = L'\0';
    path = wjoin(root, L"history.tsv"); free(root);
    if (!path) return;
    f = _wfopen(path, L"a, ccs=UTF-8"); free(path);
    if (!f) return;
    fwprintf(f, L"%llu\t%ls\t%ls\t%llu\n", (unsigned long long)time(NULL), bat, ps, (unsigned long long)count);
    fclose(f);
}

static bool write_package(const Options *o, const JobList *list, size_t unique_count,
                          wchar_t **bat_out, wchar_t **ps_out) {
    wchar_t *scripts_dir = get_scripts_dir();
    wchar_t stem[80], bat_name[96], ps_name[96];
    wchar_t *bat_path = NULL, *ps_path = NULL;
    SYSTEMTIME now;
    FILE *ps = NULL, *bat = NULL;
    size_t i, emitted = 0;
    bool ok = false;
    const wchar_t *format = o->format ? o->format : L"mp3";
    if (!scripts_dir) { fwprintf(stderr, L"WRAMIF: cannot create %%LOCALAPPDATA%%\\WRAMIF\\scripts.\n"); return false; }
    GetLocalTime(&now);
    swprintf(stem, sizeof(stem)/sizeof(stem[0]), L"WRAMIF_%04u%02u%02u_%02u%02u%02u",
             now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    swprintf(bat_name, sizeof(bat_name)/sizeof(bat_name[0]), L"%ls.bat", stem);
    swprintf(ps_name, sizeof(ps_name)/sizeof(ps_name[0]), L"%ls.ps1", stem);
    bat_path = wjoin(scripts_dir, bat_name);
    ps_path = wjoin(scripts_dir, ps_name);
    if (!bat_path || !ps_path) goto done;
    ps = _wfopen(ps_path, L"wt, ccs=UTF-8");
    if (!ps) { fwprintf(stderr, L"WRAMIF: cannot create PowerShell worker.\n"); goto done; }
    fwprintf(ps, L"$ErrorActionPreference = 'Stop'\n# WRAMIF %ls Windows worker %ls\n", APP_VERSION, stem);
    fwprintf(ps, L"$outputRoot = "); ps_quote(ps, o->output); fwprintf(ps, L"\n$format = "); ps_quote(ps, format); fwprintf(ps, L"\n");
    fwprintf(ps,
        L"$ff = Get-Command ffmpeg.exe -ErrorAction SilentlyContinue\n"
        L"if (-not $ff) { throw 'FFmpeg was not found. Install a Windows FFmpeg build and add ffmpeg.exe to PATH.' }\n"
        L"$codec = switch ($format) {\n"
        L"  'mp3' { @('-c:a','libmp3lame','-q:a','2') }\n"
        L"  'flac' { @('-c:a','flac') }\n"
        L"  'opus' { @('-c:a','libopus','-b:a','160k') }\n"
        L"  'aac' { @('-c:a','aac','-b:a','192k') }\n"
        L"  'wav' { @('-c:a','pcm_s16le') }\n"
        L"}\n$failed = 0\n$completed = 0\n$jobs = @(\n");
    for (i = 0; i < list->count; ++i) {
        if (list->items[i].duplicate) continue;
        if (emitted++) fwprintf(ps, L",\n");
        fwprintf(ps, L"  [pscustomobject]@{ Input = "); ps_quote(ps, list->items[i].input);
        fwprintf(ps, L"; Relative = "); ps_quote(ps, list->items[i].relative);
        fwprintf(ps, L" }");
    }
    fwprintf(ps,
        L"\n)\nforeach ($job in $jobs) {\n"
        L"  $relOut = [IO.Path]::ChangeExtension($job.Relative, $format)\n"
        L"  $out = Join-Path $outputRoot $relOut\n"
        L"  $parent = Split-Path -Parent $out\n"
        L"  New-Item -ItemType Directory -Force -Path $parent | Out-Null\n"
        L"  if (Test-Path -LiteralPath $out) { Write-Host ('SKIP existing: ' + $out); continue }\n"
        L"  Write-Host ('ENCODE: ' + $job.Input + ' -> ' + $out)\n"
        L"  $ffargs = @('-nostdin','-hide_banner','-n','-i',$job.Input,'-map_metadata','0','-vn') + $codec + @($out)\n"
        L"  & $ff.Source @ffargs\n"
        L"  if ($LASTEXITCODE -ne 0) { Write-Warning ('FFmpeg failed: ' + $job.Input); $failed++ } else { $completed++ }\n"
        L"}\nWrite-Host ('WRAMIF complete: ' + $completed + ' converted, ' + $failed + ' failed.')\n"
        L"if ($failed -gt 0) { exit 1 }\n");
    if (ferror(ps)) goto done;
    fclose(ps); ps = NULL;
    bat = _wfopen(bat_path, L"wt");
    if (!bat) { fwprintf(stderr, L"WRAMIF: cannot create batch launcher.\n"); goto done; }
    fwprintf(bat,
        L"@echo off\r\n"
        L"setlocal\r\n"
        L"title WRAMIF Windows Audio Converter\r\n"
        L"echo WRAMIF %ls - Windows conversion package\r\n"
        L"echo Review your source/output paths in the PowerShell worker before running.\r\n"
        L"powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File \"%%~dp0%ls\"\r\n"
        L"set \"WRAMIF_EXIT=%%ERRORLEVEL%%\"\r\n"
        L"echo.\r\n"
        L"if not \"%%WRAMIF_EXIT%%\"==\"0\" echo Some conversions failed. Review the output above.\r\n"
        L"pause\r\n"
        L"exit /b %%WRAMIF_EXIT%%\r\n", APP_VERSION, ps_name);
    if (ferror(bat)) goto done;
    fclose(bat); bat = NULL;
    append_history(scripts_dir, bat_path, ps_path, unique_count);
    *bat_out = wdup(bat_path); *ps_out = wdup(ps_path);
    ok = *bat_out != NULL && *ps_out != NULL;
done:
    if (ps) fclose(ps);
    if (bat) fclose(bat);
    if (!ok) {
        if (bat_path) DeleteFileW(bat_path);
        if (ps_path) DeleteFileW(ps_path);
        free(*bat_out); *bat_out = NULL;
        free(*ps_out); *ps_out = NULL;
    }
    free(scripts_dir); free(bat_path); free(ps_path);
    return ok;
}

static wchar_t *absolute_path(const wchar_t *path) {
    DWORD need, got;
    wchar_t *out;
    size_t n;
    need = GetFullPathNameW(path, 0, NULL, NULL);
    if (need == 0) return NULL;
    out = (wchar_t *)calloc((size_t)need + 2, sizeof(wchar_t));
    if (!out) return NULL;
    got = GetFullPathNameW(path, need + 1, out, NULL);
    if (got == 0 || got > need) { free(out); return NULL; }
    n = wcslen(out);
    /* Drop trailing separators except for a drive root such as C:\\ . */
    while (n > 3 && (out[n - 1] == L'\\' || out[n - 1] == L'/')) out[--n] = L'\\0';
    return out;
}

static bool path_is_within(const wchar_t *parent, const wchar_t *child) {
    size_t n = wcslen(parent);
    if (_wcsnicmp(parent, child, n) != 0) return false;
    if (child[n] == L'\\0') return true;
    if (n > 0 && (parent[n - 1] == L'\\' || parent[n - 1] == L'/')) return true;
    return child[n] == L'\\' || child[n] == L'/';
}

static bool run_scan(const Options *o, wchar_t **bat_out, wchar_t **ps_out, size_t *total_out, size_t *unique_out) {
    JobList list;
    DWORD attrs;
    size_t i, unique = 0;
    bool ok = false;
    wchar_t *target_abs = NULL, *output_abs = NULL;
    Options normalized;
    memset(&list, 0, sizeof(list));
    *bat_out = NULL; *ps_out = NULL; *total_out = 0; *unique_out = 0;
    if (!o->target || !o->output || !*o->target || !*o->output) {
        fwprintf(stderr, L"WRAMIF: both --target and --output are required.\n"); return false;
    }
    if (!valid_format(o->format)) { fwprintf(stderr, L"WRAMIF: unsupported output format.\n"); return false; }
    target_abs = absolute_path(o->target); output_abs = absolute_path(o->output);
    if (!target_abs || !output_abs) { fwprintf(stderr, L"WRAMIF: cannot resolve input/output paths.\n"); goto done; }
    attrs = GetFileAttributesW(target_abs);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        fwprintf(stderr, L"WRAMIF: target is not a directory: %ls\n", target_abs); goto done;
    }
    if (path_is_within(target_abs, output_abs)) {
        fwprintf(stderr, L"WRAMIF: output must not be the target directory or a directory inside it.\n"); goto done;
    }
    normalized = *o; normalized.target = target_abs; normalized.output = output_abs;
    wprintf(L"WRAMIF: scanning %ls\n", normalized.target);
    if (!scan_dir(&list, normalized.target, normalized.target, normalized.recursive)) {
        fwprintf(stderr, L"WRAMIF: scan failed (path access or memory error).\n"); goto done;
    }
    if (normalized.hash_all) {
        wprintf(L"WRAMIF: hashing all audio files for duplicate detection...\n");
        apply_full_dedupe(&list);
    } else {
        apply_basename_dedupe(&list);
    }
    for (i = 0; i < list.count; ++i) if (!list.items[i].duplicate) ++unique;
    wprintf(L"WRAMIF: found %llu audio files; %llu unique files planned.\n",
            (unsigned long long)list.count, (unsigned long long)unique);
    if (!write_package(&normalized, &list, unique, bat_out, ps_out)) goto done;
    *total_out = list.count; *unique_out = unique;
    ok = true;
done:
    free(target_abs); free(output_abs);
    free_jobs(&list);
    return ok;
}

static void console_attach(void) {
    FILE *stream;
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) AllocConsole();
    (void)freopen_s(&stream, "CONIN$", "r", stdin);
    (void)freopen_s(&stream, "CONOUT$", "w", stdout);
    (void)freopen_s(&stream, "CONOUT$", "w", stderr);
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
}

static void usage(void) {
    wprintf(L"WRAMIF %ls — Windows edition\n\n", APP_VERSION);
    wprintf(L"Usage:\n  WRAMIF.exe                         Open the graphical interface\n");
    wprintf(L"  WRAMIF.exe --cli --target PATH --output PATH [options]\n\n");
    wprintf(L"Options:\n  --gui                     Open GUI\n  --cli                     Use command-line mode\n  --target PATH             Source audio folder\n  --output PATH             Destination folder\n  --format mp3|flac|opus|aac|wav\n  --recursive               Include subfolders (default)\n  --no-recursive            Scan only top-level folder\n  --no-duplicates           Hash all files and keep one identical copy\n  --help                    Show this help\n  --version                 Show version\n\n");
    wprintf(L"WRAMIF writes a .bat launcher and a PowerShell worker under %%LOCALAPPDATA%%\\WRAMIF\\scripts.\n");
    wprintf(L"Review generated scripts before running. Conversion starts only when you launch the generated .bat.\n");
}

static bool parse_cli(int argc, wchar_t **argv, Options *o, bool *force_gui, bool *force_cli, bool *help, bool *version) {
    int i;
    memset(o, 0, sizeof(*o));
    o->recursive = true; o->format = L"mp3";
    *force_gui = false; *force_cli = false; *help = false; *version = false;
    for (i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--help") == 0 || wcscmp(argv[i], L"-h") == 0) *help = true;
        else if (wcscmp(argv[i], L"--version") == 0) *version = true;
        else if (wcscmp(argv[i], L"--gui") == 0) *force_gui = true;
        else if (wcscmp(argv[i], L"--cli") == 0) *force_cli = true;
        else if (wcscmp(argv[i], L"--recursive") == 0) o->recursive = true;
        else if (wcscmp(argv[i], L"--no-recursive") == 0) o->recursive = false;
        else if (wcscmp(argv[i], L"--no-duplicates") == 0) o->hash_all = true;
        else if (wcscmp(argv[i], L"--target") == 0 && i + 1 < argc) o->target = argv[++i];
        else if (wcscmp(argv[i], L"--output") == 0 && i + 1 < argc) o->output = argv[++i];
        else if (wcscmp(argv[i], L"--format") == 0 && i + 1 < argc) o->format = argv[++i];
        else if (wcsncmp(argv[i], L"--target=", 9) == 0) o->target = argv[i] + 9;
        else if (wcsncmp(argv[i], L"--output=", 9) == 0) o->output = argv[i] + 9;
        else if (wcsncmp(argv[i], L"--format=", 9) == 0) o->format = argv[i] + 9;
        else { fwprintf(stderr, L"Unknown or incomplete option: %ls\n", argv[i]); return false; }
    }
    return true;
}

static wchar_t *browse_folder(HWND owner) {
    BROWSEINFOW bi;
    LPITEMIDLIST pidl;
    wchar_t display[MAX_PATH] = L"";
    wchar_t path[MAX_PATH] = L"";
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = owner; bi.pszDisplayName = display;
    bi.lpszTitle = L"Choose a folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return NULL;
    if (!SHGetPathFromIDListW(pidl, path)) path[0] = L'\0';
    CoTaskMemFree(pidl);
    return path[0] ? wdup(path) : NULL;
}

static void set_font(HWND h) { if (g_font) SendMessageW(h, WM_SETFONT, (WPARAM)g_font, TRUE); }
static HWND add_control(HWND parent, const wchar_t *kind, const wchar_t *text, DWORD style,
                        int x, int y, int w, int h, int id) {
    HWND control = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, g_instance, NULL);
    set_font(control);
    return control;
}

static void browse_to(HWND edit, HWND owner) {
    wchar_t *path = browse_folder(owner);
    if (path) { SetWindowTextW(edit, path); free(path); }
}

static void generate_from_gui(void) {
    int sl = GetWindowTextLengthW(g_source), ol = GetWindowTextLengthW(g_output);
    wchar_t *source = (wchar_t *)calloc((size_t)sl + 2, sizeof(wchar_t));
    wchar_t *output = (wchar_t *)calloc((size_t)ol + 2, sizeof(wchar_t));
    wchar_t format[16] = L"mp3";
    Options o;
    wchar_t *bat = NULL, *ps = NULL;
    size_t total = 0, unique = 0;
    bool ok;
    if (!source || !output) { free(source); free(output); MessageBoxW(g_window, L"Out of memory.", APP_NAME, MB_ICONERROR); return; }
    GetWindowTextW(g_source, source, sl + 1); GetWindowTextW(g_output, output, ol + 1);
    SendMessageW(g_format, CB_GETLBTEXT, (WPARAM)SendMessageW(g_format, CB_GETCURSEL, 0, 0), (LPARAM)format);
    o.target = source; o.output = output; o.format = format;
    o.recursive = (SendMessageW(g_recursive, BM_GETCHECK, 0, 0) == BST_CHECKED);
    o.hash_all = (SendMessageW(g_hash_all, BM_GETCHECK, 0, 0) == BST_CHECKED);
    SetWindowTextW(g_status, L"Scanning audio files and building package...");
    UpdateWindow(g_status);
    ok = run_scan(&o, &bat, &ps, &total, &unique);
    if (ok) {
        wchar_t msg[2048];
        swprintf(msg, sizeof(msg)/sizeof(msg[0]),
            L"WRAMIF generated a Windows package.\n\nAudio files found: %llu\nUnique files planned: %llu\n\nBatch launcher:\n%ls\n\nPowerShell worker:\n%ls\n\nReview both files before running. FFmpeg conversion does not start until you launch the BAT file.",
            (unsigned long long)total, (unsigned long long)unique, bat, ps);
        SetWindowTextW(g_status, L"Package generated. Review the .bat and .ps1 before execution.");
        MessageBoxW(g_window, msg, L"WRAMIF — package ready", MB_ICONINFORMATION | MB_OK);
    } else {
        SetWindowTextW(g_status, L"Could not generate package. Review the error or choose valid folders.");
        MessageBoxW(g_window, L"Package generation failed. Check the source/output folders and try again.", APP_NAME, MB_ICONERROR | MB_OK);
    }
    free(bat); free(ps); free(source); free(output);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HWND h;
        g_background = CreateSolidBrush(RGB(8, 18, 12));
        g_font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
        h = add_control(hwnd, L"STATIC", L"WRAMIF — WINDOWS AUDIO CONVERSION PLANNER", 0, 22, 18, 700, 28, -1);
        (void)h;
        add_control(hwnd, L"STATIC", L"Source folder", 0, 24, 61, 130, 24, -1);
        g_source = add_control(hwnd, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 24, 84, 565, 30, IDC_SOURCE);
        add_control(hwnd, L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, 600, 84, 110, 30, IDC_BROWSE_SOURCE);
        add_control(hwnd, L"STATIC", L"Output folder", 0, 24, 126, 130, 24, -1);
        g_output = add_control(hwnd, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 24, 149, 565, 30, IDC_OUTPUT);
        add_control(hwnd, L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, 600, 149, 110, 30, IDC_BROWSE_OUTPUT);
        add_control(hwnd, L"STATIC", L"Output format", 0, 24, 194, 120, 24, -1);
        g_format = add_control(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 24, 217, 160, 220, IDC_FORMAT);
        SendMessageW(g_format, CB_ADDSTRING, 0, (LPARAM)L"mp3");
        SendMessageW(g_format, CB_ADDSTRING, 0, (LPARAM)L"flac");
        SendMessageW(g_format, CB_ADDSTRING, 0, (LPARAM)L"opus");
        SendMessageW(g_format, CB_ADDSTRING, 0, (LPARAM)L"aac");
        SendMessageW(g_format, CB_ADDSTRING, 0, (LPARAM)L"wav");
        SendMessageW(g_format, CB_SETCURSEL, 0, 0);
        g_recursive = add_control(hwnd, L"BUTTON", L"Scan subfolders recursively", BS_AUTOCHECKBOX | WS_TABSTOP, 220, 216, 250, 28, IDC_RECURSIVE);
        SendMessageW(g_recursive, BM_SETCHECK, BST_CHECKED, 0);
        g_hash_all = add_control(hwnd, L"BUTTON", L"Hash every file for duplicates (slower)", BS_AUTOCHECKBOX | WS_TABSTOP, 220, 249, 410, 28, IDC_HASH_ALL);
        add_control(hwnd, L"BUTTON", L"Generate Windows .BAT package", BS_PUSHBUTTON | WS_TABSTOP, 24, 300, 300, 42, IDC_GENERATE);
        add_control(hwnd, L"BUTTON", L"Close", BS_PUSHBUTTON | WS_TABSTOP, 600, 300, 110, 42, IDC_MODE);
        g_status = add_control(hwnd, L"STATIC", L"Ready. This application creates scripts; it does not convert audio until you launch the generated .BAT.", 0, 24, 355, 690, 44, IDC_STATUS);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BROWSE_SOURCE: browse_to(g_source, hwnd); return 0;
        case IDC_BROWSE_OUTPUT: browse_to(g_output, hwnd); return 0;
        case IDC_GENERATE: generate_from_gui(); return 0;
        case IDC_MODE: DestroyWindow(hwnd); return 0;
        default: break;
        }
        break;
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc); FillRect((HDC)wp, &rc, g_background); return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp; SetTextColor(dc, RGB(40, 235, 100)); SetBkColor(dc, RGB(8, 18, 12)); return (LRESULT)g_background;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp; SetTextColor(dc, RGB(190, 255, 205)); SetBkColor(dc, RGB(13, 29, 19)); return (LRESULT)g_background;
    }
    case WM_DESTROY:
        if (g_font) DeleteObject(g_font);
        if (g_background) DeleteObject(g_background);
        PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int cli_main(const Options *o) {
    wchar_t *bat = NULL, *ps = NULL;
    size_t total = 0, unique = 0;
    bool ok = run_scan(o, &bat, &ps, &total, &unique);
    if (ok) {
        wprintf(L"\nWRAMIF package ready.\n  Launcher: %ls\n  Worker:   %ls\n  Files:    %llu found, %llu unique\n",
                bat, ps, (unsigned long long)total, (unsigned long long)unique);
        wprintf(L"Review the scripts before running. Source files are not deleted.\n");
    }
    free(bat); free(ps);
    return ok ? 0 : 2;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show) {
    int argc = 0, i, result = 0;
    wchar_t **argv;
    bool force_gui, force_cli, help, version;
    Options options;
    (void)previous; (void)command_line;
    g_instance = instance;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
    if (!parse_cli(argc, argv, &options, &force_gui, &force_cli, &help, &version)) {
        console_attach(); usage(); LocalFree(argv); return 2;
    }
    for (i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--cli") == 0 || wcscmp(argv[i], L"--help") == 0 || wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--version") == 0) break;
    }
    if (help || version || force_cli || (argc > 1 && !force_gui)) {
        console_attach();
        if (help) usage();
        else if (version) wprintf(L"WRAMIF %ls\n", APP_VERSION);
        else if (!options.target || !options.output) { usage(); result = 2; }
        else result = cli_main(&options);
        LocalFree(argv); return result;
    }
    {
        WNDCLASSW wc;
        MSG message;
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        (void)hr;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = wnd_proc; wc.hInstance = instance; wc.lpszClassName = L"WRAMIFMainWindow";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = NULL;
        if (!RegisterClassW(&wc)) { MessageBoxW(NULL, L"Could not register WRAMIF window class.", APP_NAME, MB_ICONERROR); LocalFree(argv); return 2; }
        g_window = CreateWindowExW(0, wc.lpszClassName, L"WRAMIF — Windows Audio Converter",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
            760, 455, NULL, NULL, instance, NULL);
        if (!g_window) { MessageBoxW(NULL, L"Could not create WRAMIF window.", APP_NAME, MB_ICONERROR); CoUninitialize(); LocalFree(argv); return 2; }
        ShowWindow(g_window, show); UpdateWindow(g_window);
        while (GetMessageW(&message, NULL, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
        CoUninitialize();
    }
    LocalFree(argv);
    return result;
}
