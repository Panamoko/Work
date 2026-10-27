# UE-Like Engine（就職作品）作業ガイド

Overwatch風FPSを、自作のUEライクなエンジン（C++17 / DirectX11）で作るプロジェクト。
このファイルは、別セッションでも同じ前提で作業するための「ルール」と「現状」のまとめ。

---

## 1. 絶対に守るルール

1. **GitHub へ push / commit しない。** Git の更新はユーザーが自分で行う。
2. **ソースコードを直接編集しない。** 変更は「貼り付け用のコード」か「差し替え用のファイル（作業用フォルダで作って送る）」として提示し、ユーザーが自分で適用する。
   - ファイルを読む・検索する・ビルドして原因を調べる、は行ってよい。
   - 誤って編集した場合は、すぐ元に戻して報告する。
   - 説明用の文書（この CLAUDE.md など）は、頼まれたら直接書いてよい。
3. **`.vcxproj` の設定を変えない。** 新しいファイルは、ユーザーが Visual Studio のソリューションエクスプローラーから追加する。ライブラリが要るときは `#pragma comment(lib, "...")` で済ませる（例：`winmm.lib`）。
4. **新しい `.cpp` のファイル名は、フォルダが違っても重複させない。**
   - obj は1つのフォルダにまとめて出力されるので、同名の `.cpp` があると衝突する（MSB8027 / LNK4042 / LNK2019）。
   - 提案前に `Source/` 全体を検索して確認する。
   - `EngineSkybox`、`EngineShadowMap`、`EngineNavGrid` の `Engine` は、旧コードとの衝突を避けるために付けた名前（旧コードは削除済みだが、名前はそのまま使う）。
5. **ユーザーが付けた名前を使う。** `PlayerComponent`、`OpponentComponent`、`HPComponent` など。勝手に別名にしない。

---

## 2. コードを出すときのルール

- **小さいステップに分ける。** 1ステップごとにビルドと動作確認ができる大きさにする。大きい機能は「〇-1 / 〇-2 …」と分け、最初に全体の分け方を示す。
- **どのファイルの、どこに入れるかを具体的に書く。**
  - 「〇〇の行の直後」「〇〇の閉じ `}` の前」のように、基準になる行を示す。
  - **波かっこの内側か外側か**を必ず明記する（ループの中か外か、ポップアップの中か外か、`namespace uel` の中か、など）。
  - 範囲を差し替える場合は「どこからどこまで」を示し、**消してはいけない行**（直後の `End()` や `Update` 呼び出しなど）に注意を促す。
- **変更が多いファイルは、ファイル全体を出す。** 部分的な指示が3〜4か所を超えるなら全体のほうが安全。全体を出すときは、**元のファイルと同じ文字コード・改行**で作る（3章）。
- **既存のファイルに手を入れる前に、今の中身を読む。** ユーザーが名前や構成を変えていることがある（コミット前の変更もある。`git status` を見る）。
- **最後に確認手順を書く。** 何をして、どうなれば成功かを番号付きで。
- **既知の制限や割り切りも書く。**
- **行を足す指示の見本には、足す行だけを載せる。** 前後の行も見せるときは「追加後はこうなる」と明記し、すでにある行（`ImGui::End();` など）が二重に貼られないようにする。
- **1ファイルは 900 行以内を目安にする。** 超えそうなら、役割ごとに別ファイルへ分ける案を出す（今いちばん長いのは `InspectorPanel.cpp` の約 630 行）。
- 説明は日本語。コード中のコメントも日本語でよい（`.hlsl` / `.hlsli` も、**UTF-8（BOM なし）で保存するなら**日本語コメントでよい。理由は9章）。

### 画面に出す文字（ImGui）
- **エディタの説明・ボタン・メッセージは日本語にする。** 文字列は必ず **`u8"..."`** で書く（Shift-JIS のファイルでも UTF-8 BOM のファイルでも正しく出る）。
- **英語のまま残すもの**
  - **ウィンドウ名**（`"Hierarchy"` `"Inspector"` `"Viewport"` `"Asset Browser"` `"Environment"` `"Animation"` `"Anim Graph"` `"NavGrid"`）。`imgui.ini` のドッキング配置がウィンドウ名で保存されているので、変えると配置が崩れる。`"日本語###ID"` の形でも一度崩れる。
  - ImGui の ID（`"##name"`、`PushID("cmp_player")`、`BeginChild("ag_canvas")`、`OpenPopup("AddComponentPopup")` など）とドラッグ＆ドロップの種類（`"ASSET_GLB"` など）
  - データとして保存・比較する文字列（ノードの型名 `"Clip"` `"BlendSpace1D"` `"StateMachine"`、パラメータの型 `Float / Bool / Trigger`、既定の名前 `"Idle"` `"Hit"` `"Param1"` など）
  - Component 名・型名など、コードと対応する用語（`PlayerComponent`、`Any` など）
- 文字が欄からはみ出すときは、短い言い方にするか、`TextWrapped` にする。フォントが 18px なので、13px 前提で決めた幅（`SetNextItemWidth(66)` など）は足りないことがある。

