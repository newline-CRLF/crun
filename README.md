# crun

`crun`は、Pythonのように手軽にC/C++プログラムを実行できるWindows用ツールです。  
ソースファイルを指定するだけで、自動的にコンパイル・実行します。

## 特徴

- C/C++ソースファイル（.c, .cpp）を指定するだけでOK
- MinGW（gcc/g++）を自動検出して利用
- 一時ディレクトリでビルドし、実行後に自動削除（`--keep-temp`で保持可能）
- 追加のコンパイルフラグも指定可能

## インストール

1. MinGW（gcc/g++）をインストールし、PATHを通してください。
2. `crun.exe`を任意の場所に配置してください。

## 使い方

```sh
crun <ソースファイル> [プログラム引数...] [オプション...]
```

### 例

```sh
crun hello.c
crun test.cpp arg1 arg2
crun math.c --cflags "-O2 -lm"
```

### オプション

- `--help` : ヘルプを表示
- `--version` : バージョン情報を表示
- `--cflags "<flags>"` : 追加のコンパイルフラグを指定
- `--keep-temp` : 一時ディレクトリを削除せずに残す

## ライセンス

MIT License
