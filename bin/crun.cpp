// crun.cpp - A simple C/C++ runner (small version)
// Rewritten to use C standard library and Win32 API for minimal executable size.

#define UNICODE
#define _UNICODE

#include <windows.h>
#include <shellapi.h> // For SHFileOperationW
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#pragma comment(lib, "shell32.lib")

// --- Forward Declarations ---
void fwprintf_err(const wchar_t* format, ...);
BOOL file_exists(const wchar_t* path);
BOOL run_process(wchar_t* command_line, BOOL verbose);
BOOL run_program_and_get_exit_code(wchar_t* command_line, DWORD* p_exit_code);
BOOL run_process_and_capture_output(wchar_t* command_line, wchar_t** output);
BOOL find_executable_in_path(const wchar_t* exe_name, wchar_t* out_path, size_t out_path_size);
void get_parent_path(const wchar_t* path, wchar_t* parent_path, size_t parent_path_size);
const wchar_t* get_extension(const wchar_t* path);
void get_stem(const wchar_t* path, wchar_t* stem, size_t stem_size);
BOOL remove_directory_recursively(const wchar_t* path);

// --- Options Structure ---
struct ProgramOptions {
    wchar_t* source_file;
    wchar_t* compiler_flags;
    wchar_t** program_args;
    int num_program_args;
    const wchar_t* compiler_name;
    BOOL keep_temp;
    BOOL verbose;
    BOOL measure_time;
};

// --- Help and Version ---
void print_help() {
    wprintf(
        L"crun - A simple C/C++ runner (small version).\n\n"
        L"USAGE:\n"
        L"    crun <source_file> [program_arguments...] [options...]\n\n"
        L"OPTIONS:\n"
        L"    --help              Show this help message.\n"
        L"    --version           Show version information.\n"
        L"    --compiler <name>   Specify the compiler ('gcc' or 'clang'). Default: 'gcc'.\n"
        L"    --cflags \"<flags>\"  Pass additional flags to the compiler.\n"
        L"    --keep-temp         Keep the temporary directory after execution.\n"
        L"    --verbose, -v       Enable verbose output.\n"
        L"    --time              Measure and show the execution time.\n"
    );
}

void print_version() {
    wprintf(L"crun 0.4.0 (small)\n");
}

