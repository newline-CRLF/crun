// crun.cpp

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
#include <shellapi.h> // CommandLineToArgvW のため

#pragma comment(lib, "shell32.lib") // CommandLineToArgvW をリンクするため

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// 構造体とヘルパー関数
// -----------------------------------------------------------------------------
struct ProgramOptions {
    fs::path source_file;
    std::wstring compiler_flags;
    std::vector<std::wstring> program_args;
    bool keep_temp = false; // 一時ディレクトリを保持するオプション
    bool verbose = false; // 詳細出力オプション
};

void print_help() {
    std::wcout << L"crun - A simple C/C++ runner.\n\n"
               << L"USAGE:\n"
               << L"    crun <source_file> [program_arguments...] [options...]\n\n"
               << L"OPTIONS:\n"
               << L"    --help              Show this help message.\n"
               << L"    --version           Show version information.\n"
               << L"    --cflags \"<flags>\"  Pass additional flags to the compiler.\n"
               << L"    --keep-temp         Keep the temporary directory after execution.\n"
               << L"    --verbose, -v       Enable verbose output.\n\n"
               << L"EXAMPLE:\n"
               << L"    crun hello.c\n"
               << L"    crun app.cpp arg1 arg2 --verbose\n"
               << L"    crun math.c --cflags \"-O2 -lm\"\n";
}

void print_version() {
    std::wcout << L"crun 0.1.1\n";
}

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

bool run_process(const std::wstring& command_line) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
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
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return exit_code == 0;
}

int run_program_and_get_exit_code(const std::wstring& command_line) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
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
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return static_cast<int>(exit_code);
}


// -----------------------------------------------------------------------------
// ★★★ main関数 ★★★
// -----------------------------------------------------------------------------
int main() {
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL) {
        std::cerr << "Fatal: Failed to get command line arguments.\n";
        return 1;
    }

    // --- 引数解析 ---
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
        // ★ --verbose と -v を追加
        if (arg == L"--verbose" || arg == L"-v") { options.verbose = true; continue; }
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

    // --- ファイルシステム準備 ---
    if (!fs::exists(options.source_file)) {
        std::wcerr << L"Error: Source file not found: " << options.source_file.c_str() << L"\n";
        LocalFree(argv);
        return 1;
    }

    std::wstring ext = options.source_file.extension().wstring();
    if (ext != L".c" && ext != L".cpp") {
        std::wcerr << L"Error: Unsupported file type: " << ext << L". Only .c and .cpp are supported.\n";
        LocalFree(argv); return 1;
    }
    
    fs::path source_dir = options.source_file.parent_path();
    if (source_dir.empty()) { source_dir = L"."; }
    fs::path temp_dir = create_unique_temp_dir_path(source_dir);

    try { fs::create_directory(temp_dir); }
    catch (const fs::filesystem_error& e) {
        std::string narrow_what = e.what(); std::wstring wide_what(narrow_what.begin(), narrow_what.end());
        std::wcerr << L"Error: Failed to create temporary directory: " << wide_what << L"\n";
        LocalFree(argv); return 1;
    }
    
    fs::path executable_path = temp_dir / options.source_file.stem().replace_extension(L".exe");

    struct TempDirGuard {
        const fs::path& dir; const bool& keep;
        ~TempDirGuard() {
            if (!keep && fs::exists(dir)) {
                std::error_code ec; fs::remove_all(dir, ec);
                if (ec) { std::wcerr << L"Warning: Failed to remove temporary directory: " << dir.c_str() << L"\n"; }
            }
        }
    };
    TempDirGuard guard{temp_dir, options.keep_temp};

    // --- コンパイルと実行 ---
    fs::path compiler_path;
    if (ext == L".cpp") { compiler_path = find_executable_in_path(L"g++.exe"); }
    else { compiler_path = find_executable_in_path(L"gcc.exe"); }
    
    if (compiler_path.empty()) {
        std::wcerr << L"Error: Compiler (g++.exe or gcc.exe) not found in PATH.\n"
                   << L"Please install MinGW and add it to your PATH environment variable.\n";
        LocalFree(argv); return 1;
    }

    std::wstringstream compile_command;
    compile_command << L"\"" << compiler_path.wstring() << L"\" "
                    << L"\"" << fs::absolute(options.source_file).wstring() << L"\" "
                    << L"-o \"" << executable_path.wstring() << L"\" "
                    << options.compiler_flags;

    // ★ verbose フラグで出力制御
    if (options.verbose) {
        std::wcout << L"--- Compiling ---\n" << L"Command: " << compile_command.str() << L"\n";
    }

    if (!run_process(compile_command.str())) {
        // コンパイルエラーは常に表示する
        if (options.verbose) {
             std::wcerr << L"-------------------\n";
        }
        std::wcerr << L"Compilation failed.\n";
        LocalFree(argv); return 1;
    }

    if (options.verbose) {
        std::wcout << L"Compilation successful.\n";
    }

    std::wstringstream run_command;
    run_command << L"\"" << executable_path.wstring() << L"\"";
    for (const auto& arg : options.program_args) {
        run_command << L" \"" << arg << L"\"";
    }

    if (options.verbose) {
        std::wcout << L"--- Running ---\n" << std::flush;
    }

    int exit_code = run_program_and_get_exit_code(run_command.str());
    
    if (options.verbose) {
        std::wcout << L"\n--- Finished ---\n" << L"Program exited with code " << exit_code << L".\n";
    }

    LocalFree(argv);
    return exit_code;
}