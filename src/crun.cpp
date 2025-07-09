// crun.cpp - A simple C/C++ runner
// Rewritten to use C standard library and Win32 API for minimal executable size.
// crun.cpp - 単純なC/C++実行ツール
// 実行ファイルのサイズを最小化するため、C標準ライブラリとWin32 APIを使って書き直されています。

#define UNICODE
#define _UNICODE

#include <windows.h>
#include <shellapi.h> // For SHFileOperationW
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#pragma comment(lib, "shell32.lib") // SHFileOperationW のためにリンク

// --- Forward Declarations ---
// --- 関数プロトタイプ宣言 ---
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
BOOL read_file_content_wide(const wchar_t* path, wchar_t** content);
void clean_temp_directories(const wchar_t* target_dir);

// --- Options Structure ---
// --- プログラム設定を保持する構造体 ---
struct ProgramOptions {
    wchar_t* source_file;      // ソースファイルパス
    wchar_t* compiler_flags;   // コンパイラフラグ
    wchar_t** program_args;    // プログラム引数
    int num_program_args;      // プログラム引数の数
    const wchar_t* compiler_name; // コンパイラ名 ("gcc" or "clang")
    BOOL keep_temp;            // 一時ディレクトリを保持するか
    BOOL verbose;              // 詳細出力を有効にするか
    BOOL measure_time;         // 実行時間を計測するか
    BOOL warnings_all;         // 全ての警告を有効にするか
    BOOL debug_build;          // デバッグビルドを有効にするか
};

// --- Help and Version ---
// --- ヘルプとバージョン情報を表示する関数 ---
void print_help() {
    wprintf(
        L"crun - A simple C/C++ runner.\n\n"
        L"USAGE:\n"
        L"    crun <source_file> [program_arguments...] [options...]\n"
        L"    crun --clean\n\n"
        L"OPTIONS:\n"
        L"    --help              Show this help message.\n"
        L"    --version           Show version information.\n"
        L"    --compiler <name>   Specify the compiler ('gcc' or 'clang'). Default: 'gcc'.\n"
        L"    --cflags \"<flags>\"  Pass additional flags to the compiler.\n"
        L"    --keep-temp         Keep the temporary directory after execution.\n"
        L"    --verbose, -v       Enable verbose output.\n"
        L"    --time              Measure and show the execution time.\n"
        L"    --wall              Enable all compiler warnings (-Wall).\n"
        L"    --debug, -g         Enable debug build (-g).\n"
        L"    --clean             Remove temporary directories (crun_tmp_*) from the current directory.\n"
    );
}

void print_version() {
    wprintf(L"crun 0.6.0\n");
}