### 出す前に、作業用フォルダで確かめる
- 今の作業ツリーのファイルに変更を当てた複製を、**リポジトリの外の作業用フォルダ**に作ってビルドする。スクリプトは**絶対パス**で書き、リポジトリには書き込まない。終わったら `git status` でリポジトリが変わっていないことを確かめる。
  - `Source/`・`Shader/`・`Game.vcxproj` を複製し、`.vcxproj` の中の `External\...` を絶対パスに書き換え、DirectXTex の `ProjectReference` を消して、`MSBuild Game.vcxproj /p:SolutionDir=<複製先>\` でビルドする（Debug、必要なら Release も）。新しい `.hlsl` を足すときは、複製の `.vcxproj` にだけ `FxCompile`（`ShaderType` 付き）を足す。
  - 画面を確かめるときは、`Data/Anim`・`Data/Prefabs`・`Data/Model`・`Data/Font`・`Json` と `imgui.ini` も複製して起動し、スクリーンショットを撮る（ウィンドウを 1920×1040 にして `PrintWindow`）。**データは複製し直してから比べる**（古い複製の Prefab が Unlit のままで「影が消えた」と誤認したことがある。Prefab のインスタンスは Scene ではなく Prefab の内容で作り直される）。
  - 確認のための仮のコード（起動時に Entity を選ぶ、90 フレーム後に Play を始める、など）は**複製にだけ**入れ、渡すファイルには入れない。
  - HLSL を変えたときは `fxc` でも検証する。式を変えないつもりの整理なら、前後を `fxc /Fc` で逆アセンブルして、命令の行（コメント行を除く）が一致することを確かめる。
- 「どこまで確かめたか（ビルドだけ／画面で確認／Play 中は未確認、など）」を必ず書く。

### エラー報告を受けたときの進め方
- まず実際のファイルを読み、必要ならビルドして、**本物のエラーとそうでないものを切り分ける**。
- ビルド（読み取り目的。成果物は `bin\x64\Debug\Game.exe`）：
  ```
  "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Game.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:m /clp:ErrorsOnly
  ```
- **無視してよい表示**（コンパイルエラーではない）：
  - 「関数定義が見つかりません」「〇〇があいまいです」→ IntelliSense の古い情報
  - cereal / rapidjson / ImGui の「メンバー変数が初期化されていません (type.6)」、`switch` のフォールスルー (es.78) → 外部ライブラリへのコード分析警告
  - 作業用フォルダでビルドしたときの MSB8029（一時フォルダに出力している、という警告）
- **本物として扱う表示**：「識別子が見つかりませんでした」(C3861) などコンパイラのエラー、「移動元のオブジェクトを使用 (lifetime.1)」（実際のバグを指していたことがある）

### これまで実際に起きたミス（指示を書くときに防ぐ）
| 起きたこと | 原因 | 防ぎ方 |
|---|---|---|
| アサート「Mismatched Begin/End」 | 範囲差し替えで `ImGui::End();` まで消えた | 差し替え範囲の終わりを明示する |
| `ImGui::End();` が2つになった | 「3行追加」の見本に、元からある `End()` まで載せた | 足す行だけを載せる。関数が短いなら関数ごと差し替える |
| Play中に何も動かない・画面が水色1色 | 行を直すとき、下の `c->Update(ctx);` が消えた | 直す行の前後も含めて示す |
| メニュー項目・描画が出ない | `BeginPopup` や `for` の外／中の置き間違い | かっこの内外を明記する |
| 保存されない項目がある | `push_back(std::move(ed))` の後に値を書いた | 登録は必ず最後、と書く |
| 構文エラーが大量に出る | 余分な `}` が1つ残った | 差し替え後のかっこの数を確認する |
| 宣言が見つからない | `.h` への宣言追加が抜けた | `.h` と `.cpp` をセットで示す |
| Inspector の「+ コンポーネントを追加」が途中に挟まった | チェックリストに「`ImGui::End();` より前に足す」と書いていた | 足す場所は「`+ コンポーネントを追加` より前（ScreenUI の後ろ）」（7章） |
| ファイルを分けたら `min` が見つからない | 元のファイルでは `d3d11.h` 経由の Windows の `min` マクロに頼っていた | `(std::min)(a, b)` と書き、`<algorithm>` を include する |
| ドッキングの配置が崩れた | ウィンドウ名を日本語にした | ウィンドウ名は英語のまま |
| 起動時にアサートで止まる | `AddFontFromFileTTF` に存在しないフォントを渡した | 先に `std::filesystem::exists` で確かめる |
| `Illegal character in shader file`・`unexpected #endif`・「型 〇〇 を解決できません」 | VS で `.hlsl` を新規作成して中身を貼り付けたら、UTF-8 BOM 付き（や Shift-JIS）で保存された | `.hlsl` / `.hlsli` は作業用フォルダで UTF-8（BOM なし）で作り、`cp` コマンドでコピーしてもらう（9章） |

---

## 3. 環境・前提

- C++17 / DirectX 11 / Visual Studio 2022 (v143) / x64 / `Game.sln`・`Game.vcxproj`
- 外部ライブラリ（`External/`）：imgui-docking（**1.80 WIP**）、ImGuizmo、tinygltf、cereal、DirectXTex、Tracy
- **文字コードはファイルごとに違う。今の文字コードのまま保つ。**
  - ほとんどは **Shift-JIS（CP932）・CRLF**。ツールで読むと日本語コメントが文字化けして見えるが、壊れてはいない。
  - **UTF-8（BOM 付き）・CRLF** のもの：`Editor/` の `InspectorPanel.cpp` `EditorPanels.cpp` `AssetBrowserPanel.cpp` `ImGuiLayer.cpp` `AnimEditorPanels.cpp` `AnimGraphPanel.cpp` `AnimGraphCanvas.cpp` `AnimGraphStateMachineView.cpp` `AnimGraphPanelInternal.h`、`Serialization/StageSerializer.cpp`、`Animation/AnimBlendSpaceNode.*` `AnimationPose.h`、`Graphics/RenderTexture.h`。
  - 調べ方：先頭3バイトが `EF BB BF` なら UTF-8 BOM。貼り付けて保存するときに VS が「Unicode で保存しますか」と聞くことがある。
  - **`Shader/` の `.hlsl` / `.hlsli` だけは別の決まり：UTF-8（BOM なし）**。BOM 付きは fxc がエラーにし、Shift-JIS は日本語コメントで行が消えることがある（9章）。※ 今は VS で貼り付けた分が Shift-JIS のまま残っている（`Common.hlsli` など。行末が `\` になる行は無いので動いている）。直すときは 9章のコマンドで UTF-8 に変換する。
- 作業ディレクトリ（実行時のカレント）はプロジェクト直下。
  - アセットは `Data/` 以下（`kAssetRoot = "Data"`）。パスは `Data/Model/...` の形。
  - Scene の保存先は `Json/` 以下（`kSceneDir = "Json"`。パスは `SceneFilePath("Stage.json")` で作る）。
  - アニメーショングラフは `Data/Anim/<名前>.animgraph.json`。
- Debug の情報形式は `/Zi`（`/ZI` だと Tracy がエラーになる）。
- **フォント**（`ImGuiLayer.cpp` の `LoadUIFont`）：18px。`BIZ UDPゴシック` → `Meiryo UI` → `MS ゴシック` → `Data/Font/ArialUni.ttf` の順に探し、最初に見つかったものを使う。文字の範囲は ImGui の日本語（ひらがな・カタカナ・よく使う漢字）と記号（矢印・罫線・●★など）。範囲に無い漢字は「?」になる。
- **FPS は 60 に固定**（`EngineConfig.h` の `kTargetFps`。`Engine.cpp` の `FrameLimiter`）。VSync はオフ（`Present(0, 0)`）。重さを測るときは `kTargetFps = 0`（制限なし）にして、**Release で、FPS ではなく ms で比べる**。Debug は ImGui の UI が重く出る（キャラを選ぶと Inspector と Anim Graph の分で約 0.2ms 増えるが、Release では差が無い）。

### ImGui 1.80 の注意
- `ImGui::SeparatorText` は無い → `Separator()` + `TextUnformatted()`
- `ImGuiViewport` は `GetWorkPos()` / `GetWorkSize()`
- キー入力は仮想キーコードで判定：`ImGui::IsKeyPressed('W')`、`0x71`(F2)、`0x0D`(Enter)、`' '`(Space)
- 大きい文字は `dl->AddText(font, size, ...)` と `font->CalcTextSizeA(...)`
- ビューポートへのドラッグ＆ドロップは「直前のUI部品＝描画画像」を受け取り先にしている。**画像とドロップ処理の間にボタンなどの部品を置かない**（線の描画だけなら可）。
- `ImDrawList::AddRect` の角の指定は `ImDrawCornerFlags_All`（新しい版の `ImDrawFlags` ではない）。

---

## 4. コードの構成

エンジンは `Source/Engine/`（名前空間 `uel`）。開始点（`wWinMain`）は `Source/Engine/Core/Main.cpp`。

**旧コードは削除済み**（コミット `8781c76`）。昔の実装（BehaviorTree、旧シェーダクラス、SphereCast など）を参考にしたいときは、削除直前のコミット `2df15d7` から読める：
```
git show 2df15d7:Source/AI/BehaviorTree.cpp
```
**シェーダは `Shader/`（プロジェクト直下）の `.hlsl` / `.hlsli`。** ビルド時に VS（FxCompile）が `Data/Shader/<名前>.cso` へコンパイルし、実行時に `ShaderLoader` が読む（9章。コミット `f5ac873` で C++ 内の文字列から移した）。昔の別実装の Toon / PBR（`8781c76` で一度消したもの）は `git show 8781c76:Shader/ToonPS.hlsl` のように読める。移行前の文字列版は `git show 53f5734:Source/Engine/Graphics/ShaderCommon.h`。

```
Shader/
  Common.hlsli         b0（ObjectCB）・t0/t1・s0/s1・PSIn・MakePSIn・MakeOutlinePSIn・PSOut（色＋法線の MRT）
  Lighting.hlsli       影（ShadowFactor）・Toon の段階・テクスチャの単純化・ShadeSurface（見た目の式はここ）
  SkyColor.hlsli       空の3色と SkyColor(dir)（空の描画と金属の映り込みで共用）
  MeshVertex.hlsli     静的メッシュの VSIn と ToWorld
  SkinnedVertex.hlsli  スキンドの VSIn と ToWorld（Skin を呼ぶ）
  Skinning.hlsli       b1（Bones[128]）と Skin()（本描画と影で共用）
  Shadow.hlsli         影パスの b0（ShadowCB）
  Skybox.hlsli / PostProcess.hlsli   それぞれの cbuffer と VS→PS の構造体
  MeshVS / MeshOutlineVS / SkinnedMeshVS / SkinnedMeshOutlineVS / ShadowVS / ShadowSkinnedVS /
  SkyboxVS / PostProcessVS  (.hlsl, 頂点)
  SurfacePS / OutlinePS / SkyboxPS / PostProcessPS  (.hlsl, ピクセル)
