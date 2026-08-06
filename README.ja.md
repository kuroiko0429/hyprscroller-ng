# hyprscroller-ng

[Hyprland](https://hyprland.org/) 向けのスクロール（カラムベース）レイアウトプラグイン。

[hyprscroller](https://github.com/dawsers/hyprscroller)（MIT）に着想を得て、upstream の更新が止まっていたため一から作り直しました。Hyprland の API 調査と初期実装には AI を活用しています。追加機能もいくつか実装済みです。

> **開発中です。** コア機能は安定しており、日常使いしています。

---

## 機能

- カラムベースのスクロールレイアウト（PaperWM / Scroller 風）
- カラムを画面左右端にピン留め
- カラムの折りたたみ / 展開トグル
- Zen モード（フォーカス中のカラムのみ表示）
- フォーカス履歴（前後への移動）
- ウィンドウクラスごとの自動カラム幅ルール
- カラム単位でのワークスペース移動
- プリセット幅の巡回
- 全カラムのフィット操作

---

## インストール

### hyprpm（推奨）

```bash
hyprpm add https://github.com/kuroiko0429/hyprscroller-ng
hyprpm enable hyprscrolling
```

`hyprland.conf` に追加:

```
general {
    layout = hyprscrolling
}
```

### ソースからビルド

**必要なもの:** `hyprland`, `libdrm`, `libinput`, `libudev`, `pangocairo`, `pixman-1`, `wayland-server`, `xkbcommon`

```bash
git clone https://github.com/kuroiko0429/hyprscroller-ng
cd hyprscroller-ng
make
```

`hyprscrolling.so` がプロジェクトディレクトリに生成されます。

### プラグインのロード

`hyprland.conf` に追加:

```
plugin = /path/to/hyprscrolling.so
general {
    layout = hyprscrolling
}
```

---

## 設定

すべてのオプションは `plugin { hyprscrolling { ... } }` の中に記述します。

| オプション | 型 | デフォルト | 説明 |
|---|---|---|---|
| `column_width` | float (0–1) | `0.5` | デフォルトのカラム幅（モニター幅に対する割合） |
| `fullscreen_on_one_column` | bool | `false` | カラムが1つだけの場合にフルスクリーンにする |
| `focus_fit_method` | int (0 or 1) | `0` | フォーカス時の表示方法: `0` = 中央揃え, `1` = 画面内に収める |
| `follow_focus` | bool | `true` | フォーカスしたカラムが見えるように自動スクロール |
| `follow_debounce_ms` | int | `0` | follow_focus のデバウンス時間（ms） |
| `explicit_column_widths` | string | `0.333, 0.5, 0.667, 1.0` | `+conf` / `-conf` で巡回するプリセット幅（カンマ区切り） |
| `collapsed_width` | int (px) | `30` | 折りたたみ時のカラム幅（ピクセル） |
| `focus_history` | bool | `true` | フォーカス履歴の有効/無効 |
| `auto_width_rules` | string | `` | ウィンドウクラスごとの幅: `firefox:0.7, kitty:0.3` |

### 設定例

```
plugin {
    hyprscrolling {
        column_width = 0.5
        fullscreen_on_one_column = false
        focus_fit_method = 0
        follow_focus = true
        follow_debounce_ms = 0
        explicit_column_widths = 0.333, 0.5, 0.667, 1.0
        collapsed_width = 30
        focus_history = true
        auto_width_rules = firefox:0.7, kitty:0.3, code-oss:0.6
    }
}
```

---

## レイアウトメッセージ

`layoutmsg` ディスパッチャーでコマンドを送ります:

```
bind = SUPER, key, layoutmsg, <メッセージ>
```

### フォーカス

| メッセージ | 説明 |
|---|---|
| `focus l/r/u/d` | 指定方向にフォーカスを移動。端に達すると折り返す（隣のモニターには移動しない） |
| `focusback` | フォーカス履歴を一つ戻る |
| `focusfwd` | フォーカス履歴を一つ進む |

### ウィンドウ移動

| メッセージ | 説明 |
|---|---|
| `movewindowto l/r/u/d` | ウィンドウを隣のカラム/スタックに移動。右端で右に移動すると新しいカラムに昇格 |
| `promote` | ウィンドウを独立した新しいカラムに昇格 |

### カラム操作

| メッセージ | パラメータ | 説明 |
|---|---|---|
| `swapcol l/r` | `l` または `r` | 現在のカラムを左右の隣と入れ替え。端で折り返す |
| `colresize` | `0.5`, `+0.2`, `-0.2`, `+conf`, `-conf`, `all 0.5` | カラム幅を変更（全カラム一括も可） |
| `movecoltoworkspace` | `1`, `+1`, `-1`, `special` など | カラム全体を指定ワークスペースに移動 |
| `togglecollapse` | — | カラムを折りたたむ / 展開する |

### スクロール

| メッセージ | パラメータ | 説明 |
|---|---|---|
| `move` | `+col`, `-col`, `+200`, `-200` | レイアウトをカラム単位またはピクセル単位で横スクロール |

### フィット操作

| メッセージ | パラメータ | 説明 |
|---|---|---|
| `fit` | `active`, `visible`, `all`, `toend`, `tobeg` | カラムを画面に合わせてリサイズ/整列 |
| `togglefit` | — | `focus_fit_method` を center (0) と fit (1) で切り替え |

### 表示モード

| メッセージ | 説明 |
|---|---|
| `zen` | フォーカス中のカラムのみ表示（集中モード）。もう一度で解除 |
| `pin left` | カラムを画面左端に固定（スクロールしても動かない） |
| `pin right` | カラムを画面右端に固定 |
| `unpin` | ピン留めを解除 |

---

## キーバインド例

[`scrolling.conf`](./scrolling.conf) をそのまま `source =` で読み込むか、以下を参考に設定してください:

```
# フォーカス移動
bind = SUPER, H, layoutmsg, focus l
bind = SUPER, L, layoutmsg, focus r
bind = SUPER, K, layoutmsg, focus u
bind = SUPER, J, layoutmsg, focus d

# ウィンドウ移動
bind = SUPER SHIFT, H, layoutmsg, movewindowto l
bind = SUPER SHIFT, L, layoutmsg, movewindowto r
bind = SUPER SHIFT, K, layoutmsg, movewindowto u
bind = SUPER SHIFT, J, layoutmsg, movewindowto d
bind = SUPER, P, layoutmsg, promote

# カラム入れ替え
bind = SUPER ALT, H, layoutmsg, swapcol l
bind = SUPER ALT, L, layoutmsg, swapcol r

# カラム幅変更
bind = SUPER, equal, layoutmsg, colresize +0.05
bind = SUPER, minus, layoutmsg, colresize -0.05
bind = SUPER, bracketright, layoutmsg, colresize +conf
bind = SUPER, bracketleft, layoutmsg, colresize -conf
bind = SUPER SHIFT, equal, layoutmsg, colresize all 0.5

# スクロール
bind = SUPER, period, layoutmsg, move +col
bind = SUPER, comma, layoutmsg, move -col
bind = SUPER SHIFT, period, layoutmsg, move +200
bind = SUPER SHIFT, comma, layoutmsg, move -200

# フィット
bind = SUPER, F, layoutmsg, fit active
bind = SUPER SHIFT, F, layoutmsg, fit visible
bind = SUPER CTRL, F, layoutmsg, fit all
bind = SUPER, T, layoutmsg, togglefit

# ピン留め
bind = SUPER CTRL, bracketleft, layoutmsg, pin left
bind = SUPER CTRL, bracketright, layoutmsg, pin right
bind = SUPER CTRL, backslash, layoutmsg, unpin

# 折りたたみ
bind = SUPER, C, layoutmsg, togglecollapse

# Zen モード
bind = SUPER, Z, layoutmsg, zen

# フォーカス履歴
bind = SUPER ALT, bracketleft, layoutmsg, focusback
bind = SUPER ALT, bracketright, layoutmsg, focusfwd

# ワークスペース移動
bind = SUPER CTRL SHIFT, 1, layoutmsg, movecoltoworkspace 1
bind = SUPER CTRL SHIFT, 2, layoutmsg, movecoltoworkspace 2
bind = SUPER CTRL SHIFT, 3, layoutmsg, movecoltoworkspace 3
bind = SUPER CTRL SHIFT, right, layoutmsg, movecoltoworkspace +1
bind = SUPER CTRL SHIFT, left, layoutmsg, movecoltoworkspace -1
bind = SUPER CTRL SHIFT, S, layoutmsg, movecoltoworkspace special
```

---

## ビルドシステム

3種類のビルドシステムに対応しています:

| ファイル | コマンド |
|---|---|
| `Makefile` | `make` |
| `CMakeLists.txt` | `cmake -B build && cmake --build build` |
| `meson.build` | `meson setup build && ninja -C build` |

---

## ライセンス

MIT