// --- Main Entry Point ---
// --- メインエントリーポイント ---
int main() {
    int argc;
    // コマンドライン引数をワイド文字列として取得
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL) { return 1; } // Should not happen

    // --clean オプションを特別に処理
    if (argc == 2 && wcscmp(argv[1], L"--clean") == 0) {
        wchar_t current_dir[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, current_dir);
        clean_temp_directories(current_dir);
        LocalFree(argv);
        return 0;
    }

    if (argc < 2) {
        print_help();
        LocalFree(argv);
        return 1;
    }

    // --- Argument Parsing ---
    // --- 引数解析 ---
    ProgramOptions opts = {0};
    opts.compiler_name = L"gcc"; // デフォルトコンパイラ
    opts.program_args = (wchar_t**)malloc(sizeof(wchar_t*) * argc);
    if (!opts.program_args) {
        fwprintf_err(L"Error: Failed to allocate memory for arguments.\n");
        LocalFree(argv);
        return 1;
    }

    BOOL cflags_next = FALSE;    // --cflags の次の引数をフラグとして解釈するためのフラグ
    BOOL compiler_next = FALSE; // --compiler の次の引数をコンパイラ名として解釈するためのフラグ

    for (int i = 1; i < argc; ++i) {
        wchar_t* arg = argv[i];
        if (cflags_next) { opts.compiler_flags = arg; cflags_next = FALSE; continue; }
        if (compiler_next) {
            if (wcscmp(arg, L"gcc") == 0 || wcscmp(arg, L"clang") == 0) {
                opts.compiler_name = arg;
            } else {
                fwprintf_err(L"Error: Invalid compiler. Use 'gcc' or 'clang'.\n");
                free(opts.program_args);
                LocalFree(argv);
                return 1;
            }
            compiler_next = FALSE;
            continue;
        }
        if (wcscmp(arg, L"--help") == 0) { print_help(); free(opts.program_args); LocalFree(argv); return 0; }
        if (wcscmp(arg, L"--version") == 0) { print_version(); free(opts.program_args); LocalFree(argv); return 0; }
        if (wcscmp(arg, L"--keep-temp") == 0) { opts.keep_temp = TRUE; continue; }
        if (wcscmp(arg, L"--verbose") == 0 || wcscmp(arg, L"-v") == 0) { opts.verbose = TRUE; continue; }
        if (wcscmp(arg, L"--time") == 0) { opts.measure_time = TRUE; continue; }
        if (wcscmp(arg, L"--wall") == 0) { opts.warnings_all = TRUE; continue; }
        if (wcscmp(arg, L"--debug") == 0 || wcscmp(arg, L"-g") == 0) { opts.debug_build = TRUE; continue; }
        if (wcscmp(arg, L"--clean") == 0) { continue; } // Special handling at the start
        if (wcscmp(arg, L"--cflags") == 0) { cflags_next = TRUE; continue; }
        if (wcscmp(arg, L"--compiler") == 0) { compiler_next = TRUE; continue; }

        if (wcsncmp(arg, L"--", 2) == 0) {
            fwprintf_err(L"Error: Unknown option '%s'.\n", arg);
            free(opts.program_args);
            LocalFree(argv);
            return 1;
        }
        // オプションでない最初の引数をソースファイルとして解釈
        if (!opts.source_file) { opts.source_file = arg; }
        // それ以降の引数をプログラムの引数として解釈
        else { opts.program_args[opts.num_program_args++] = arg; }
    }

    if (cflags_next || compiler_next) { fwprintf_err(L"Error: Option requires an argument.\n"); free(opts.program_args); LocalFree(argv); return 1; }
    if (!opts.source_file) { fwprintf_err(L"Error: No source file specified.\n"); print_help(); free(opts.program_args); LocalFree(argv); return 1; }

    // --- Path and File Setup ---
    // --- パスとファイルの設定 ---
    wchar_t full_source_path[MAX_PATH];
    // ソースファイルのフルパスを取得
    if (!GetFullPathNameW(opts.source_file, MAX_PATH, full_source_path, NULL)) {
        fwprintf_err(L"Error: Could not get full path for source file.\n");
        free(opts.program_args);
        LocalFree(argv);
        return 1;
    }
    if (!file_exists(full_source_path)) {
        fwprintf_err(L"Error: Source file not found: %s\n", full_source_path);
        free(opts.program_args);
        LocalFree(argv);
        return 1;
    }
    const wchar_t* ext = get_extension(full_source_path);
    if (!ext || (wcscmp(ext, L".c") != 0 && wcscmp(ext, L".cpp") != 0)) {
        fwprintf_err(L"Error: Unsupported file type. Only .c and .cpp are supported.\n");
        free(opts.program_args);
        LocalFree(argv);
        return 1;
    }

    // 一時ディレクトリを作成
    wchar_t source_dir[MAX_PATH];
    get_parent_path(full_source_path, source_dir, MAX_PATH);
    wchar_t temp_dir[MAX_PATH];
    swprintf_s(temp_dir, MAX_PATH, L"%s\\crun_tmp_%lu_%lu", source_dir, GetTickCount(), GetCurrentProcessId());
    if (!CreateDirectoryW(temp_dir, NULL)) {
        fwprintf_err(L"Error: Failed to create temporary directory.\n");
        free(opts.program_args);
        LocalFree(argv);
        return 1;
    }

    // 実行ファイルパスを生成
    wchar_t source_stem[MAX_PATH];
    get_stem(full_source_path, source_stem, MAX_PATH);
    wchar_t executable_path[MAX_PATH];
    swprintf_s(executable_path, MAX_PATH, L"%s\\%s.exe", temp_dir, source_stem);

    // --- Compiler Setup ---
    // --- コンパイラの設定 ---
    wchar_t compiler_exe_name[20];
    BOOL is_cpp = (wcscmp(ext, L".cpp") == 0);

    // 拡張子に応じてコンパイラ実行ファイル名を決定
    if (is_cpp) {
        wcscpy_s(compiler_exe_name, 20, (wcscmp(opts.compiler_name, L"gcc") == 0) ? L"g++.exe" : L"clang++.exe");
    } else {
        wcscpy_s(compiler_exe_name, 20, (wcscmp(opts.compiler_name, L"gcc") == 0) ? L"gcc.exe" : L"clang.exe");
    }

    // PATH環境変数からコンパイラのフルパスを検索
    wchar_t compiler_path[MAX_PATH];
    if (!find_executable_in_path(compiler_exe_name, compiler_path, MAX_PATH)) {
        fwprintf_err(L"Error: Compiler '%s' not found in PATH.\n", compiler_exe_name);
        if (!opts.keep_temp) remove_directory_recursively(temp_dir);
        free(opts.program_args);
        LocalFree(argv);
        return 1;
    }

    // --- Compilation ---
    // --- コンパイル ---
    wchar_t compile_command[32767] = {0};
    wchar_t auto_flags[256] = L""; // 自動フラグ

    // ビルドの種類に応じてフラグを設定
    if (opts.debug_build) {
        wcscpy_s(auto_flags, 256, L"-g"); // デバッグ情報
    } else {
        wcscpy_s(auto_flags, 256, L"-O2 -s"); // リリースビルド用の最適化
    }

    // ソースコードの内容を読み込む
    wchar_t* source_content = NULL;
    if (read_file_content_wide(full_source_path, &source_content)) {
        // Windows APIヘッダのインクルードをチェック
        if (wcsstr(source_content, L"<windows.h>")) { wcscat_s(auto_flags, 256, L" -lkernel32 -luser32 -lshell32 -lgdi32 -lwinspool -lcomdlg32 -ladvapi32"); }
        if (wcsstr(source_content, L"<winsock2.h>") || wcsstr(source_content, L"<winsock.h>")) { wcscat_s(auto_flags, 256, L" -lws2_32"); }
        if (wcsstr(source_content, L"<shlobj.h>")) { wcscat_s(auto_flags, 256, L" -lole32"); }

        // 標準ライブラリヘッダのインクルードをチェック
        if (wcsstr(source_content, L"<pthread.h>")) { wcscat_s(auto_flags, 256, L" -lpthread"); }
        if (wcsstr(source_content, L"<math.h>")) { wcscat_s(auto_flags, 256, L" -lm"); }

        free(source_content); // メモリを解放
    } else {
        // ファイルが読み込めない場合、従来のヘッダ依存性チェックにフォールバック
        wchar_t dep_command[MAX_PATH * 2];
        swprintf_s(dep_command, MAX_PATH * 2, L"\"%s\" -MM \"%s\"", compiler_path, full_source_path);
        wchar_t* dep_output = NULL;
        if (run_process_and_capture_output(dep_command, &dep_output) && dep_output) {
            if (wcsstr(dep_output, L"pthread.h")) { wcscat_s(auto_flags, 256, L" -lpthread"); }
            if (wcsstr(dep_output, L"math.h")) { wcscat_s(auto_flags, 256, L" -lm"); }
            free(dep_output);
        }
    }

    // 警告フラグを追加
    if (opts.warnings_all) {
        wcscat_s(auto_flags, 256, L" -Wall");
    }

    // 最終的なコンパイルコマンドを構築
    swprintf_s(compile_command, 32767, L"\"%s\" \"%s\" -o \"%s\" %s %s",
        compiler_path, full_source_path, executable_path, auto_flags,
        opts.compiler_flags ? opts.compiler_flags : L"");

    if (opts.verbose) wprintf(L"--- Compiling ---\nCommand: %s\n", compile_command);
    if (!run_process(compile_command, opts.verbose)) {
        fwprintf_err(L"Compilation failed.\n");
        if (!opts.keep_temp) remove_directory_recursively(temp_dir);
        free(opts.program_args);
        LocalFree(argv);
        return 1;
    }
    if (opts.verbose) wprintf(L"Compilation successful.\n");

    // --- Execution ---
    // --- 実行 ---
    wchar_t run_command[32767];
    swprintf_s(run_command, 32767, L"\"%s\"", executable_path);
    // プログラム引数をコマンドラインに追加
    for (int i = 0; i < opts.num_program_args; ++i) {
        wcscat_s(run_command, 32767, L" \"");
        wcscat_s(run_command, 32767, opts.program_args[i]);
        wcscat_s(run_command, 32767, L"\"");
    }

    if (opts.verbose) { wprintf(L"--- Running ---\n"); fflush(stdout); }

    // 実行時間を計測
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
    // --- クリーンアップ ---
    if (!opts.keep_temp) remove_directory_recursively(temp_dir);
    free(opts.program_args);
    LocalFree(argv);
    return exit_code;
}