// --- Main Entry Point ---
int main() {
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL || argc < 2) {
        print_help();
        if (argv) LocalFree(argv);
        return 1;
    }

    // --- Argument Parsing ---
    ProgramOptions opts = {0};
    opts.compiler_name = L"gcc";
    opts.program_args = (wchar_t**)malloc(sizeof(wchar_t*) * argc);
    if (!opts.program_args) {
        fwprintf_err(L"Error: Failed to allocate memory for arguments.\n");
        return 1;
    }

    BOOL cflags_next = FALSE;
    BOOL compiler_next = FALSE;

    for (int i = 1; i < argc; ++i) {
        wchar_t* arg = argv[i];
        if (cflags_next) { opts.compiler_flags = arg; cflags_next = FALSE; continue; }
        if (compiler_next) {
            if (wcscmp(arg, L"gcc") == 0 || wcscmp(arg, L"clang") == 0) {
                opts.compiler_name = arg;
            } else {
                fwprintf_err(L"Error: Invalid compiler. Use 'gcc' or 'clang'.\n");
                free(opts.program_args);
                return 1;
            }
            compiler_next = FALSE;
            continue;
        }
        if (wcscmp(arg, L"--help") == 0) { print_help(); free(opts.program_args); return 0; }
        if (wcscmp(arg, L"--version") == 0) { print_version(); free(opts.program_args); return 0; }
        if (wcscmp(arg, L"--keep-temp") == 0) { opts.keep_temp = TRUE; continue; }
        if (wcscmp(arg, L"--verbose") == 0 || wcscmp(arg, L"-v") == 0) { opts.verbose = TRUE; continue; }
        if (wcscmp(arg, L"--time") == 0) { opts.measure_time = TRUE; continue; }
        if (wcscmp(arg, L"--cflags") == 0) { cflags_next = TRUE; continue; }
        if (wcscmp(arg, L"--compiler") == 0) { compiler_next = TRUE; continue; }
        if (wcsncmp(arg, L"--", 2) == 0) {
            fwprintf_err(L"Error: Unknown option '%s'.\n", arg);
            free(opts.program_args);
            return 1;
        }
        if (!opts.source_file) { opts.source_file = arg; }
        else { opts.program_args[opts.num_program_args++] = arg; }
    }

    if (cflags_next || compiler_next) { fwprintf_err(L"Error: Option requires an argument.\n"); free(opts.program_args); return 1; }
    if (!opts.source_file) { fwprintf_err(L"Error: No source file specified.\n"); print_help(); free(opts.program_args); return 1; }

    // --- Path and File Setup ---
    wchar_t full_source_path[MAX_PATH];
    if (!GetFullPathNameW(opts.source_file, MAX_PATH, full_source_path, NULL)) {
        fwprintf_err(L"Error: Could not get full path for source file.\n");
        free(opts.program_args);
        return 1;
    }
    if (!file_exists(full_source_path)) {
        fwprintf_err(L"Error: Source file not found: %s\n", full_source_path);
        free(opts.program_args);
        return 1;
    }
    const wchar_t* ext = get_extension(full_source_path);
    if (!ext || (wcscmp(ext, L".c") != 0 && wcscmp(ext, L".cpp") != 0)) {
        fwprintf_err(L"Error: Unsupported file type. Only .c and .cpp are supported.\n");
        free(opts.program_args);
        return 1;
    }

    wchar_t source_dir[MAX_PATH];
    get_parent_path(full_source_path, source_dir, MAX_PATH);
    wchar_t temp_dir[MAX_PATH];
    swprintf_s(temp_dir, MAX_PATH, L"%s\\crun_tmp_%lu_%lu", source_dir, GetTickCount(), GetCurrentProcessId());
    if (!CreateDirectoryW(temp_dir, NULL)) {
        fwprintf_err(L"Error: Failed to create temporary directory.\n");
        free(opts.program_args);
        return 1;
    }

    wchar_t source_stem[MAX_PATH];
    get_stem(full_source_path, source_stem, MAX_PATH);
    wchar_t executable_path[MAX_PATH];
    swprintf_s(executable_path, MAX_PATH, L"%s\\%s.exe", temp_dir, source_stem);

    // --- Compiler Setup ---
    wchar_t compiler_exe_name[20];
    BOOL is_cpp = (wcscmp(ext, L".cpp") == 0);

    if (is_cpp) {
        if (wcscmp(opts.compiler_name, L"gcc") == 0) {
            wcscpy_s(compiler_exe_name, 20, L"g++.exe");
        } else if (wcscmp(opts.compiler_name, L"clang") == 0) {
            wcscpy_s(compiler_exe_name, 20, L"clang++.exe");
        }
    } else {
        if (wcscmp(opts.compiler_name, L"gcc") == 0) {
            wcscpy_s(compiler_exe_name, 20, L"gcc.exe");
        } else if (wcscmp(opts.compiler_name, L"clang") == 0) {
            wcscpy_s(compiler_exe_name, 20, L"clang.exe");
        }
    }

    wchar_t compiler_path[MAX_PATH];
    if (!find_executable_in_path(compiler_exe_name, compiler_path, MAX_PATH)) {
        fwprintf_err(L"Error: Compiler '%s' not found in PATH.\n", compiler_exe_name);
        if (!opts.keep_temp) remove_directory_recursively(temp_dir);
        free(opts.program_args);
        return 1;
    }

    // --- Compilation ---
    wchar_t compile_command[32767] = {0};
    wchar_t auto_flags[100] = L"-Os -s";

    wchar_t dep_command[MAX_PATH * 2];
    swprintf_s(dep_command, MAX_PATH * 2, L"\"%s\" -MM \"%s\"", compiler_path, full_source_path);
    wchar_t* dep_output = NULL;
    if (run_process_and_capture_output(dep_command, &dep_output) && dep_output) {
        if (wcsstr(dep_output, L"pthread.h")) { wcscat_s(auto_flags, 100, L" -lpthread"); }
        if (wcsstr(dep_output, L"math.h")) { wcscat_s(auto_flags, 100, L" -lm"); }
        free(dep_output);
    }

    swprintf_s(compile_command, 32767, L"\"%s\" \"%s\" -o \"%s\" %s %s",
        compiler_path, full_source_path, executable_path, auto_flags,
        opts.compiler_flags ? opts.compiler_flags : L"");

    if (opts.verbose) wprintf(L"--- Compiling ---\nCommand: %s\n", compile_command);
    if (!run_process(compile_command, opts.verbose)) {
        fwprintf_err(L"Compilation failed.\n");
        if (!opts.keep_temp) remove_directory_recursively(temp_dir);
        free(opts.program_args);
        return 1;
    }
    if (opts.verbose) wprintf(L"Compilation successful.\n");

    // --- Execution ---
    wchar_t run_command[32767];
    swprintf_s(run_command, 32767, L"\"%s\"", executable_path);
    for (int i = 0; i < opts.num_program_args; ++i) {
        wcscat_s(run_command, 32767, L" \"");
        wcscat_s(run_command, 32767, opts.program_args[i]);
        wcscat_s(run_command, 32767, L"\"");
    }

    if (opts.verbose) { wprintf(L"--- Running ---\n"); fflush(stdout); }

    LARGE_INTEGER start_time, end_time, frequency;
    if (opts.measure_time) { QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start_time); }

    DWORD exit_code = 0;
    run_program_and_get_exit_code(run_command, &exit_code);

    if (opts.measure_time) {
        QueryPerformanceCounter(&end_time);
        double elapsed_ms = (double)(end_time.QuadPart - start_time.QuadPart) * 1000.0 / frequency.QuadPart;
        wprintf(L"\nExecution time: %.3f ms\n", elapsed_ms);
    }
    if (opts.verbose) wprintf(L"\n--- Finished ---\nProgram exited with code %lu.\n", exit_code);

    // --- Cleanup ---
    if (!opts.keep_temp) remove_directory_recursively(temp_dir);
    free(opts.program_args);
    return exit_code;
}

