// crun.cpp
// C/C++ソースファイルを一時ディレクトリでコンパイルし、実行する簡易ランナー

#define UNICODE
#define _UNICODE

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <random>
#include <sstream>
#include <cstdlib>
#include <windows.h>
#include <shellapi.h> // CommandLineToArgvWのため

#pragma comment(lib, "shell32.lib") // CommandLineToArgvW用

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// オプション情報を保持する構造体
// -----------------------------------------------------------------------------
struct ProgramOptions {
    fs::path source_file;                // ソースファイルのパス
    std::wstring compiler_flags;         // 追加のコンパイラフラグ
    std::vector<std::wstring> program_args; // 実行時引数
    bool keep_temp = false;              // 一時ディレクトリを保持するか
    bool verbose = false;                // 詳細出力を有効にするか
    bool measure_time = false;           // 実行時間を計測するか
};

// -----------------------------------------------------------------------------
// ヘルプメッセージを表示する
// -----------------------------------------------------------------------------
void print_help() {
    std::wcout << L"crun - A simple C/C++ runner.\n\n"
               << L"USAGE:\n"
               << L"    crun <source_file> [program_arguments...] [options...]\n\n"
               << L"OPTIONS:\n"
               << L"    --help              Show this help message.\n"
               << L"    --version           Show version information.\n"
               << L"    --cflags \"<flags>\"  Pass additional flags to the compiler. Overrides auto-detected flags.\n"
               << L"    --keep-temp         Keep the temporary directory after execution.\n"
               << L"    --verbose, -v       Enable verbose output.\n\n"
               << L"    --time              Measure and show the execution time of the program.\n\n"
               << L"AUTOMATIC COMPILATION OPTIONS:\n"
               << L"    crun automatically adds common compiler flags for optimization (-Os, -s) and detects required libraries (e.g., -lpthread, -lm) based on included headers. Use --cflags to override.\n\n"
               << L"EXAMPLE:\n"
               << L"    crun hello.c\n"
               << L"    crun app.cpp arg1 arg2 --verbose\n"
               << L"    crun math.c // Automatically links -lm if math.h is included\n"
               << L"    crun pthread_app.c // Automatically links -lpthread if pthread.h is included\n"
               << L"    crun custom.c --cflags \"-O3 -DDEBUG\" // Override auto-flags\n"
               << L"    crun benchmark.cpp --time\n";
}

// -----------------------------------------------------------------------------
// バージョン情報を表示する
// -----------------------------------------------------------------------------
void print_version() {
    std::wcout << L"crun 0.3.0\n";
}

// -----------------------------------------------------------------------------
// 一意な一時ディレクトリのパスを生成する
// base_path配下に、ナノ秒＋乱数で衝突しにくいディレクトリ名を作る
// -----------------------------------------------------------------------------
fs::path create_unique_temp_dir_path(const fs::path& base_path) {
    auto now = std::chrono::high_resolution_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1000, 9999);
    std::wstringstream ss;
    ss << L"crun_tmp_" << nanos << L"_" << distrib(gen);
    return base_path / ss.str();
}

// -----------------------------------------------------------------------------
// PATH環境変数から指定した実行ファイル(例: gcc.exe)を探す
// 見つかればそのパスを返す。なければ空文字。
// -----------------------------------------------------------------------------
fs::path find_executable_in_path(const std::wstring& exe_name) {
    const wchar_t* path_env_var = _wgetenv(L"PATH");
    if (path_env_var == nullptr) return L"";
    std::wstringstream ss(path_env_var);
    std::wstring path_item;
    while (std::getline(ss, path_item, L';')) {
        if (path_item.empty()) continue;
        fs::path full_path = fs::path(path_item) / exe_name;
        if (fs::exists(full_path)) return full_path;
    }
    return L"";
}

// -----------------------------------------------------------------------------
// 指定したコマンドラインでプロセスを実行し、正常終了したかどうかを返す
// 標準入出力・エラーは親プロセスに引き継ぐ
// -----------------------------------------------------------------------------
bool run_process(const std::wstring& command_line) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(NULL, const_cast<wchar_t*>(command_line.c_str()), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        std::wcerr << L"Error: Failed to execute process. Code: " << GetLastError() << L"\n";
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code == 0;
}