// --- Helper Function Implementations ---
// --- ヘルパー関数の実装 ---

// 標準エラー出力に書式付きで出力
void fwprintf_err(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    vfwprintf(stderr, format, args);
    va_end(args);
}

// ファイルの存在を確認
BOOL file_exists(const wchar_t* path) {
    DWORD attrib = GetFileAttributesW(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

// プロセスを実行し、完了を待つ
BOOL run_process(wchar_t* command_line, BOOL verbose) {
    PROCESS_INFORMATION pi = {0};
    STARTUPINFOW si = {0};
    si.cb = sizeof(STARTUPINFOW);
    // verboseでない場合、コンパイラのコンソールウィンドウを非表示にする
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

// プログラムを実行し、標準入出力を引き継いで終了コードを取得
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

// プロセスを実行し、その標準出力をキャプチャする
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
    si.hStdError = h_child_stdout_wr; // 標準エラー出力もキャプチャ
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    if (!CreateProcessW(NULL, command_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(h_child_stdout_rd); CloseHandle(h_child_stdout_wr);
        return FALSE;
    }
    CloseHandle(h_child_stdout_wr); // 書き込みハンドルは不要なので閉じる

    // パイプから出力を読み取る
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

    // UTF-8からワイド文字列に変換
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

// PATH環境変数から実行ファイルを検索
BOOL find_executable_in_path(const wchar_t* exe_name, wchar_t* out_path, size_t out_path_size) {
    return SearchPathW(NULL, exe_name, NULL, (DWORD)out_path_size, out_path, NULL) > 0;
}

// フルパスから親ディレクトリのパスを取得
void get_parent_path(const wchar_t* path, wchar_t* parent_path, size_t parent_path_size) {
    wcsncpy_s(parent_path, parent_path_size, path, _TRUNCATE);
    wchar_t* last_slash = wcsrchr(parent_path, L'\\');
    if (last_slash) *last_slash = L'\0';
}

// パスから拡張子を取得
const wchar_t* get_extension(const wchar_t* path) {
    const wchar_t* dot = wcsrchr(path, L'.');
    return (!dot || dot == path) ? NULL : dot;
}

// パスから拡張子を除いたファイル名（ステム）を取得
void get_stem(const wchar_t* path, wchar_t* stem, size_t stem_size) {
    const wchar_t* last_slash = wcsrchr(path, L'\\');
    const wchar_t* filename = last_slash ? last_slash + 1 : path;
    wcsncpy_s(stem, stem_size, filename, _TRUNCATE);
    wchar_t* last_dot = wcsrchr(stem, L'.');
    if (last_dot) *last_dot = L'\0';
}

// ディレクトリを再帰的に削除
BOOL remove_directory_recursively(const wchar_t* path) {
    wchar_t path_double_null[MAX_PATH + 1] = {0};
    wcsncpy_s(path_double_null, MAX_PATH, path, _TRUNCATE);
    // SHFileOperationW は二重ヌル終端文字列を要求する
    SHFILEOPSTRUCTW file_op = {
        NULL, FO_DELETE, path_double_null, NULL,
        FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
        FALSE, 0, L""
    };
    return SHFileOperationW(&file_op) == 0;
}

// ワイド文字(UTF-16)でファイル内容を読み込む
BOOL read_file_content_wide(const wchar_t* path, wchar_t** content) {
    HANDLE h_file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h_file == INVALID_HANDLE_VALUE) return FALSE;

    DWORD file_size = GetFileSize(h_file, NULL);
    if (file_size == INVALID_FILE_SIZE) { CloseHandle(h_file); return FALSE; }

    char* buffer = (char*)malloc(file_size + 1);
    if (!buffer) { CloseHandle(h_file); return FALSE; }

    DWORD bytes_read;
    if (!ReadFile(h_file, buffer, file_size, &bytes_read, NULL) || bytes_read != file_size) {
        free(buffer); CloseHandle(h_file); return FALSE;
    }
    buffer[file_size] = '\0';
    CloseHandle(h_file);

    // BOMチェック (UTF-8 or other encodings)
    int offset = 0;
    UINT code_page = CP_ACP; // Default to ANSI codepage
    if (file_size >= 3 && (unsigned char)buffer[0] == 0xEF && (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF) {
        code_page = CP_UTF8;
        offset = 3;
    } else if (file_size >= 2 && (unsigned char)buffer[0] == 0xFF && (unsigned char)buffer[1] == 0xFE) {
        // UTF-16 LE, just copy memory
        *content = (wchar_t*)malloc(file_size - 1);
        if (*content) {
            memcpy(*content, buffer + 2, file_size - 2);
            (*content)[(file_size / 2) - 1] = L'\0';
        }
        free(buffer);
        return *content != NULL;
    }

    int wide_char_size = MultiByteToWideChar(code_page, 0, buffer + offset, -1, NULL, 0);
    if (wide_char_size == 0) { free(buffer); return FALSE; }

    *content = (wchar_t*)malloc(wide_char_size * sizeof(wchar_t));
    if (!*content) { free(buffer); return FALSE; }

    if (MultiByteToWideChar(code_page, 0, buffer + offset, -1, *content, wide_char_size) == 0) {
        free(*content); free(buffer);
        return FALSE;
    }

    free(buffer);
    return TRUE;
}

// crunの一時ディレクトリを掃除する
void clean_temp_directories(const wchar_t* target_dir) {
    wchar_t search_path[MAX_PATH];
    swprintf_s(search_path, MAX_PATH, L"%s\\crun_tmp_*", target_dir);

    WIN32_FIND_DATAW find_data;
    HANDLE h_find = FindFirstFileW(search_path, &find_data);

    if (h_find == INVALID_HANDLE_VALUE) {
        wprintf(L"No crun temporary directories to clean.\n");
        return;
    }

    int count = 0;
    do {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            wchar_t dir_to_delete[MAX_PATH];
            swprintf_s(dir_to_delete, MAX_PATH, L"%s\\%s", target_dir, find_data.cFileName);
            wprintf(L"Removing: %s\n", dir_to_delete);
            if (remove_directory_recursively(dir_to_delete)) {
                count++;
            } else {
                fwprintf_err(L"Warning: Failed to remove directory %s\n", dir_to_delete);
            }
        }
    } while (FindNextFileW(h_find, &find_data) != 0);

    FindClose(h_find);

    if (count > 0) {
        wprintf(L"\nSuccessfully removed %d temporary director(y/ies).\n", count);
    } else {
        wprintf(L"No crun temporary directories found to clean.\n");
    }
}