```

```
Source/Engine/
  Core/          Main（wWinMain）, Engine（メインループ、Scene管理、Play開始/停止/Scene遷移、FPS固定）
  Platform/      Window
  Config/        EngineConfig（kAssetRoot, kSceneDir, SceneFilePath, kTargetFps, kMaxDeltaTime, ClampDeltaTime）
  Components/    Component（基底）と、すべての Component（フォルダの中は分けない）
                 Transform / MeshRenderer / SkinnedMesh / Animator / Shader / HP / Player / Opponent /
                 Hitbox / NavObstacle / Spawner / GameRule / ScreenUI / PrefabInstance
  Scene/         EntityId, Entity, World
  Game/          GameContext, InputState, GameFlow（Component が使う、ゲーム側の受け渡し用の型）
  Animation/     Skeleton, AnimationClip, SkinnedModel, AnimationPose（ポーズの計算・ブレンド）,
                 AnimParams, AnimNode（基底・AnimNodePtr・AnimClipNode）, AnimBlendSpaceNode,
                 AnimStateMachineNode, AnimMontage
  Systems/       RenderSystem（描画）, AnimationSystem（毎フレームの姿勢の計算）
  Graphics/      GraphicsDevice, RenderTexture（シーン用／色だけ の2種類）, Mesh,
                 MeshShader, SkinnedMeshShader, SkinnedRenderModel,
                 ShaderCommon（b0 の C++ 側の形 ObjectCB と FillShading）, ShaderLoader（.cso の読み込み）,
                 ShadingParams（描き方のデータ）,
                 EngineSkybox, EngineShadowMap, SceneLighting, PostProcess, PostProcessSettings
  Assets/        GltfMeshLoader, MeshCache, GltfSkinnedLoader, SkinnedModelCache,
                 TinyGltfImpl（tinygltf / stb_image の実装の埋め込み。プロジェクトで1か所だけ）
  AI/            EngineNavGrid（A*）
  Physics/       GroundQuery（メッシュへのレイ）, HitShapes（球・カプセルへのレイ）
  Serialization/ StageSerializer（Scene・Scene一覧・Prefab・Environment）, AnimGraphSerializer（アニメーショングラフ）
  Editor/        ImGuiLayer（フォント・ドッキング）, ViewportPanel, EditorCamera,
                 EditorPanels（Hierarchy・メニューバー・ショートカット・Environment・NavGrid）,
                 InspectorPanel, AssetBrowserPanel,
                 AnimEditorPanels（Animation パネル、ノードの詳細欄、通知の編集欄）,
                 AnimGraphPanel（Anim Graph パネルの入口・パラメータ・モンタージュ）,
                 AnimGraphCanvas（キャンバスの共通処理とポーズグラフ）, AnimGraphStateMachineView（ステートマシンの画面）,
                 AnimGraphPanelInternal.h（Anim Graph の3ファイルで共有する定数と状態。namespace uel::animgraph）,
                 CommandHistory, EntityCommands, TransformEdit
