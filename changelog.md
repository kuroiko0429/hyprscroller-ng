# Hyprscrolling Plugin Changelog

## 2026-05-11 — Hyprland 0.55.0 対応

### Hyprland 0.55.0 API 変更点まとめ

#### 🔴 破壊的変更（プラグインに直接影響）

##### 1. `addConfigValue()` → `addConfigValueV2()` に移行

旧APIの `addConfigValue()` が `[[deprecated]]` になり、新しい型付きシステムに移行。

```diff
- HyprlandAPI::addConfigValue(PHANDLE, "plugin:foo:bar", Hyprlang::INT{0});
- HyprlandAPI::addConfigValue(PHANDLE, "plugin:foo:baz", Hyprlang::FLOAT{0.5F});
- HyprlandAPI::addConfigValue(PHANDLE, "plugin:foo:qux", Hyprlang::STRING{"hello"});
+ HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::Int>("plugin:foo:bar", "description", 0));
+ HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::Float>("plugin:foo:baz", "description", 0.5F));
+ HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::String>("plugin:foo:qux", "description", std::string{"hello"}));
```

**新しい型クラス一覧** (`Config::Values` namespace):

| 型クラス | 対応する型 | ヘッダ |
|---|---|---|
| `Bool` (`CBoolValue`) | `Config::BOOL` (= `bool`) | `BoolValue.hpp` |
| `Int` (`CIntValue`) | `Config::INTEGER` (= `int64_t`) | `IntValue.hpp` |
| `Float` (`CFloatValue`) | `Config::FLOAT` (= `float`) | `FloatValue.hpp` |
| `String` (`CStringValue`) | `Config::STRING` (= `std::string`) | `StringValue.hpp` |
| `Color` (`CColorValue`) | `CHyprColor` | `ColorValue.hpp` |
| `Vec2` (`CVec2Value`) | `Config::VEC2` | `Vec2Value.hpp` |
| `Gradient` (`CGradientValue`) | - | `GradientValue.hpp` |
| `CssGap` (`CCssGapValue`) | - | `CssGapValue.hpp` |
| `FontWeight` (`CFontWeightValue`) | - | `FontWeightValue.hpp` |

**ポイント:**
- 各型にはコンストラクタで `name`, `description`, `default_value` を渡す
- オプションで `min`/`max` 制約、バリデーション関数なども指定可能
- `#include <hyprland/src/config/values/ConfigValues.hpp>` で全型を一括 include できる

---

##### 2. `recalculate()` シグネチャ変更

`IModeAlgorithm::recalculate()` に `eRecalculateReason` パラメータが追加された。

```diff
- virtual void recalculate();
+ virtual void recalculate(Layout::eRecalculateReason reason = Layout::RECALCULATE_REASON_UNKNOWN);
```

**`eRecalculateReason` の値:**

| 値 | 意味 |
|---|---|
| `RECALCULATE_REASON_UNKNOWN` | 理由不明・未指定 |
| `RECALCULATE_REASON_WORKSPACE_CHANGE` | ワークスペース変更 |
| `RECALCULATE_REASON_SPECIAL_WORKSPACE_TOGGLE` | スペシャルワークスペースのトグル |
| `RECALCULATE_REASON_TOGGLE_FULLSCREEN` | フルスクリーン切り替え |
| `RECALCULATE_REASON_INVALIDATE_MONITOR_GEOMETRIES` | モニターのジオメトリ無効化 |
| `RECALCULATE_REASON_RENDER_MOINTOR` | モニター描画 |

定義元: `<hyprland/src/layout/space/Space.hpp>`

---

##### 3. `layoutMsg()` 返り値変更

`std::expected<void, std::string>` → `Config::ErrorResult` に変更。

```diff
- virtual std::expected<void, std::string> layoutMsg(const std::string_view& sv);
+ virtual Config::ErrorResult              layoutMsg(const std::string_view& sv);
```

**`Config::ErrorResult` の定義:**

