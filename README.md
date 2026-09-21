# jjysim

JJY（日本標準時電波、40kHz/60kHz）の時刻コードを模擬送信するためのシミュレータです。
PC または Raspberry Pi 上のソフトウェアで JJY のタイムコードを生成し、外付けハードウェア（ATtiny13 で生成したキャリア波、または PCB 基板）を変調することで、ごく近距離向けの JJY 電波を再現します。

## 構成

### `app/` — PC 用アプリケーション（Qt/C++）
- USB シリアル（FTDI 等のビットバングモード対応アダプタ）経由で外部ハードウェアの GPIO ピンを制御し、JJY のタイムコードを送出します。
- [main.cpp](app/main.cpp): 現在時刻から JJY の変調パターン（分・時・日・年・曜日・パリティを含む 60 秒分のフレーム）を生成し、シリアル経由でピンを制御して出力します。
- [serial.h](app/serial.h) / `serial.cpp`: termios ベースのシリアル通信ラッパー。
- ビルドには Qt の `qmake`（[jjysim.pro](app/jjysim.pro)）を使用します。

### `rpi/` — Raspberry Pi 用アプリケーション（C++）
- [bcm2835](https://www.airspayce.com/mikem/bcm2835/) ライブラリを使い、Raspberry Pi の GPIO を直接制御して JJY タイムコードを出力します。
- ロジックは `app/` 版とほぼ同じですが、シリアル経由ではなく GPIO を直接操作する点が異なります。
- `make` でビルドします（[Makefile](rpi/Makefile)）。

### `attiny13-firmware/` — キャリア波生成用ファームウェア（C）
- ATtiny13 上で動作し、40kHz または 60kHz の方形波キャリアを生成します。
- 入力ピンの状態でキャリア周波数の切り替えと、変調（ON/OFF）を行います。
- `make` でビルドし、`make write` で avrdude 経由で書き込みます（[Makefile](attiny13-firmware/Makefile)）。

### `pcb/` — 基板設計データ（KiCad）
- 実機ハードウェアの回路図・基板レイアウト（[jjysim.kicad_sch](pcb/jjysim.kicad_sch) / [jjysim.kicad_pcb](pcb/jjysim.kicad_pcb)）。
- `gerber/` ディレクトリに製造用の Gerber / ドリルデータを収録。

## 仕組みの概要

1. 現在時刻（年月日時分秒）から JJY のタイムコードビット列を生成し、1 秒ごとにマーカー（200ms パルス）、データ 0（800ms パルス）、データ 1（500ms パルス）のいずれかを割り当てます。
2. `app/` 版はこのパルスパターンに従って外部アダプタの GPIO ピンをシリアル経由で ON/OFF します。`rpi/` 版は同様の処理を Raspberry Pi の GPIO で直接行います。
3. GPIO 出力は ATtiny13 ファームウェアへの変調入力となり、ATtiny13 が生成する 40kHz/60kHz キャリア波をこのパルスに合わせて ON/OFF することで、JJY 標準電波と同じ形式の信号を作り出します。