// --- Helper Function Implementations ---
void fwprintf_err(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    vfwprintf(stderr, format, args);
    va_end(args);
}

BOOL file_exists(const wchar_t* path) {
    DWORD attrib = GetFileAttributesW(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

BOOL run_process(wchar_t* command_line, BOOL verbose) {
    PROCESS_INFORMATION pi = {0};
    STARTUPINFOW si = {0};
    si.cb = sizeof(STARTUPINFOW);
    // When not verbose, hide the console window of the compiler
    if (!verbose) {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }
    if (!CreateProcessW(NULL, command_line, NULL, NULL, FALSE, verbose ? 0 : CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return FALSE;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code == 0;
}

BOOL run_program_and_get_exit_code(wchar_t* command_line, DWORD* p_exit_code) {
    PROCESS_INFORMATION pi = {0};
    STARTUPINFOW si = {0};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcessW(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        return FALSE;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, p_exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return TRUE;
}

BOOL run_process_and_capture_output(wchar_t* command_line, wchar_t** output) {
    HANDLE h_child_stdout_rd, h_child_stdout_wr;
    SECURITY_ATTRIBUTES sa_attr = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&h_child_stdout_rd, &h_child_stdout_wr, &sa_attr, 0) || !SetHandleInformation(h_child_stdout_rd, HANDLE_FLAG_INHERIT, 0)) return FALSE;

    PROCESS_INFORMATION pi = {0};
    STARTUPINFOW si = {0};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = h_child_stdout_wr;
    si.hStdError = h_child_stdout_wr; // Capture both stdout and stderr
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    if (!CreateProcessW(NULL, command_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(h_child_stdout_rd); CloseHandle(h_child_stdout_wr);
        return FALSE;
    }
    CloseHandle(h_child_stdout_wr);

    char buffer[1024];
    DWORD bytes_read, total_size = 0;
    char* narrow_output = NULL;
    while (ReadFile(h_child_stdout_rd, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read != 0) {
        char* new_output = (char*)realloc(narrow_output, total_size + bytes_read);
        if (!new_output) { free(narrow_output); CloseHandle(h_child_stdout_rd); return FALSE; }
        narrow_output = new_output;
        memcpy(narrow_output + total_size, buffer, bytes_read);
        total_size += bytes_read;
    }
    char* final_output = (char*)realloc(narrow_output, total_size + 1);
    if (!final_output) { free(narrow_output); CloseHandle(h_child_stdout_rd); return FALSE; }
    narrow_output = final_output;
    narrow_output[total_size] = '\0';

    int wchars_num = MultiByteToWideChar(CP_UTF8, 0, narrow_output, -1, NULL, 0);
    *output = (wchar_t*)malloc(wchars_num * sizeof(wchar_t));
    if (*output) MultiByteToWideChar(CP_UTF8, 0, narrow_output, -1, *output, wchars_num);
    free(narrow_output);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(h_child_stdout_rd);
    return exit_code == 0;
}

BOOL find_executable_in_path(const wchar_t* exe_name, wchar_t* out_path, size_t out_path_size) {
    return SearchPathW(NULL, exe_name, NULL, (DWORD)out_path_size, out_path, NULL) > 0;
}

void get_parent_path(const wchar_t* path, wchar_t* parent_path, size_t parent_path_size) {
    wcsncpy_s(parent_path, parent_path_size, path, _TRUNCATE);
    wchar_t* last_slash = wcsrchr(parent_path, L'\\');
    if (last_slash) *last_slash = L'\0';
}

const wchar_t* get_extension(const wchar_t* path) {
    const wchar_t* dot = wcsrchr(path, L'.');
    return (!dot || dot == path) ? NULL : dot;
}

void get_stem(const wchar_t* path, wchar_t* stem, size_t stem_size) {
    const wchar_t* last_slash = wcsrchr(path, L'\\');
    const wchar_t* filename = last_slash ? last_slash + 1 : path;
    wcsncpy_s(stem, stem_size, filename, _TRUNCATE);
    wchar_t* last_dot = wcsrchr(stem, L'.');
    if (last_dot) *last_dot = L'\0';
}

BOOL remove_directory_recursively(const wchar_t* path) {
    wchar_t path_double_null[MAX_PATH + 1] = {0};
    wcsncpy_s(path_double_null, MAX_PATH, path, _TRUNCATE);
    SHFILEOPSTRUCTW file_op = {
        NULL, FO_DELETE, path_double_null, NULL,
        FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
        FALSE, 0, L""
    };
    return SHFileOperationW(&file_op) == 0;
}