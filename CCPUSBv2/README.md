# camdrv2 for CCP-USB(V2): Linux 用 CAMAC デバイスドライバ (豊伸電子 CCP-USB(V2))

## 概要

このドライバは，豊伸 CCP-USB(V2)（Vendor ID: 0x24b9, Product ID: 0x0011）を Linux システムで使用するためのカーネルモジュールです．

## 必要な環境

### カーネルバージョン

- **最小要件**: Linux 6.x
- **動作確認済み**: Linux 6.14 (Ubuntu 24.04)

### 必要なパッケージ

- カーネルヘッダーファイル（`/lib/modules/$(uname -r)/build` が存在すること）
- ビルドツール（`make`、`gcc` など）

## ビルド方法

```bash
make
```

これにより，`camdrv.ko` カーネルモジュールが作成されます．

## インストール方法

```bash
sudo make install
```

このコマンドは以下の処理を実行します：

1. モジュールをビルド（まだビルドされていない場合）
2. udevルールを `/etc/udev/rules.d/99-camdrv.rules` にコピー
3. モジュールをカーネルにロード（デバイスが接続されている場合）

**注意**: デバイスが接続されていない場合でも，インストールは成功します．デバイスを接続すると，自動的にモジュールがロードされます．

### udevルールの適用

udevルールを適用するには，以下のいずれかを実行してください：

```bash
# udevルールをリロード
sudo udevadm control --reload-rules
sudo udevadm trigger

# または、システムを再起動
sudo reboot
```

## アンインストール方法

```bash
sudo make uninstall
```

このコマンドは以下の処理を実行します：

1. モジュールをカーネルからアンロード
2. udevルールを削除

## デバイスノード

インストール後、以下のデバイスノードが作成されます：

- `/dev/camdrv0` - マイナー番号0のデバイス
- `/dev/camdrv1` - マイナー番号1のデバイス
- `/dev/camdrv` - 最新のデバイスを指すシンボリックリンク（通常は `/dev/camdrv0`）

すべてのデバイスノードは，udevルールにより `0666` のパーミッションが設定されます．

## 使用方法

### デバイスの確認

デバイスが正しく認識されているか確認するには：

```bash
# モジュールがロードされているか確認
lsmod | grep camdrv

# デバイスノードが存在するか確認
ls -l /dev/camdrv*

# カーネルメッセージを確認
dmesg | grep camdrv
```

## トラブルシューティング

### モジュールがロードされない

```bash
# エラーメッセージを確認
dmesg | tail -20

# モジュールを手動でロード
sudo insmod camdrv.ko
```

### デバイスノードが作成されない

```bash
# udevルールが正しくインストールされているか確認
cat /etc/udev/rules.d/99-camdrv.rules

# udevルールをリロード
sudo udevadm control --reload-rules
sudo udevadm trigger

# デバイスが接続されているか確認
lsusb | grep 24b9
```

### パーミッションエラー

```bash
# デバイスノードのパーミッションを確認
ls -l /dev/camdrv*

# 必要に応じて手動でパーミッションを設定
sudo chmod 666 /dev/camdrv*
```

### ビルドエラー

```bash
# カーネルヘッダーがインストールされているか確認
ls /lib/modules/$(uname -r)/build

# カーネルヘッダーをインストール（ディストリビューションに応じて）
# Ubuntu/Debian:
sudo apt-get install linux-headers-$(uname -r)

# Fedora/RHEL:
sudo dnf install kernel-devel-$(uname -r)
```

## クリーンアップ

ビルドファイルを削除するには：

```bash
make clean
```

## ファイル構成

- `camdrv.c` - メインドライバソースコード
- `camdrv.h` - ヘッダーファイル（ioctl 定義）
- `Makefile` - ビルド設定
- `99-camdrv.rules` - udevルールファイル

## ライセンス

GPL2 (GNU General Public License Version 2)

## 作成者

Sanshiro Enomoto (University of Washington, Seattle)

## 更新履歴

- 2013年10月3日: 初版作成（Linux 3.x 用）
- 2025年11月9日: CCP-USB(V2) 対応，Linux Kernel 6.x 形式に更新
