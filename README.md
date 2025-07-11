# crun

`crun`は、**C/C++ソースファイルを指定するだけで自動的にコンパイル＆実行**できる、Windows用のシンプルなコマンドラインツールです。  
Pythonのような手軽さでC/C++プログラムを試したい方に最適です。

---

## 特徴

- **複数ソースファイル対応**: `.c`または`.cpp`ファイルを複数指定して、まとめてビルド＆実行できます。
- **インクルード内容を解析し、必要なコンパイラオプションを自動追加**
- **ヘッダに応じたライブラリの自動リンク**: `windows.h`, `pthread.h`などのヘッダに応じて、必要なライブラリを自動的にリンクします。
- **実行ファイルサイズを最小化する最適化オプションを自動適用**
- **警告オプションのサポート**: `--wall`オプションでコンパイラの警告をすべて有効化できます。
- **デバッグビルドのサポート**: `--debug`または`-g`オプションでデバッグビルドを有効化できます。
- **詳細出力**: `--verbose`または`-v`オプションでコンパイルコマンドなどの詳細な出力を表示できます。
- MinGW (gcc/g++) または Clang を利用（PATHが通っている必要あり）
- 一時ディレクトリでビルドし、実行後に自動削除（`--keep-temp`で保持可能）
- 追加のコンパイルオプションや詳細出力（`--verbose`）もサポート
- プログラム引数の指定も可能
- 標準入出力・エラーはそのまま親プロセスに引き継がれます

---

## インストール

1. **コンパイラをインストール**: [MinGW-w64](https://www.mingw-w64.org/) または [Clang](https://clang.llvm.org/) をインストールしてください。
    - インストール時、「gcc.exe」「g++.exe」または「clang.exe」「clang++.exe」がインストールされていることを確認してください。
2. **PATH環境変数を設定**: MinGWまたはClangの`bin`ディレクトリ（例: `C:\mingw-w64\bin` や `C:\Program Files\LLVM\bin`）を**PATH環境変数**に追加してください。
    - コマンドプロンプトで `gcc --version` や `clang --version` が動作すればOKです。
3. `crun.exe`は、`crun_clang.exe`と`crun_gcc.exe`の両方がビルドされ、`crun.exe`は`crun_clang.exe`のコピーとして提供されます。`crun.exe`を任意のディレクトリに配置し、そのディレクトリもPATHに追加すると便利です。

---

## 使い方

```sh
crun <ソースファイル1> [ソースファイル2...] [プログラム引数...] [オプション...]
crun --clean
```

- `<ソースファイル...>`: 1つ以上の`.c`または`.cpp`ファイルを指定
- `[プログラム引数...]`: 実行するプログラムに渡す引数（任意）
- `[オプション...]`: crunの動作を制御するオプション（後述）

### 例

```sh
# 単一ファイル
crun hello.c
crun --compiler clang app.cpp arg1 arg2 --verbose

# 複数ファイル
crun main.c utils.c -o my_app
crun game.cpp player.cpp enemy.cpp --debug

# ライブラリの自動リンク
crun math_app.c             # math.h をインクルードしていれば -lm を自動追加
crun network_app.c          # winsock2.h をインクルードしていれば -lws2_32 を自動追加

# その他のオプション
crun custom.c --cflags "-O3 -DNDEBUG"
crun test.cpp --keep-temp
crun benchmark.cpp --time
crun --clean
```

---

## オプション一覧

| オプション                | 説明                                    |
|--------------------------|-----------------------------------------|
| `--help`                 | ヘルプを表示                            |
| `--version`              | バージョン情報を表示 (v0.7.0)           |
| `--compiler <name>`      | 使用するコンパイラを指定します (`gcc` or `clang`)。デフォルトは `gcc` です。 |
| `--cflags "<flags>"`     | 追加のコンパイルフラグを指定。自動検出されたフラグを上書きします。 |
| `--keep-temp`            | 実行後も一時ディレクトリを削除しない     |
| `--verbose`, `-v`        | 詳細な出力を有効化                       |
| `--time`                 | プログラムの実行時間を計測・表示         |
| `--wall`                 | コンパイラの警告をすべて有効化 (`-Wall`)   |
| `--debug`, `-g`          | デバッグビルドを有効化 (`-g`)            |
| `--clean`                | カレントディレクトリの一時ディレクトリをすべて削除 |

- オプションは**どの位置でも指定可能**です（例: `crun --verbose hello.c` もOK）。
- `--cflags`の直後にフラグ文字列を指定してください（例: `--cflags "-Wall -O2"`）。

---

## 自動コンパイルオプション

`crun`は、コンパイル時に以下のオプションを自動的に適用します。

- **基本最適化**: `-O2 -s` (実行ファイルのサイズと速度を両立)
- **デバッグビルド**: `--debug` 指定時は `-g`
- **ライブラリ自動リンク**: ソースコードが特定のヘッダファイル（例: `pthread.h`, `math.h`, `windows.h`, `winsock2.h`）をインクルードしている場合、対応するライブラリリンクオプション（例: `-lpthread`, `-lm`, `-lkernel32`, `-lws2_32`）を自動的に追加します。

これらの自動オプションは、`--cflags`オプションで上書きすることが可能です。

---

## 動作の流れ

1. ソースファイルの存在と拡張子（`.c`/`.cpp`）をチェック
2. 一時ディレクトリを作成し、そこにビルド
3. **ソースファイルのインクルード内容を解析し、必要なコンパイラオプションを自動決定**
4. MinGWの`gcc.exe`/`g++.exe`またはClangの`clang.exe`/`clang++.exe`でコンパイル
5. 実行ファイルを生成し、指定した引数で実行
6. 終了後、一時ディレクトリを自動削除（`--keep-temp`指定時は保持）

---

## 注意事項

- **対応ファイル**: `.c`または`.cpp`のみ対応しています。
- **コンパイラ必須**: MinGW (gcc/g++) または Clang (clang/clang++) のインストールとPATH設定が必要です。
- **管理者権限不要**: 通常のユーザー権限で動作します。
- **一時ディレクトリ**: 最初のソースファイルと同じディレクトリ内に`crun_tmp_...`という一時ディレクトリを作成します。
- **エラー時の挙動**: コンパイルエラーや実行エラー時はエラーメッセージを表示し、終了コード1で終了します。`Ctrl+C` などで中断された場合も、一時ディレクトリは自動的にクリーンアップされます。
- **日本語ファイル名**: 日本語やスペースを含むパスにも対応しています。

---

## よくある質問

### Q. コンパイラが見つからないと言われる

A. `gcc.exe`/`g++.exe`または`clang.exe`/`clang++.exe`がPATHに含まれているか確認してください。コマンドプロンプトで `gcc --version` や `clang --version` が動作すればOKです。それでも見つからない場合は、crunを実行しているターミナルを再起動してみてください。

### Q. 一時ディレクトリが消えない

A. `--keep-temp`オプションを指定した場合や、ごく稀な異常終了時に残ることがあります。不要な場合は `crun --clean` コマンドでカレントディレクトリのものを削除できます。

### Q. どのコンパイラが使われますか？

A. デフォルトでは、`.c`ファイルのみの場合は`gcc.exe`、`.cpp`ファイルが1つでも含まれる場合は`g++.exe`が自動的に選択されます。`--compiler clang` オプションを指定すると、`clang.exe` と `clang++.exe` が使用されます。

### Q. 標準入力や標準出力はどうなりますか？

A. crun自身の標準入出力・エラーは、実行したプログラムにそのまま引き継がれます。

---

## ライセンス

MITライセンス

---