```cpp
// <hyprland/src/config/shared/ConfigErrors.hpp>
using ErrorResult = std::expected<void, SConfigError>;

struct SConfigError {
    std::string       message;
    eConfigErrorLevel level = eConfigErrorLevel::ERROR;  // SILENT, INFO, WARNING, ERROR
    eConfigErrorCode  code  = eConfigErrorCode::UNKNOWN; // INVALID_ARGUMENT, NOT_FOUND, etc.
};

// ヘルパー
Config::configError("message", eConfigErrorLevel::ERROR, eConfigErrorCode::UNKNOWN);
```

---

#### 🟡 非推奨化（deprecated）されたAPI

| 旧API | 新API | 備考 |
|---|---|---|
| `addConfigValue()` | `addConfigValueV2()` | 上記参照 |
| `addConfigKeyword()` | V2 | 同様に型付きシステム |
| `getConfigValue()` | `CConfigValue<T>` | 直接アクセス推奨 |
| `registerCallbackDynamic()` | `Event::bus()` | シグナルシステム |
| `unregisterCallback()` | `CHyprSignalListener` | RAII管理 |
| `addLayout()` | `addTiledAlgo()` / `addFloatingAlgo()` | 0.54で既に変更済み |
| `removeLayout()` | `removeAlgo()` | 同上 |
| `addDispatcher()` | `addDispatcherV2()` | `SDispatchResult` 返り値 |
| `getFunctionAddressFromSignature()` | `findFunctionsByName()` | 名前ベース検索 |

---

#### 🟢 新規追加API

| API | 概要 |
|---|---|
| `addConfigValueV2()` | 型安全な設定値の登録 |
| `addLuaFunction()` | Luaランタイムにプラグイン関数を公開 |
| `removeLuaFunction()` | Lua関数の登録解除 |
| Lua Layout API | Lua から直接カスタムレイアウトを定義可能 |

---

#### 🔵 Hyprland 0.55.0 の主要な新機能

- **Lua設定サポート**: `hyprland.conf` に加えて Lua ファイルでの設定に対応
- **Lua Layout API**: Lua からカスタムレイアウトを定義可能（per-workspace, per-monitor）
- **ICCプロファイル**: モニターごとのICCプロファイル読み込み対応
- **組み込みスクロールレイアウトの改善**: `expel`, `consume`, `consume_or_expel`, ラッピングオプション追加
- **新バインドフラグ**: `auto_consuming`
- **新ウィンドウルール**: `confine_pointer`
- **新ディスパッチャ**: `move_into_or_create_group`
- **ライブピンチカーソルズーム** (ジェスチャー)

---

### hyprscrolling プラグインの修正内容

| ファイル | 修正内容 |
|---|---|
| `main.cpp` | `addConfigValue()` → `addConfigValueV2()` に全9項目を移行、`ConfigValues.hpp` の include 追加 |
| `Scrolling.hpp` | `recalculate()` → `recalculate(eRecalculateReason)` に変更、`layoutMsg` 返り値を `Config::ErrorResult` に変更、`ConfigErrors.hpp` の include 追加 |
| `Scrolling.cpp` | 上記2つの関数定義を更新 |

---

## 2026-03-29 — Hyprland 0.54.3 対応

- API変更なし。再ビルドのみで対応。

## 2026-03-17 — 初版作成 (Hyprland 0.54.2 対応)

- hyprscrolling プラグインを Hyprland 0.54.2 の新APIに合わせて全面書き換え
- `IHyprLayout` → `ITiledAlgorithm` / `addTiledAlgo()` への移行
- シグナル処理を `Event::bus()` ベースに変更
- 新機能追加:
  - カラムピン留め (`pin left/right`, `unpin`)
  - `movecoltoworkspace` レイアウトメッセージ
  - カラム折りたたみ (`collapse`, `expand`, `togglecollapse`)
  - フォーカス履歴 (`focusback`, `focusfwd`)
  - 自動カラム幅ルール (`auto_width_rules`)
  - Zenモード (`zen`)