// -----------------------------------------------------------------------------
// 指定したコマンドラインでプロセスを実行し、標準出力をキャプチャして返す
// -----------------------------------------------------------------------------
std::pair<int, std::wstring> run_process_and_capture_output(const std::wstring& command_line) {
    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
        std::wcerr << L"Error: CreatePipe failed. Code: " << GetLastError() << L"\n";
        return {-1, L""};
    }

    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        std::wcerr << L"Error: SetHandleInformation failed. Code: " << GetLastError() << L"\n";
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        return {-1, L""};
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdOutput = hChildStd_OUT_Wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, const_cast<wchar_t*>(command_line.c_str()), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        std::wcerr << L"Error: Failed to execute process for output capture. Code: " << GetLastError() << L"\n";
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        return {-1, L""};
    }

    CloseHandle(hChildStd_OUT_Wr); // Close the write end of the pipe in the parent process

    std::string output_narrow;
    CHAR read_buffer[4096];
    DWORD bytes_read;
    while (ReadFile(hChildStd_OUT_Rd, read_buffer, sizeof(read_buffer) - 1, &bytes_read, NULL) && bytes_read != 0) {
        read_buffer[bytes_read] = '\0';
        output_narrow.append(read_buffer, bytes_read);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStd_OUT_Rd);

    // Convert narrow string (UTF-8 assumed from compiler output) to wide string
    int wchars_num = MultiByteToWideChar(CP_UTF8, 0, output_narrow.c_str(), -1, NULL, 0);
    std::wstring output_wide(wchars_num, 0);
    MultiByteToWideChar(CP_UTF8, 0, output_narrow.c_str(), -1, &output_wide[0], wchars_num);

    return {static_cast<int>(exit_code), output_wide};
}

// -----------------------------------------------------------------------------
// 指定したコマンドラインでプログラムを実行し、終了コードを返す
// 標準入出力・エラーは親プロセスに引き継ぐ
// -----------------------------------------------------------------------------
int run_program_and_get_exit_code(const std::wstring& command_line) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(NULL, const_cast<wchar_t*>(command_line.c_str()), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        std::wcerr << L"Error: Failed to execute program. Code: " << GetLastError() << L"\n";
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exit_code);
}

