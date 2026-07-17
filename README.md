# camdrv2: Linux 用 CAMAC デバイスドライバ

- Modernized version of camdrv: https://www.awa.tohoku.ac.jp/~sanshiro/kinoko/camdrv
- Tested for Linux kernel version 6
- Currently only supports Hoshin CCP-USB(v2)

Linux 2/3 用に書かれた CAMAC デバイスドライバ camdrv を新しい CCP-USB(v2) 用に書き換えたものです．
あわせて，最近のカーネル (Linux 6 以降）で使えるようにしました．

使い方は [camdrv のページ](https://www.awa.tohoku.ac.jp/~sanshiro/kinoko/camdrv) を参照してください．テストプログラムは `test` ディレクトリに移しました．本体のライブラリをコンパイル後，`cd test` して `make` してください．

**ドライバのコンパイル**
```bash
cd CCPUSBv2
make
sudo make install
cd ..
```

**ライブラリのコンパイル**
```bash
make
```

**テストプログラムのコンパイル**
```bash
cd test
make
```

Python 版の camlib も作ってみました．バインディングではないので C++ の camlib には依存していませんが，ドライバのコンパイルとインストールは必要です．