```
- `EditorPanels.h` に、Editor の各パネルの関数（`DrawInspectorPanel` など）の宣言がまとまっている。`AnimGraphPanel.h` だけは別。
- Anim Graph の3ファイルは、パネルの状態を `uel::animgraph::g`（`PanelState`。実体は `AnimGraphCanvas.cpp`）で共有している。
- `Source/` 直下の `export_to_text.bat`・`structure.txt`・`ファイル階層構造テキスト出力ツール.bat` は、ユーザーの道具（ビルドとは関係ない）。

### 基本の考え方
- **Entity + Component。** `Component` は `virtual void Update(GameContext&)` と、純粋仮想の `Clone()` を持つ。
  描画・姿勢の計算・保存などはエンジン側（System / Serializer）が担当し、ゲームの動きは Component の `Update` に書く（UE寄りのハイブリッド）。
- **`EntityId` は index + generation。** 消したEntityの古いIDは無効になる。
- **編集用World と Play用World は別。** Play 開始時に `World::Clone()` で複製し、`playWorld` を動かす。Stop すれば編集内容は無傷。
  → **Play中に Inspector で変えた値は、実行中のWorldには反映されない**（次の Play から）。Anim Graph パネルは Play 中は実行中の World を表示するが、そこでの編集は残らない。
- Play 開始時、プレイヤーの `MeshRenderer` / `SkinnedMesh` の `firstPerson` を true にする（一人称表示）。

### 1フレームの流れ（`Engine::Run`）
1. タイトルバーの FPS 表示（0.5秒ごと）、メニューバー（保存・Play切替・Scene切替／追加／リネーム）
2. Play中なら、前フレームに出された Scene遷移リクエストを処理（`LoadPlayScene`）
3. `ViewportPanel::DrawUI`：（Play中）NavGrid を bake → 全 Component の `Update`（`dt` は `ClampDeltaTime` で上限付き）→ `FlushDestroy` → カメラ決定 → 画像表示 → オーバーレイ
4. 各パネル（Hierarchy / Inspector / Asset Browser / Environment / Animation / Anim Graph / NavGrid）。Environment はライト（`SceneLighting`）とポストプロセス（`PostProcessSettings`）の両方を編集する。どちらも `Engine` が1つずつ持つ
5. Prefab の Apply があれば全Sceneへ反映
6. `AnimationSystem::Update`：スキンドの全 Entity の姿勢を計算する（編集中も Play 中も動く。`dt` は上限付き）
7. `ViewportPanel::RenderScene`：**シャドウパス（静的 → スキンド）→ 空 → 静的メッシュ → スキンドメッシュ → ポストプロセス**（詳しくは9章「描画の流れ」）
8. ImGui の描画、`Present`、`FrameLimiter::Wait`（次のフレームの時刻まで待つ）

※ 3 の「画像表示」は 7 より先に呼ばれるが、ImGui が実際に描くのはフレームの最後なので、その時点の画像が出る。**表示用テクスチャの作り直し（リサイズ）は `DrawUI` の中だけで行う**（`RenderScene` で作り直すと、ImGui に渡したポインタが無効になる）。
※ Component の `Update`（3）は姿勢の計算（6）より先。だから **アニメの通知（Notify）をゲーム側が受け取るのは次のフレーム**になる。

### Play中の更新ループの決まり
- **添字で回している**（`for (size_t i = 0; i < world.Slots().size(); ++i)`）。Spawner が途中で Entity を増やしても壊れないようにするため。範囲for に戻さない。
- Component の `Update` の中で **自分自身の Entity を消すときは `world.DestroyLater(id)`**（全員の `Update` の後に `FlushDestroy` でまとめて消す）。`DestroyEntity` を直接呼ばない。
- `GameContext` の中身：`world, self, meshCache, device, dt, input, nav, flow`
- 入力（`InputState`）はフレームごと：`jumpPressed`・`shootClicked` は押した1フレームだけ true。

---

## 5. Component 一覧

| Component | 役割 | 主な項目 |
|---|---|---|
| Transform | 位置・回転（度）・スケール | `position, rotationEuler, scale` |
| MeshRenderer | 静的メッシュの描画。壁・床・地面の判定にも使う | `assetId, color, visible, hiddenSubmesh, submeshRole, firstPerson` |
| SkinnedMesh | スキン付きメッシュ（どのモデルか）と「いまの姿勢」。姿勢は AnimationSystem が書く | `assetPath, animPath`（追加のアニメ GLB。複数可）, `clipIndex, time, speed, playing, loop, visible, color, fpHideBone`（一人称で潰す骨）。実行時：`firstPerson, boneGlobal, palette` |
| Animator | アニメの制御（UE の AnimInstance）。グラフが空ならクリップ1つをクロスフェード再生、グラフがあればグラフが姿勢を決める（6章） | `clipIndex, speed, loop, playing, blendTime, params, graph, graphPath, montages`。`Play()`, `PlayMontage("名前")`, `StopMontage()`, `WasFired("通知名")` |
| Shader | その Entity の MeshRenderer / SkinnedMesh を**どう描くか**。付けていなければ既定（Lit）。中身は `ShadingParams` 1つ | `mode`(Lit / Unlit / Toon), `receiveShadow`, `metallic`, `roughness`, `specularStrength`, `rim*`, `toon*`（しきい値・ぼかし・陰の色・テクスチャの単純化）, `outline*`（背面法の輪郭線） |
| HP | 体力 | `hp, maxHp` |
| Player | 操作キャラ。**これを持つEntityがプレイヤー** | `moveSpeed, eyeHeight, eyeForward, groundY, radius`。アニメへ `Speed / MoveX / MoveY / Grounded / Jump / Attack / Dead` を渡す |
| Opponent | 敵。NavGrid の経路でプレイヤーを追い、近づいたら攻撃。HP 0 で倒れてから消える | `moveSpeed, stopRange, attackRange, attackDamage, attackInterval, attackHitDelay, attackMoveLock, deathDelay`。アニメへ `Speed / Attack / Dead` を渡し、通知 `Hit` でダメージ |
| Hitbox | 当たり判定（胴体カプセル＋頭の球） | `bodyBottom, bodyTop, bodyRadius, headHeight, headRadius, headMultiplier` |
| NavObstacle | NavGrid を塞ぐ箱 | `halfX, halfZ` |
| Spawner | Prefab から敵をウェーブで湧かせる | `prefabPath, waveCount, baseCount, countIncrease, spawnInterval, firstDelay, waveDelay, spawnRadius` |
| GameRule | 勝敗判定 → Scene遷移 | `winScene, loseScene, endDelay` |
| ScreenUI | Title / Result 画面 | `title, subtitle, buttonText, nextScene, showResult` |
| PrefabInstance | どの Prefab から作ったか | `path` |

### 今のゲームの仕様
- 操作：右ドラッグで視点、WASD 移動、Space ジャンプ、左クリックで射撃
- 射撃：基本ダメージ 34。ヒットボックスの頭は倍率（既定2倍）。壁の向こうには当たらない。ヒットボックスが無くHPを持つ物はメッシュで判定。倒れている最中の敵には当たらない。
- 敵：0.25秒ごとに A* で経路を再計算。攻撃は `attackInterval` ごと。ダメージは、グラフに通知 `Hit` があればその瞬間、無ければ `attackHitDelay` 秒後。HP が 0 になったら `Dead` を立て、`deathDelay` 秒後に `DestroyLater`。
- NavGrid（Play中）：中心 (0,0,0)、セル 1m、60×60 で固定。塞ぐのは NavObstacle を付けたEntityだけ。
- 流れ：Title →(START)→ Stage →(全ウェーブ撃破 or 死亡)→ Result →(RETRY)→ Stage
  - クリア条件：全 Spawner が最終ウェーブまで終了、かつ敵が0体
  - Scene遷移は `GameFlow::requestedScene` に名前を入れる（次のフレームの頭で切り替わる）
- 当たり判定に使う地面・壁は「Visible な MeshRenderer」。スキンドのキャラは MeshRenderer を非表示にして使う。
- 一人称：プレイヤーの `SkinnedMesh::fpHideBone`（頭など）のスケールをほぼ 0 にして、カメラの前に自分の頭が映らないようにする。

---

## 6. アニメーション

### 全体の形
- **`AnimatorComponent` が「状態と指示」、`AnimationSystem` が「計算」、`SkinnedMeshComponent` が「結果（`boneGlobal` / `palette`）」を持つ。** 描画（RenderSystem）は `palette` を読むだけ（影と本描画で同じものを使う）。
- `AnimationSystem::Update` の中の順番（Entity ごと）：
  1. ローカル姿勢を決める：Animator（グラフあり）→ Animator（クリップ1つ・クロスフェード）→ SkinnedMesh（クリップ1つ）の順に、あるものを使う
  2. モンタージュを上に重ねる（再生中だけ）
  3. 一人称なら `fpHideBone` を潰す
  4. 通知の記録（`events`、表示用の `lastEvent`）
  5. 行列にして `boneGlobal` / `palette` へ
- Trigger パラメータは、グラフの計算が終わったら毎フレーム 0 に戻す（1フレームだけ有効）。

### グラフ（ノードの木）
- ノードの基底は `AnimNode`（`Update` で時間を進め、`Evaluate` で姿勢を出す。`Clone` で子ごと複製）。ノードの持ち主は `AnimNodePtr`（コピーすると中身を Clone する。だから Animator の `Clone()` は普通のコピーで済む）。
- ノードの種類（`TypeName()` の文字列は保存にも使う。変えない）
  - **Clip**（`AnimClipNode`）：クリップを**名前で**指定して再生。`loop`、`speed`、`speedParam`（Float パラメータを速度に掛ける）、`notifies`（時刻と名前）
  - **BlendSpace1D**（`AnimBlendSpace1DNode`）：Float パラメータ1つを軸に、近い2つのクリップを混ぜる。2つは**同じ割合の位置（位相）**で再生する（歩きと走りの足がずれない）。`smoothTime` で入力をなめらかに追う
  - **StateMachine**（`AnimStateMachineNode`）：状態（中身は Clip か BlendSpace1D）と遷移。遷移は**上から順に調べて最初に成り立ったもの**を使う。条件はすべて成り立ったとき（Float は `> / <`、Bool は true / false、Trigger は立っていれば）。`from = -1` は「Any（どの状態からでも）」。`waitForEnd` は「移る前の状態のクリップが終わってから」（ループしないクリップのときだけ効く）
- 根に置けるのは3種類どれでも。状態の中身にステートマシンは入れられない（入れ子なし）。
- `CondOp` と `AnimParamType` は数値で保存している。**並び順を変えない**（増やすときは末尾）。

### パラメータ・通知・モンタージュ
- **ゲーム側はクリップを知らない。** パラメータに値を書くだけで、どのクリップを出すかはグラフが決める。
  - 書くときは `params.SetIfExists("Speed", v)`（グラフにその名前が無ければ何もしない。勝手に増やさないため）。
  - 名前は Component の定数で持つ（`PlayerComponent::kAnimSpeed` など）。Player と Opponent で `Speed / Attack / Dead` は同じ名前。
- **通知（Notify）**：Clip ノード（とモンタージュ）の時刻に名前を置くと、再生位置がそこを通ったフレームに `events` に入る。ゲーム側は**次のフレームに** `an->WasFired("Hit")` で受け取る。
- **モンタージュ**：グラフの上に1回だけ重ねて再生するクリップ（UE の Anim Montage）。定義（名前・クリップ・`blendIn`・`blendOut`・`rate`・通知）はグラフのアセットに保存する。ゲーム側は `PlayMontage("HitReact")` と名前で呼ぶ。下のグラフは止めずに進むので、抜けると「今の」動きに戻る。再生中に別のモンタージュを呼ぶと、今の姿勢から混ぜて切り替える。

### グラフのアセット（`AnimGraphSerializer`）
- `Data/Anim/<名前>.animgraph.json`。中身：パラメータの一覧（今の値が初期値。Trigger は 0 で保存）、ノードの木、モンタージュの定義。
- 版は `kAnimGraphVersion = 4`（1 最初 / 2 エディタ上の位置 / 3 通知 / 4 モンタージュ）。Scene と同じく「その版以降だけ読む」。
- Scene に保存するのは `graphPath` だけ（`anGraph`）。読み込み時にグラフのファイルを読む。**グラフの変更は「保存」ボタンを押すまでファイルに残らない。**
- 名前だけ（`Enemy`）を入れると `AnimGraphPathFromName` が `Data/Anim/Enemy.animgraph.json` にする。
- 今あるグラフ：`Character` / `Enemy` / `Large` / `Player1`。

### エディタ
- **Inspector の Animator 欄**：クリップ1つの再生、グラフのファイルの保存／読み込み、グラフの根が Clip / BlendSpace1D ならその詳細、パラメータの編集。
- **Anim Graph パネル**：上にファイルの行、左にパラメータとモンタージュ、中央にキャンバス、右に選んだものの詳細。
  - ポーズグラフ（根のノード → 出力ポーズ）。右クリックでノードの追加・置き換え・削除。StateMachine はダブルクリックで中へ。
  - ステートマシンの画面：右クリックで状態の追加・遷移の追加、ドラッグで移動、ホイールで拡大縮小、F で全体表示。
- **Animation パネル**：クリップの一覧とタイムライン（グラフを使っていない Entity のとき）。
- Asset Browser のグラフ（`[グラフ]`）は、Inspector や Anim Graph のファイル欄へドラッグ＆ドロップで読み込める（種類 `ASSET_ANIMGRAPH`）。

---

## 7. 新しい Component を足すときのチェックリスト

1. **ファイル名の重複を確認**（`Source/` 全体を検索）
2. `Source/Engine/Components/` にヘッダを作る：`Component` を継承し、`Clone()` を実装（`return std::make_unique<T>(*this);`）。動きが必要なら `Update(GameContext&)`
3. ユーザーが **.sln に追加**（`.cpp` があればそれも）
4. `Editor/InspectorPanel.cpp`
   - include を追加
   - 編集UIのブロック（`PushID` / `BulletText` / `削除` ボタン / 各項目 / `PopID`）を、**一番下の「+ コンポーネントを追加」より前（今は ScreenUI のブロックの後ろ）**に足す。`ImGui::End();` の直前ではない。
   - 「+ コンポーネントを追加」のポップアップの中（`BeginPopup` 〜 `EndPopup` の間）に `MenuItem` を1行。並びは編集UIと同じ順（見た目・アニメ → 区切り線 → ゲーム）。
   - 文字列の編集には `editStr` ラムダを使える（見出し「コンポーネント」の直後で定義している）。
   - 表示する文字は `u8"..."` で日本語に（2章）。
5. `Serialization/StageSerializer.cpp`（保存したい場合）
   - include を追加
   - `kSchemaLatest` を +1
   - `EntityDTO` にメンバを追加し、`serialize` に `if (g_loadSchema >= N) ar(...);` を追加
   - `ToDTO` と `FromDTO` に追加（`PrefabInstanceComponent` の行より前。※ Animator だけ今その後ろにあるが、PrefabInstance は `Update` を持たないので動作は同じ）
6. Prefab のインスタンスに付けた場合は **Prefab に反映** で全体に反映

※ Delete→Undo と Duplicate は `Clone()` で全 Component を複製するので、追加の作業は不要。

---

## 8. 保存形式

- Scene：`Json/<Scene名>.json`。一覧は `Json/Scenes.json`。起動時は `Scenes.json` の一覧で読み込み、無いときだけデモの2Sceneを作る。`Json/` フォルダが無ければ起動時に作る。
- Environment（ライトとポストプロセスの設定）：`Json/Environment.json`。**Scene を保存（Ctrl+S）したときに一緒に保存**し、起動時に読む。版は `kEnvSchemaLatest = 1`。確認用の表示モード（`debugView`）だけは保存しない。
- Prefab：`Data/Prefabs/<名前>.prefab.json`（Entity 1個ぶん。位置は 0 で保存）
- アニメーショングラフ：`Data/Anim/<名前>.animgraph.json`（6章）
- **Scene の schemaVersion は現在 19。** ファイルの版に存在する項目だけを読む方式なので、古いファイルもそのまま読める。

| 版 | 追加した項目 |
|---|---|
| 3 | Transform / Mesh / HP / Opponent / Player |
| 4 | SkinnedMesh |
| 5 | SkinnedMesh の animPath |
| 6 | NavObstacle |
| 7 | ScreenUI / GameRule |
| 8 | prefabPath（Prefab とのリンク） |
| 9 | Hitbox |
| 10 | Spawner |
| 11 | Shader（mode, receiveShadow） |
| 12 | Shader のスペキュラ・リム（`shSpecPower` は 12〜15 だけに存在） |
| 13 | Shader の Toon（しきい値・ぼかし・陰の色） |
| 14 | Shader のアウトライン（太さ・色） |
| 15 | Shader の Toon テクスチャ単純化（steps, smooth） |
| 16 | Shader の metallic / roughness（`shSpecPower` を廃止して `shRoughness` に置き換え） |
| 17 | Animator（clip, speed, loop, blendTime） |
| 18 | Animator のグラフのパス（`anGraph`） |
| 19 | SkinnedMesh の `fpHideBone`（`skFpHide`） |

- **項目を置き換えたときの読み方（版 16 の例）**：`shSpecPower` は `if (g_loadSchema >= 12 && g_loadSchema <= 15)` で古い版のときだけ読む。`shRoughness` は初期値 −1（ファイルに無かった印）にしておき、`FromDTO` でマイナスなら `RoughnessFromSpecularPower` で換算する。cereal の JSON は名前で探すので、読む順番がファイルの並びと違っても読める。
- `ShadingMode` は数値で保存している。**並び順を変えない**（増やすときは末尾）。
- Prefab のインスタンスが個別に持つのは **名前と Transform だけ**。それ以外は Scene を読むたびに Prefab の内容で作り直す（Prefab が見つからない場合だけ、Scene に保存した内容で復元）。インスタンスに付けた Component も、「Prefab に反映」をしないと残らない。
- 保存されない項目（今のところ）：Opponent の攻撃・死亡まわりの値（`attackRange` 〜 `deathDelay`）、Player の各値、SkinnedMesh の再生位置と `playing`、Animator の `playing`。

---

## 9. 描画・数学の決まり

- 左手系、Y が上。回転はオイラー角（度）。ワールド行列は `S * R * T`。
- DirectXMath の**行ベクトル形式**。シェーダへは `XMMatrixTranspose` して渡し、HLSL では `mul(ベクトル, 行列)`。
- glTF の行列は `m[i][j] = data[i * 4 + j]` で読む（これで行ベクトル形式として正しくなる）。
- スキニング：`palette[i] = inverseBind[i] * globalPose[i]`。ボーンは最大 128。4ボーンまで。
- `SkinnedVertex` は 80 バイト（position 0 / normal 12 / color 24 / uv 40 / bones(uint×4) 48 / weights 64）。
- シャドウマップ：2048、平行投影、バイアス 0.0025、3×3 PCF。**影が出る範囲はカメラに追従する正方形**（一辺は `SceneLighting::shadowRange`、既定 30m。Environment パネルの「影の範囲」）。
  - 中心は「カメラ位置から水平な前方へ `range × 0.35` 進めた点」（後ろより前を広く取る）。ライトの位置は中心から光と逆向きに 25m、奥行きは 0.1〜60m。
  - **中心はシャドウマップ1画素ぶん（`range / 2048`）の刻みに丸めている。** 丸めないと、カメラが動くたびに影の縁がちらつく。行列を作っているのは `ViewportPanel::RenderScene` の先頭。
  - シェーダ側（`ShadowFactor`）は、範囲の外（横・奥行きとも）を「影なし」にし、端から 15% は影を薄くして消す。
- カメラ：FOV 60度、near 0.1 / far 100
- スキン付きGLB：パーツごとに分かれたモデル（複数の skin+mesh ノード）に対応。アニメが別ファイルの場合は `animPath` に指定し（複数可）、**ボーン名**で対応付ける。同じ名前のクリップが重なったら、後から来たほうを捨てる（グラフはクリップを名前で探すため）。
- 面の向き：**外から見て時計回りが表**（D3D の既定）。既定の立方体も、手持ちの GLB も、この向きで確認済み。カメラ行列はすべて左手系（`...LH`）。

### シェーダの持ち方
- **HLSL は `Shader/` の `.hlsl` / `.hlsli`**（一覧は4章）。**ビルド時に** VS の FxCompile が `Data/Shader/<ファイル名>.cso` にコンパイルし、**実行時に** `ShaderLoader`（`CreateVertexShaderFromCso` / `CreatePixelShaderFromCso`）が読んでシェーダを作る。エンジンのコードに `D3DCompile` は残っていない（`d3dcompiler` を使うのは `imgui_impl_dx11.cpp` だけ）。
  - プロジェクト全体の既定（`Game.vcxproj` の `ItemDefinitionGroup`）：Shader Model 5.0、出力 `Data\Shader\%(Filename).cso`、エントリ関数は既定の `main`。**1ファイル＝1シェーダ**で、どの `.hlsl` も関数名は `main`。
  - **「シェーダーの種類」（頂点／ピクセル）はファイルごとのプロパティ**で、ユーザーが VS で設定する（Debug・Release の両方）。`.hlsli` は「ビルドに含めない」（`None`）。
  - 新しい `.hlsl` を足す手順：作業用フォルダで UTF-8（BOM なし）で作る → ユーザーが `cp` でコピー → ソリューションエクスプローラーで追加 → プロパティで種類を設定。
  - `.cso` が見つからないと、VS の「出力」に `[Shader] not found: Data/Shader/X.cso (...)` と出て、その部分が描かれない（空なら背景はクリア色のまま）。HLSL の文法エラーは**ビルドエラー**として出る（実行時ではない）。
  - `Data/Shader/*.cso` は Git 管理下（ビルドで上書きされる）。
- **`.hlsl` / `.hlsli` の文字コードは UTF-8（BOM なし）**。fxc で確かめた結果：
  - **BOM 付き** → `error X3000: Illegal character in shader file`。`.hlsli` に BOM があると、それを include した側で `unexpected #endif` や「型を解決できません」になる。
  - **Shift-JIS** → 日本語コメントの最後の文字の2バイト目が `\`（`表` `ソ` `能` など）だと、**次の行まで黙ってコメントになる**（エラーが出ないぶん危ない）。
  - **VS で新規作成して貼り付けると BOM 付きか Shift-JIS になる。** だから貼り付けではなく、作業用フォルダのファイルを `cp` でコピーしてもらう。直すときは VS の「保存オプションの詳細設定」／「エンコード付きで保存」で「Unicode (UTF-8 シグネチャなし) - コードページ 65001」。
  - まとめて変換するコマンド（UTF-8 として読めないファイルだけを Shift-JIS から変換する）：
    ```
    cd /c/Users/2240192/Desktop/Work/Shader && for f in *.hlsl *.hlsli; do if ! iconv -f UTF-8 -t UTF-8 "$f" >/dev/null 2>&1; then iconv -f CP932 -t UTF-8 "$f" > "$f.tmp" && mv "$f.tmp" "$f" && echo "converted: $f"; fi; done
    ```
- **メッシュ用のシェーダは `#include` で組み立てる。**
  - 頂点シェーダ（`MeshVS` / `SkinnedMeshVS` と、それぞれの `...OutlineVS`）は、`MeshVertex.hlsli` / `SkinnedVertex.hlsli` の `ToWorld`（ワールド位置と法線。スキンドはここで `Skin()`）を呼び、`Common.hlsli` の `MakePSIn` / `MakeOutlinePSIn` で出力を作る。
  - ピクセルシェーダは静的・スキンドで共通：`SurfacePS`（`ShadeSurface` の結果＋法線）、`OutlinePS`（単色＋法線）。
  - **見た目の式を変えるときは `Lighting.hlsli` だけ直せば、静的・スキンドの両方に効く。**
  - **影のスキニングも `Skinning.hlsli` の `Skin()` を使う**（本描画と同じ計算・同じ `palette`。影は姿勢に合わせて動く。確認済み）。
- **C++ と HLSL で形をそろえる組**（片方だけ直すと値がずれる。並び・型・16 バイト境界を合わせる）：
  - `ShaderCommon.h` の `ObjectCB` ⇔ `Common.hlsli` の `cbuffer ObjectCB`
  - `SkinnedMeshShader.cpp` / `EngineShadowMap.cpp` の `BoneCB` ⇔ `Skinning.hlsli` の `cbuffer Bones`（128 個）
  - `EngineShadowMap.cpp` の `SCB` ⇔ `Shadow.hlsli` の `cbuffer ShadowCB`
  - `EngineSkybox.cpp` の `SkyCB` ⇔ `Skybox.hlsli`、`PostProcess.cpp` の `PostCB` ⇔ `PostProcess.hlsli`
  - 入力レイアウト（`MeshShader.cpp` / `SkinnedMeshShader.cpp` / `EngineShadowMap.cpp`）⇔ 各 `VSIn` のセマンティクス。レイアウトは読み込んだ `.cso` のバイトコードで作る。
- スロット：`b0`＝`ObjectCB`（オブジェクト＋ライト＋カメラ位置＋描き方）、`b1`＝ボーン、`t0`＝テクスチャ、`t1`＝シャドウマップ、`s0`＝通常（線形・繰り返し）、`s1`＝影用の比較サンプラ。ポストプロセスは別で、`b0`＝`PostCB`、`t0`＝色、`t1`＝法線、`t2`＝深度、`s0`＝ポイント・端で止める。
- **描き方のデータの流れ**：`ShaderComponent`（Scene）が `ShadingParams`（Graphics）を1つ持つ → `RenderSystem` が取り出して `Draw` に渡す → `FillShading` が `ObjectCB` に詰める。Graphics 側は Component を知らない。
- **描き方の項目を足すときに直す場所**：`ShadingParams.h` → `ShaderCommon.h`（`ObjectCB`・`FillShading`）→ `Shader/Common.hlsli`（`cbuffer ObjectCB`）→ `Shader/Lighting.hlsli`（式）→ `InspectorPanel.cpp`（Inspector）→ `StageSerializer.cpp`（版を +1）。`MeshShader` / `SkinnedMeshShader` / `RenderSystem` は触らなくてよい。

### ライティングと材質（`ShadeSurface`）
- **Lit**：`下地 ×（アンビエント＋平行光×影）` ＋ スペキュラ（Blinn-Phong）＋ リムライト ＋ 金属の映り込み。
- **Unlit**：テクスチャ × 色そのまま。
- **Toon**：ハーフランバートを `toonThreshold` で2色に分ける（陰には `toonShadeColor` を掛ける）。シャドウマップの影も同じ「陰の色」になる。スペキュラ・リムは 0.5 を境に2値化。任意でテクスチャの単純化（周り8点との平均 → 明るさを段階に丸める。色相は保つ）。
- **metallic / roughness**（簡易版。PBR ではない）：metallic で下地の色（拡散）が消え、ハイライトと映り込みに下地の色が付く。roughness はハイライトの大きさ（`power = 2 / roughness^4 − 2`）と映り込みのぼけ。
- **映り込みは空の色を式で計算している**（キューブマップは使っていない）。空の3色と式は `SkyColor.hlsli` の1か所だけ（空の描画 `SkyboxPS` と `Lighting.hlsli` の両方が include する）。
- **アウトライン（背面法）**：`outlineWidth > 0` の Entity は、本描画の直後に同じ `Draw` の中でもう1回描く。法線方向に押し出した形を `CULL_FRONT`（裏面だけ）で単色描画。定数バッファもボーンも本描画のものを使う。太さはワールドの長さ。

### 描画の流れ（`ViewportPanel::RenderScene`）
1. **シャドウパス**：カメラに追従するライト行列を作り、ライトから見た深度をシャドウマップに書く（静的 → スキンド）。
2. **`rt.Begin`**：シーン用の描画先（色・法線・深度）をクリアし、**色だけ**を描画先にする。
3. **空**：色だけを描く（空のシェーダは法線を書かないため）。
4. **`rt.BindSceneTargets`**：色＋法線の2枚を描画先にする（MRT）。
5. **静的メッシュ → スキンドメッシュ**：`SurfacePS` / `OutlinePS` が色（`SV_TARGET0`）とワールド法線（`SV_TARGET1`。`×0.5+0.5`、a=1 が「物がある」印）を同時に書く。
6. **ポストプロセス**：`postProcess.Apply(context, rt, finalRT, postSettings, proj)`。`rt` の色・法線・深度を読んで `finalRT` に書く。**Viewport に表示するのは `finalRT`。**

- **`RenderTexture` は2種類**（`Create` の4つ目の引数 `forScene`）。`true`＝シーン用（色＋法線＋深度。3つとも SRV で読める。深度は `R24G8_TYPELESS` で作り、DSV と SRV で形式を分けている）、`false`＝色だけ（深度なし。`finalRT`）。
- **同じテクスチャを読みながら書くことはできない。** だから描画先が2枚要る。`Apply` の最後で SRV を外している（次のフレームでまた描画先にするため）。
- **ポストプロセスの中身**（`Shader/PostProcessPS.hlsl`。`PostProcess.cpp` が画面を覆う三角形を1枚描く）：彩度 → エッジ検出の輪郭線（任意）。確認用の表示モード（完成 / 法線 / 深度 / エッジ）がある。
- **エッジ検出**：周り4方向と比べる。深度は「2階の差分（両隣の合計 − 中心の2倍）」を使う。深度バッファの生の値は平らな面の上で一定の割合で変わるので、床を浅い角度で見ても 0 になる。段差は距離に対する割合に直してからしきい値と比べる。法線は `1 − dot`。遠くの線は薄くして消す。
- 深度から距離への変換は `proj._43 / (深度 − proj._33)`（near / far を別に渡さず、射影行列の要素から求める）。
- グリッド・ギズモ・HP バーなどは ImGui で画像の上に重ねているので、ポストプロセスは掛からない。

---

## 10. 既知の制限・気づいている問題

- `Game.vcxproj` に、存在しないファイル `External\SphereCast\Include\SphereCast.h` の登録が残っている（ビルドには影響しない）。
- 外部ライブラリの Tracy / DirectXTex、`imgui_demo.cpp` は、今のエンジンからは使われていない。
- `GraphicsDevice.cpp` の `Present(0, 0)` は VSync オフだが、横のコメントは「VSync ON」のまま。
- **時間・FPS**
  - FPS は 60 固定だが VSync はオフなので、カメラを速く回すとティアリングが見えることがある。
  - `dt` の上限は 0.1 秒（`kMaxDeltaTime`）。10FPS を下回るほど重いと、ゲーム全体がゆっくりになる（ワープしない代わり）。止まっていた間の `playTime` も 0.1 秒ぶんしか数えない。
  - ゲームの更新は固定ステップではない（可変 `dt`）。重力とジャンプは「速度→位置」の順の計算なので、`dt` によってジャンプの高さが少し変わる（60FPS で約 1.15m）。
  - 編集用のカメラ（`EditorCamera.cpp`）は `io.DeltaTime` をそのまま使っている（上限なし）。
- **描画まわり**
  - `ShaderComponent` は Entity 単位。サブメッシュごと（鎧だけ金属、など）には分けられない。
  - glTF のメタリック／ラフネスは読んでいない。手持ちのモデルの値はほぼ変換ツールの既定値（0.2 / 0.8、または未指定）なので、読んでも部位の差は出ない。
  - 金属の映り込みは空だけ（周りの物は映らない。屋内でも空が映る。影の中でも暗くならない）。
  - 背面法のアウトラインは、太さがワールドの長さなので遠くで細くなる。角で法線が分かれているメッシュ（立方体）は角で線が途切れる。スケールがマイナスの Entity は塗りつぶされる。
  - エッジ検出の線にアンチエイリアスは無い。色の違いは見ていない。面ごとに法線が分かれたローポリは線が多く出る。
  - Toon の陰は2色だけ。`toonThreshold` を 0.5 より下げるなら `receiveShadow` を外す（自分の影で境目がガタつく）。
  - ライティングは LDR・ガンマ補正なしのまま。強さを上げすぎると白く飛ぶ。
  - 影：「影の範囲」を広げると粗くなる（30m で約 1.5cm、60m で約 3cm 単位）。中心から光源側へ 25m より高い物は影を落とさない。Toon の影は2値化しているので、範囲の端でやや唐突に消える。シャドウマップの大きさ `2048` は `Lighting.hlsli`（`texel = 1.0 / 2048.0`）にも書いてあり、変えるときは C++ と両方を直す。
  - 影の形は光の角度で決まる。今の光の向き（-0.4, -1.0, 0.35）は高さ約 62° なので、影は足元に短くまとまり、手足の動きが分かりにくい。動きを見せたいなら Environment の「光の向き」で光を低くする（例：(-1.0, -0.45, 0.2) で約 24°、影は身長の約 2.3 倍）。
  - 一人称のプレイヤーの影は、`fpHideBone` を潰した後の姿勢を使うので、頭が無い。
  - `ViewportPanel::Initialize` は各シェーダの `Initialize` の戻り値を見ていない（`.cso` が無くても起動は続き、その部分が描かれないだけ）。
  - シェーダのホットリロードは無い（`.hlsl` を変えたらビルドして起動し直す）。
- `Data/Sprite/IBL/` に IBL 用の DDS が7つある（約 285MB、Git 管理下）が、**使っていない**。中身は屋外の実写 HDRI から作った正しい一式（キューブ 1024²・RGBA16F・ミップ付き、`specular_pmrem` は事前計算済み）。本格的な PBR をやるなら使えるが、Toon 路線とは絵が合わない。布用の3つ（`sheen_pmrem` / `lut_charlie` / `lut_sheen_e`）は使い道がない。
- **アニメーション**
  - Blend Space は1軸だけ（2D は無い）。上半身だけ別の動き（ボーンごとのレイヤー）、ルートモーションは無い。
  - 状態の中にステートマシンは入れられない。モンタージュは同時に1つ、全身に掛かる。
  - 通知をゲーム側が受け取るのは1フレーム後。
  - Animation パネルのタイムラインは、グラフを使っていない Entity のときだけ。
  - Anim Graph パネルは選んだ Entity のグラフを直接編集する。同じグラフのファイルを使うほかの Entity には、保存して読み込み直すまで反映されない。
- ヒットボックスは体の中心線に固定。ボーン（頭など）に追従しない。
- NavGrid の範囲が固定（原点中心、60×60）。
- プレイヤーの移動・攻撃は `PlayerComponent` 1つに全部入っている（キャラごとの差は未対応）。
- Play中にコンポーネントを足した／消した結果は、編集用Worldにしか入らない。
- Prefab の Apply / Revert、Component の追加／削除は Undo できない。
- **エディタ**
  - Inspector は、画面外にはみ出た項目も毎フレーム処理している（Debug ではキャラを選ぶと少し重くなる。Component ごとに折りたためるようにすると軽くなる）。
  - `GlobalSkinnedCache().GetOrLoad` は呼ぶたびにキーの文字列を作る（Inspector などで毎フレーム数回呼ばれる。今は問題になっていない）。
  - パラメータ欄など一部は、文字の幅を数値（px）で決めている。フォントを変えたら見直す。

---

## 11. これからやる予定（セッションを分けて進める）

- ~~本格的なシェーダ~~ → **一区切り済み**。残っている候補：
  - 部位ごとの材質（`ShaderComponent` をサブメッシュ単位に）
  - ~~HLSL をファイルに出す~~ → **済み**（`Shader/*.hlsl` → ビルドで `.cso`。コミット `f5ac873`）。残り：ホットリロード（実行中に `.hlsl` を `D3DCompile` し直す）
  - metallic / roughness の見え方を直す（金属感とラフネスが期待と違う、とユーザーから指摘あり。次にやる）
- ~~アニメーション（ブレンド、ステートマシンなど）~~ → **一区切り済み**（Animator、クロスフェード、ノードの木、Blend Space 1D、ステートマシン、ノードエディタ、通知、モンタージュ、グラフのアセット）。残っている候補：Blend Space 2D、上半身レイヤー、ルートモーション
- ~~ファイルの分割・フォルダ整理・エディタの日本語化・FPS 固定~~ → **済み**
- 当たり判定（ボーン追従のヒットボックスなど）
- プレイヤーのキャラごとの攻撃方法・移動方法
- （必要になったら）ゲームの更新を固定ステップにする（物理を作るとき、60FPS を下回るほど重くなったとき）。入力の「押した1フレーム」を取りこぼさない工夫が要る
- （必要になったら）Inspector の Component を折りたためるようにする（`CollapsingHeader`）
