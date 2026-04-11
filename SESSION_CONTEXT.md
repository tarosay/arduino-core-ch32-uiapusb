# uiapusb 開発セッション引き継ぎ資料

作成日: 2026-04-11

---

## プロジェクト概要

- **ボード**: Pro Micro CH32V003（QingKe V2 RISC-V）
- **リポジトリ**: `D:\git\github\arduino-core-ch32-uiapusb`
- **Arduino15インストール先**: `C:\Users\minao\AppData\Local\Arduino15\packages\uiapusb\hardware\ch32v\0.0.4\cores\arduino\`
- **WebLink USB**: `D:\git\github\WebLink_USB`（ブラウザ経由 HID 通信ツール）

### 基本技術構成

| 技術 | 内容 |
|---|---|
| `rv003usb` | GPIO bit-bang USB Low-Speed HID（CH32V003用） |
| `ch32fun` | WCH公式HALではなくch32fun使用。`pre_init()`/`systick_init()`は使わない |
| `SysTick` | `CTLR=1`（free-running、割り込みなし）→ rv003usb と共存可能 |
| `DelaySysTick` | ch32fun のポーリングベース遅延。USB タイミングを壊さない |
| DMDATA0/1 | RISC-V デバッグレジスタ経由のデータ転送 |
| `RV003USB_USB_TERMINAL=1` | USB HID Feature Report 0xFD ↔ DMDATA0 ブリッジ |

---

## 重要な制約

- `systick_init()` を呼ぶと `SysTick CTLR=0xF`（auto-reload+割り込み）になり **USB Code43** 発生 → 絶対に呼ばない
- `pre_init()` も USB と共存不可 → 使わない
- グローバル割り込み（mstatus MIE bit3）: ch32fun スタートアップでは **0x1880** に設定 → MIE=0（無効）
- ヒープは **256バイト**（new_delete.cpp）。`new HardwareTimer()` でヒープ枯渇 → `while(1){}` ハング

---

## ファイル構成（現時点）

### リポジトリ側（D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\）

#### `uiapusb.h`（リポジトリ最新版）
```c
void uiap_core_begin(void);   // core初期化のみ（USB起動なし）
void uiapusb_begin(void);     // USB/HID初期化（HIDuiap.begin()から呼ばれる）
int  uiapusb_available(void);
int  uiapusb_read(uint8_t *buf, int maxlen);
int  uiapusb_write(const uint8_t *buf, int len);
```

#### `uiapusb.c`（リポジトリ最新版・**作業中**）
- `g_uiap_core_inited` / `g_uiap_usb_inited` の二段階フラグ管理
- `uiap_core_begin()`: `SystemInit()` + `SysTick->CNT = 0` のみ
- `uiapusb_begin()`: USB呼び出し（`usb_setup()`等）が**全部コメントアウト中**
- `uiapusb_available()` / `uiapusb_read()` / `uiapusb_write()`: フラグ未初期化時は 0 を返す

```c
// uiapusb_begin() の現状（作業中）
void uiapusb_begin(void) {
    if (g_uiap_usb_inited) return;
    //uiap_core_begin();  // コメントアウト
    //Delay_Ms(1);        // コメントアウト
    //usb_setup();        // コメントアウト ← ★ USB起動なし
    g_uiap_usb_inited = 1;
}
```

#### `main.cpp`（リポジトリ最新版）
```cpp
extern "C" int main(void) {
    uiap_core_begin();   // core初期化（delay/millis/micros を有効化）
    setup();
    while (1) { loop(); }
}
```

#### `HIDuiap.h`（リポジトリ・Arduino15 共通）
```cpp
class HIDuiapClass {
public:
    void begin(void)                        { uiapusb_begin(); }
    int  available(void)                    { return uiapusb_available(); }
    int  read(uint8_t *buf, int maxlen)     { return uiapusb_read(buf, maxlen); }
    int  write(const uint8_t *buf, int len) { return uiapusb_write(buf, len); }
};
extern HIDuiapClass HIDuiap;
```

#### `wiring_time.c`（リポジトリ・Arduino15 共通）
- `SysTick->CNT` を free-running カウンタとして使用（割り込みなし）
- `uiapusb_timebase_ticks()` で 64bit 累積
- `millis()` / `micros()` / `delay()` を実装
- `delay()` は `DelaySysTick(ms * 48000)` を使用

#### `Tone.cpp`（リポジトリ・Arduino15 共通・修正済み）
- HardwareTimer / TIM2割り込みを**使わない**ソフトウェア実装
- `DelaySysTick()` でピンをトグルして矩形波生成（ブロッキング方式）
- 旧実装のハング原因: `new HardwareTimer()` → ヒープ256バイト枯渇 → `while(1){}`

---

### Arduino15側（0.0.4）とリポジトリの差異

| ファイル | Arduino15(0.0.4) | リポジトリ |
|---|---|---|
| `uiapusb.h` | 旧版（`uiap_core_begin`なし） | 新版（`uiap_core_begin`あり） |
| `uiapusb.c` | 旧版（`uiapusb_begin`にUSB初期化あり） | 新版（作業中・USB呼び出しコメントアウト） |
| `main.cpp` | `setup()`のみ（`uiap_core_begin`呼ばない） | `uiap_core_begin()`→`setup()` |
| `HIDuiap.h` | あり | あり |
| `HIDuiap.cpp` | あり | あり |
| `Arduino.h` | `#include "HIDuiap.h"` あり | `#include "HIDuiap.h"` あり |
| `wiring_time.c` | 同一 | 同一 |
| `Tone.cpp` | 修正済み（software実装） | 修正済み（software実装） |