// -----------------------------------------------------------------------------
// メイン関数: 引数解析、コンパイル、実行、一時ディレクトリ管理
// -----------------------------------------------------------------------------
int main() {
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL) {
        std::cerr << "Fatal: Failed to get command line arguments.\n";
        return 1;
    }

    // 引数解析
    if (argc < 2) {
        print_help();
        LocalFree(argv);
        return 1;
    }

    ProgramOptions options;
    bool cflags_next = false;
    for (int i = 1; i < argc; ++i) {
        std::wstring_view arg(argv[i]);
        if (cflags_next) {
            options.compiler_flags = arg;
            cflags_next = false;
            continue;
        }
        if (arg == L"--help") { print_help(); LocalFree(argv); return 0; }
        if (arg == L"--version") { print_version(); LocalFree(argv); return 0; }
        if (arg == L"--keep-temp") { options.keep_temp = true; continue; }
        if (arg == L"--verbose" || arg == L"-v") { options.verbose = true; continue; }
        if (arg == L"--time") { options.measure_time = true; continue; }
        if (arg == L"--cflags") { cflags_next = true; continue; }
        if (arg.rfind(L"--", 0) == 0) {
            std::wcerr << L"Error: Unknown option '" << arg << L"'.\n"; LocalFree(argv); return 1;
        }
        if (options.source_file.empty()) {
            options.source_file = fs::path(arg);
        } else {
            options.program_args.push_back(std::wstring(arg));
        }
    }

    if (cflags_next) { std::wcerr << L"Error: --cflags requires an argument.\n"; LocalFree(argv); return 1; }
    if (options.source_file.empty()) { std::wcerr << L"Error: No source file specified.\n"; print_help(); LocalFree(argv); return 1; }

    // ソースファイル存在チェック
    if (!fs::exists(options.source_file)) {
        std::wcerr << L"Error: Source file not found: " << options.source_file.c_str() << L"\n";
        LocalFree(argv);
        return 1;
    }

    // 拡張子チェック
    std::wstring ext = options.source_file.extension().wstring();
    if (ext != L".c" && ext != L".cpp") {
        std::wcerr << L"Error: Unsupported file type: " << ext << L". Only .c and .cpp are supported.\n";
        LocalFree(argv); return 1;
    }
    
    // 一時ディレクトリ作成
    fs::path source_dir = options.source_file.parent_path();
    if (source_dir.empty()) { source_dir = L"."; }
    fs::path temp_dir = create_unique_temp_dir_path(source_dir);

    try {
        fs::create_directory(temp_dir);
    } catch (const fs::filesystem_error& e) {
        std::string narrow_what = e.what();
        std::wstring wide_what(narrow_what.begin(), narrow_what.end());
        std::wcerr << L"Error: Failed to create temporary directory: " << wide_what << L"\n";
        LocalFree(argv); return 1;
    }
    
    // 実行ファイルのパスを決定
    fs::path executable_path = temp_dir / options.source_file.stem().replace_extension(L".exe");

    // スコープ終了時に一時ディレクトリを削除するガード
    struct TempDirGuard {
        const fs::path& dir;
        const bool& keep;
        ~TempDirGuard() {
            if (!keep && fs::exists(dir)) {
                std::error_code ec;
                fs::remove_all(dir, ec);
                if (ec) {
                    std::wcerr << L"Warning: Failed to remove temporary directory: " << dir.c_str() << L"\n";
                }
            }
        }
    };
    TempDirGuard guard{temp_dir, options.keep_temp};

    // コンパイラのパスを検索
    fs::path compiler_path;
    if (ext == L".cpp") {
        compiler_path = find_executable_in_path(L"g++.exe");
    } else {
        compiler_path = find_executable_in_path(L"gcc.exe");
    }
    
    if (compiler_path.empty()) {
        std::wcerr << L"Error: Compiler (g++.exe or gcc.exe) not found in PATH.\n"
                   << L"Please install MinGW and add it to your PATH environment variable.\n";
        LocalFree(argv); return 1;
    }

    // コンパイルコマンド生成
    std::wstringstream compile_command;
    compile_command << L"\"" << compiler_path.wstring() << L"\" "
                    << L"\"" << fs::absolute(options.source_file).wstring() << L"\" "
                    << L"-o \"" << executable_path.wstring() << L"\" ";

    // デフォルトの最適化・サイズ削減フラグ
    std::wstring auto_flags = L"-Os -s";

    // 依存関係を解析し、自動でコンパイラフラグを追加
    std::wstringstream dependency_command;
    dependency_command << L"\"" << compiler_path.wstring() << L"\" -MM \"" << fs::absolute(options.source_file).wstring() << L"\"";

    if (options.verbose) {
        std::wcout << L"--- Analyzing Dependencies ---\n" << L"Command: " << dependency_command.str() << L"\n";
    }

    auto [dep_exit_code, dep_output] = run_process_and_capture_output(dependency_command.str());

    if (dep_exit_code == 0) {
        // 依存関係の出力からヘッダファイルを解析し、必要なライブラリフラグを追加
        // 簡単な例として、pthread.h があれば -lpthread を追加
        if (dep_output.find(L"pthread.h") != std::wstring::npos) {
            auto_flags += L" -lpthread";
        }
        // 他のライブラリもここに追加可能 (例: math.h -> -lm, etc.)
        if (dep_output.find(L"math.h") != std::wstring::npos) {
            auto_flags += L" -lm";
        }
    } else {
        if (options.verbose) {
            std::wcerr << L"Warning: Failed to analyze dependencies. Continuing without auto-flags.\n";
        }
    }

    compile_command << auto_flags;

    // ユーザーが指定したフラグがあれば追加
    if (!options.compiler_flags.empty()) {
        compile_command << L" " << options.compiler_flags;
    }

    // 詳細出力
    if (options.verbose) {
        std::wcout << L"--- Compiling ---\n" << L"Command: " << compile_command.str() << L"\n";
    }

    // コンパイル実行
    if (!run_process(compile_command.str())) {
        if (options.verbose) {
             std::wcerr << L"-------------------\n";
        }
        std::wcerr << L"Compilation failed.\n";
        LocalFree(argv); return 1;
    }

    if (options.verbose) {
        std::wcout << L"Compilation successful.\n";
    }

    // 実行コマンド生成
    std::wstringstream run_command;
    run_command << L"\"" << executable_path.wstring() << L"\"";
    for (const auto& arg : options.program_args) {
        run_command << L" \"" << arg << L"\"";
    }

    if (options.verbose) {
        std::wcout << L"--- Running ---\n" << std::flush;
    }

    // プログラム実行
    auto start_time = std::chrono::high_resolution_clock::now();
    int exit_code = run_program_and_get_exit_code(run_command.str());
    auto end_time = std::chrono::high_resolution_clock::now();
    
    if (options.verbose) {
        std::wcout << L"\n--- Finished ---\n" << L"Program exited with code " << exit_code << L".\n";
    }

    // 実行時間を表示
    if (options.measure_time) {
        std::chrono::duration<double, std::milli> elapsed_ms = end_time - start_time;
        std::wcout << L"\nExecution time: " << elapsed_ms.count() << L" ms\n";
    }

    LocalFree(argv);
    return exit_code;
}