[![License: MIT](https://img.shields.io/badge/License-MIT-A31F34.svg)](https://github.com/Virtual-World-RE/O3RE/blob/master/LICENSE)
[![OS: Windows](https://img.shields.io/badge/OS-Windows-97CA00.svg)](https://img.shields.io/badge/OS-Windows-97CA00.svg)
[![Discord: Virtual World](https://img.shields.io/discord/322510885799198722.svg?label=Virtual%20World&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/UTVkc98p7P)
[![SemVer: 2.0.0](https://img.shields.io/badge/SemVer-2.0.0-3F4551.svg)](https://github.com/Virtual-World-RE/O3RE/blob/master/SEMVER.md)
[![Keep a Changelog: 1.1.0](https://img.shields.io/badge/Keep%20a%20changelog-1.1.0-f25d30.svg)](https://github.com/Virtual-World-RE/O3RE/blob/master/CHANGELOG.md)

# Onimusha 3 Launcher
Open Source Fixes for Onimusha 3: Demon Siege.
> May become a full reverse of the game one day ?

---

## このフォークについて（siratamalkuhj/Oni3-Launcher）

[Virtual-World-RE/Oni3-Launcher](https://github.com/Virtual-World-RE/Oni3-Launcher) のフォークです。`OniPatch.dll` に次の機能を追加しています。

- **Xbox コントローラー（XInput）対応**
- **オン・オフできるチート**（無敵、一閃、自動連鎖など）と、画面左上への表示

元の機能（ウィンドウを 1920×1080 にする）はそのまま使えます。

### 対象のゲーム

- 鬼武者3 PC（ソースネクスト版）、公式パッチ適用後の `oni3.exe` バージョン 1.1.0.0 で動作を確認しています。
- チートは実行ファイルの決まった場所を書き換えるため、ほかのバージョンでは動きません。書き換える前に元の命令を照合し、違っていればそのチートだけ無効になります。

### 使い方

1. ビルドした `OniLauncher.exe` と `OniPatch.dll`（`Dist\Release\Win32\` にできます）を、ゲームのフォルダ（例：`C:\Program Files (x86)\CAPCOM\ONIMUSHA3 PC\`）にコピーします。Program Files に書き込むので、管理者の権限が必要です。
2. **Xbox コントローラーをつないでから**、`OniLauncher.exe` を起動して「Play now」を押します。コントローラーは起動時につながっているときだけ使えるようになります。
3. `OniPatch.dll` を置き換えるときは、先にゲームを終了してください（起動中はファイルが使用中で置き換えられません）。

### コントローラーの割り当て

| Xbox の操作 | ゲームの操作 |
|---|---|
| 左スティック | 移動 |
| 右スティック | 構えの方向変更 |
| 十字キー | 十字キー |
| A | 魂の吸収／取り消し |
| B | 調べる／決定 |
| X | 攻撃 |
| Y | 戦術殻の使用 |
| LB | 防御 |
| RB | 構え |
| RT | 180度反転 |
| R3（右スティック押し込み） | 鬼武者変身 |
| L3（左スティック押し込み） | 現在位置表示 |
| Back | 一時停止 |
| Start | 管理画面 |
| LT | チート切り替え用（ゲームには渡しません） |

- 前走り・バックステップ・左右回転は、キーボードで操作してください。
- A／B の役割は PS2 版のボタン位置（×／○）に合わせています。表示は言語設定によって入れ替わります。
- Steam が起動していると、Back＋Start などの組み合わせを Steam が横取りすることがあります。その場合は Steam のコントローラー設定（デスクトップレイアウト）で外してください。

### チート

| キー | コントローラー（LT を押しながら） | チート | 内容 |
|---|---|---|---|
| F1 | 十字キー↑ | 無敵 | プレイヤーへのダメージを 0 にし、体力を最大に保つ |
| F2 | 十字キー→ | 鬼力・魂 無限 | 鬼力の消費を止め、赤魂・紫魂が減らないようにする |
| F3 | 十字キー↓ | アイテム無限 | 回復薬などの個数・矢・弾が減らないようにする |
| F4 | 十字キー← | 一撃必殺 | 敵へのダメージを「残り体力＋1」にする |
| F5 | Y | 常時一閃 | 敵の近くで攻撃すると、タイミングに関係なく本物の一閃（追尾つき）になる。ジャックの銃は撃つたびに会心になる |
| F6 | B | 自動連鎖 | 一閃のあと、ボタンを押さなくても連鎖一閃が続く |
| F7 | X | 移動速度 | 押すたびに オフ → ×2 → ×3 → ×4 → オフ と切り替わる。プレイヤーの水平方向の移動だけが速くなる（敵や高さ方向の動きは変わらない） |

- オンになっているチートは、画面左上に黄色い文字で表示されます。もう一度押すとオフになります。
- アイテム・魂は「減ったら戻す」方式です。セーブデータの読み込みなどで一度に多くの値が変わったときは、新しい値を基準にします。
- 一撃必殺は、一部のボスで不具合が出ることがあるかもしれません（参考にした Cheat Engine の表のメモより）。
- ジャックの銃では、常時一閃は「銃の会心」になります（銃は一閃の判定の前に弾かれる作りのため、射撃の動作番号を会心のものに置き換えています）。自動連鎖は近接の一閃にだけ効きます。

### 仕組み（概要）

- **XInput 対応**：`DirectInput8Create` を横取りし、ゲームのアクションマッピング（`EnumDevicesBySemantics` → `BuildActionMap` → `GetDeviceData`）に、XInput の状態を読む偽のゲームパッドを渡します。本物の Xbox コントローラーの DirectInput デバイス（HID のパスに `IG_` を含むもの、および同じ VID/PID の付属インターフェース）は一覧から隠し、入力が二重にならないようにしています。
  - 注意：`oni3.exe` は `GetDeviceInfo` を、構造体のサイズ（`dwSize`）を設定しないまま呼びます。偽パッドは DirectInput と同じく、正しいサイズのときだけ書き込みます。
- **チート**：ゲームの処理に短い差し替え（`jmp`）を入れます。
  - `0x5DF10E`：ダメージ処理（無敵・一撃必殺）
  - `0x5466B2`：鬼力の加算（減る方向だけ止める）
  - `0x6BB910`：一閃の判定の入口（プレイヤーの `+0x22F4` に 0x40000 を立て、敵側のタイミング判定を飛ばす）
  - `0x6BB221`：銃の射撃の動作番号（0x97 → 銃の会心 0x99）
  - `0x53F5A7` / `0x53F602`：位置の更新（書き込み先がプレイヤーの位置 `+0x90` のとき、移動量の X・Y を選んだ倍率にする。Z は高さ）
  - `0x6BCBCC`：連鎖の受付中の攻撃ボタン判定（押されたことにする）。連鎖の残り回数（`+0x44DB`）は 5 以上に保つ
  - プレイヤーのデータは `0x033F9B20` から始まります（体力 `+0x21E8`、最大体力 `+0x21E4` など）。
- **画面表示**：`IDirect3DDevice9::Present` の直前に、GDI で描いた文字をテクスチャにして重ねています。D3DX は使っていません。

### ビルド

1. Visual Studio 2022 に「C++ によるデスクトップ開発」ワークロードを入れます（Windows SDK 10 に含まれる DirectInput / XInput / Direct3D 9 のヘッダーを使うので、DirectX SDK は不要です）。
2. NuGet パッケージ（Detours、nlohmann.json）を復元してから、32 ビット（x86）でビルドします。ゲームが 32 ビットなので、x64 版は使えません。

```bat
MSBuild "Onimusha 3 RE.sln" -t:restore -p:RestorePackagesConfig=true
MSBuild "Onimusha 3 RE.sln" -p:Configuration=Release -p:Platform=x86
```

### テスト

`Dist\Release\Win32\OniPatchTests.exe` を実行すると、コントローラーの変換処理の自動テストが動きます（コントローラーは不要です）。
`OniPatchTests.exe live` は `OniPatch.dll` を読み込み、ゲームと同じ手順で DirectInput を呼び出して、デバイスの一覧と 15 秒間の入力を表示します。

### クレジット

- 元のランチャー：[Virtual-World-RE/Oni3-Launcher](https://github.com/Virtual-World-RE/Oni3-Launcher)
- OniPatch：[Xavier Monin](https://github.com/xaviermonin/Onimusha3Patch)
- 体力・アイテムなどのアドレスは、作者不明の Cheat Engine の表を参考にし、実行ファイルと照合して使っています。

チートは一人用のゲームを個人で楽しむためのものです。

---

以下は元の README です。


## How to Use

1. Download the latest release [here](https://github.com/Virtual-World-RE/O3RE/releases/latest/).
2. Extract .zip to the root of the game (Default ``C:\Program Files (x86)\CAPCOM\ONIMUSHA3\``) <!-- Or use our smart exe installer -->
3. Execute ``OniLauncher.exe``, configure, and Play !


## External Projects

This project use a modified version of [OniPatch](https://github.com/xaviermonin/Onimusha3Patch/tree/master/src/OniPatch) from [Xavier Monin](https://github.com/xaviermonin)


## How to Build

### Pre-requisites

1. [DirectX DevKit](https://www.microsoft.com/en-us/download/details.aspx?id=6812).
2. [Visual Studio 2022](https://visualstudio.microsoft.com/fr/free-developer-offers/).


## License

This project is under MIT license (See [LICENSE](https://github.com/Virtual-World-RE/O3RE/blob/master/LICENSE))