---

## ★ 現在調査中の問題

### 「USB calls コメントアウト中なのに USB が認識される」

**現象**: リポジトリの `uiapusb.c` では `usb_setup()` がコメントアウトされているにもかかわらず、`HIDuiap.begin()` を呼ぶと USB デバイスとして認識される。

**調査が必要な理由の候補**:

1. **Arduino IDE は Arduino15(0.0.4) をコンパイルしている**  
   Arduino15の `uiapusb.c` は旧版のため、`uiapusb_begin()` の中に `SystemInit()` + `usb_setup()` が残っている。  
   → .ino で `HIDuiap.begin()` を呼ぶ → `uiapusb_begin()` → 旧版コードが動く  
   **これが最有力仮説**

2. **ch32fun スタートアップが `usb_setup()` を呼んでいる**  
   `rv003usb.c` の初期化がどこかで自動実行されている可能性

3. **`SystemInit()` 自体がUSBに関係している**  
   `uiap_core_begin()` が呼ぶ `SystemInit()` の中身に USB 関連処理が含まれる可能性

**次のステップ（調査手順案）**:
- Arduino15の `uiapusb.c` を確認し、旧版か新版かを明確にする
- Arduino IDE のビルドキャッシュをクリアして再コンパイルし直す
- `usb_setup()` の呼び出し元を全検索する

---

## 解決済み問題

| 問題 | 原因 | 対策 |
|---|---|---|
| `uiapusb_read()` が常に 0 を返す | `poll_input()` が呼ばれていなかった | `uiapusb_available()` と `uiapusb_read()` に `poll_input()` を追加 |
| `delay()` が無限ループ | `systick_init()` で SysTick auto-reload → USB Code43 | `DelaySysTick()` を使う実装に変更 |
| `tone()` がハング | `new HardwareTimer()` でヒープ(256B)枯渇 → `while(1){}` | ソフトウェアbit-bang実装に全面置き換え |
| USB opt-in | `main.cpp` が無条件で `uiapusb_begin()` 呼び出し | `main.cpp` から削除。`.ino` で `HIDuiap.begin()` を呼ぶ方式に |
| クラスAPIの衝突 | `UIAPusb.h` と `uiapusb.h` がWindows上で同一ファイル | C++クラスを `HIDuiap.h`/`HIDuiap.cpp` にリネーム |
| LTO最適化による消滅 | `-flto` で不要とみなされたコードが削除される | `volatile` 修飾で回避 |

---

## WebLink USB (D:\git\github\WebLink_USB)

- `src/js/app/terminal.js`: `receiveFeatureReport(0xFD)` を 10ms ごとにポーリング
- HID Feature Report フォーマット: 7バイト、`arr[0] & 0x80` でデータ有無、最大 **6バイト**/転送
- ローカルエコー: 送った文字は web 側で折り返し表示される（仕様）

---

## 参照ファイルパス一覧

```
リポジトリ:
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\uiapusb.h
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\uiapusb.c
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\HIDuiap.h
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\HIDuiap.cpp
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\main.cpp
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\Arduino.h
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\wiring_time.c
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\wiring_time.h
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\Tone.cpp
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\usb_config.h
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\ch32fun.c
  D:\git\github\arduino-core-ch32-uiapusb\cores\arduino\rv003usb.c
  D:\git\github\arduino-core-ch32-uiapusb\variants\CH32V00x\CH32V003F4\variant_CH32V003F4.h

Arduino15 (0.0.4):
  C:\Users\minao\AppData\Local\Arduino15\packages\uiapusb\hardware\ch32v\0.0.4\cores\arduino\
  （上記と同名ファイルが存在。一部は旧版のまま）
```
