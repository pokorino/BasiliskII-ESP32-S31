# BasiliskII for ESP32-S31-Korvo-1

**ESP32-S31 Emulator Preview**  
Developed by pokorino LABO

公開初版はソースコードのプレビューです。PC用画面ビューアーと検証用Flash起動構成は含みません。

Experimental **interpreter-based (non-JIT)** port of BasiliskII to the
ESP32-S31-Korvo-1 with an 800×480 RGB LCD, 16 MiB flash and 16 MiB PSRAM.

**Physical hardware operation has not been verified.** This repository contains
source code, not a claim of a completed or stable hardware product.

## 実装した機能

- UAE 68040インタープリターとソフトウェアFPU。
- 800×480表示、1/2/4/8-bitゲスト画面の変換。
- microSD上のMacintosh ROM・ディスクイメージ読み込み。
- GT1151タッチ、画面キーボード。
- USB HIDキーボード・マウス、ハブ経由の接続処理。
- ES8389音声出力処理。

タッチ・USB・音声は実装済みですが、S31実機での確認は未完了です。
Wi-Fi、マイク、ExtFS、設定GUIはこのS31構成では接続していません。
JIT実験版とLinuxプロジェクトは含みません。

## 検証状況

ローカルの別検証構成では、esp-emu v0.50.0上で68040 CPUテスト25項目に成功し、
ROM/OSを内蔵Flashに配置した構成でSystem 7.0のFinder表示とキー入力を確認しました。
それはエミュレーター上の別構成による結果です。この公開構成のmicroSD・LCD・USB・
タッチ・音声が実機で動くことを証明したものではありません。

この公開パッケージの既定構成は **microSD・非JIT** です。
個人のROM、Mac OSイメージ、Flash起動用イメージ、検証ログは配布しません。

## ビルド

ESP32-S31対応の **ESP-IDF 6.1** と、そのRISC-Vツールチェーンを用意し、
ESP-IDFの環境を有効にして実行してください。

```sh
idf.py set-target esp32s31
idf.py build
idf.py -p YOUR_SERIAL_PORT flash monitor
```

Component Managerが `main/idf_component.yml` の依存関係を取得します。
主な固定依存は Korvo-1 BSP 1.0.1 と Arduino-ESP32 4.0.0-rc1 です。
開発版SDK・コンポーネントを使用しているため、別リビジョンでのビルドは未検証です。

## ROMとディスク

権利を確認したROMとOSディスクを利用者自身で用意し、microSDルートへ配置します。

| ファイル名 | 用途 |
|---|---|
| `Q650.ROM` | Macintosh ROM |
| `Macintosh8.dsk` | 起動ディスク（読み書き） |

Mac RAMの既定値は8 MiB、MacプロファイルはQuadra 900です。
ROM・OSとの互換性は利用者の構成で確認が必要です。
ディスクのファイルオフセットは32-bitのため、2 GiB未満を想定しています。
重要なディスクのコピーを作って試してください。

アプリの配置は `0x10000`。書き込みにはブートローダーとパーティションも必要なため、
上記の `idf.py flash` を使用してください。

## ライセンス・移植元

BasiliskII/UAEのGPL-2.0-or-laterの通知と各ファイルの著作権表示を保持しています。
ライセンス本文は [LICENSE](LICENSE)、移植元は [THIRD_PARTY.md](THIRD_PARTY.md) を参照。
ESP-IDF・Arduino・BSP等の依存コンポーネントには、それぞれのライセンスが適用されます。
