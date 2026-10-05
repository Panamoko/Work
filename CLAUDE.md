# UE-Like Engine（就職作品）作業ガイド

Overwatch風FPSを、自作のUEライクなエンジン（C++17 / DirectX11）で作るプロジェクト。
このファイルは、別セッションでも同じ前提で作業するための「ルール」と「現状」のまとめ。

---

## 1. 絶対に守るルール

1. **GitHub へ push / commit しない。** Git の更新はユーザーが自分で行う。
2. **ソースコードを直接編集しない。** 変更は「貼り付け用のコード」として提示し、ユーザーが自分で適用する。
   - ファイルを読む・検索する・ビルドして原因を調べる、は行ってよい。
   - 誤って編集した場合は、すぐ元に戻して報告する。
3. **`.vcxproj` の設定を変えない。** 新しいファイルは、ユーザーが Visual Studio のソリューションエクスプローラーから追加する。
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
- **変更が多いファイルは、ファイル全体を出す。** 部分的な指示が3〜4か所を超えるなら全体のほうが安全。
- **既存のファイルに手を入れる前に、今の中身を読む。** ユーザーが名前や構成を変えていることがある。
- **最後に確認手順を書く。** 何をして、どうなれば成功かを番号付きで。
- **既知の制限や割り切りも書く。**
- **出す前に、作業用フォルダで確かめる。** 今のリポジトリのファイルに変更を当てた複製を作業用フォルダ（リポジトリの外）に作り、`cl /c` でコンパイル、HLSL は `fxc` で検証してから提示する。「どこまで確かめたか（実機の見た目は未確認、など）」も書く。
- **行を足す指示の見本には、足す行だけを載せる。** 前後の行も見せるときは「追加後はこうなる」と明記し、すでにある行（`ImGui::End();` など）が二重に貼られないようにする。
- 説明は日本語。コード中のコメントも日本語でよい。**ただし HLSL の文字列の中は英語（ASCII）だけ**（理由は8章）。
- **画面に出す文字（ImGui）は英語にする。** 今のフォントには日本語が入っておらず「?」になる。

### エラー報告を受けたときの進め方
- まず実際のファイルを読み、必要ならビルドして、**本物のエラーとそうでないものを切り分ける**。
- ビルド（読み取り目的。成果物は `bin\x64\Debug\Game.exe`）：
  ```
  "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Game.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:m /clp:ErrorsOnly
  ```
- **無視してよい表示**（コンパイルエラーではない）：
  - 「関数定義が見つかりません」「〇〇があいまいです」→ IntelliSense の古い情報
  - cereal / rapidjson / ImGui の「メンバー変数が初期化されていません (type.6)」、`switch` のフォールスルー (es.78) → 外部ライブラリへのコード分析警告
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

---

## 3. 環境・前提

- C++17 / DirectX 11 / Visual Studio 2022 (v143) / x64 / `Game.sln`・`Game.vcxproj`
- 外部ライブラリ（`External/`）：imgui-docking（**1.80 WIP**）、ImGuizmo、tinygltf、cereal、DirectXTex、Tracy
- **ソースの文字コードは Shift-JIS（CP932）・改行 CRLF。** ツールで読むと日本語コメントが文字化けして見えるが、壊れてはいない。貼り付けて保存するときに VS が「Unicode で保存しますか」と聞くことがある。
- 作業ディレクトリ（実行時のカレント）はプロジェクト直下。アセットは `Data/` 以下（`kAssetRoot = "Data"`）。パスは `Data/Model/...` の形。
- Debug の情報形式は `/Zi`（`/ZI` だと Tracy がエラーになる）。

### ImGui 1.80 の注意
- `ImGui::SeparatorText` は無い → `Separator()` + `TextUnformatted()`
- `ImGuiViewport` は `GetWorkPos()` / `GetWorkSize()`
- キー入力は仮想キーコードで判定：`ImGui::IsKeyPressed('W')`、`0x71`(F2)、`0x0D`(Enter)、`' '`(Space)
- 大きい文字は `dl->AddText(font, size, ...)` と `font->CalcTextSizeA(...)`
- ビューポートへのドラッグ＆ドロップは「直前のUI部品＝描画画像」を受け取り先にしている。**画像とドロップ処理の間にボタンなどの部品を置かない**（線の描画だけなら可）。

---

## 4. コードの構成

エンジンは `Source/Engine/`（名前空間 `uel`）。開始点（`wWinMain`）だけ `Source/Core/Main.cpp` にある。

**旧コードは削除済み**（コミット `8781c76`）。昔の実装（BehaviorTree、旧シェーダクラス、SphereCast など）を参考にしたいときは、削除直前のコミット `2df15d7` から読める：
```
git show 2df15d7:Source/AI/BehaviorTree.cpp
```

**旧 `Shader/` フォルダ（`.hlsl` / `.hlsli`）と `Data/CompiledShader/` は削除済み。** 今のエンジンにシェーダのファイルは無く、HLSL は C++ 内の文字列を起動時にコンパイルしている（8章）。旧 Toon / PBR の実装を見たいときは `git show 8781c76:Shader/ToonPS.hlsl` のように読める。

```
Source/Core/Main.cpp   開始点（wWinMain → uel::Engine）
Source/Engine/
  Core/          Engine（メインループ、Scene管理、Play開始/停止/Scene遷移）
  Platform/      Window
  Graphics/      GraphicsDevice, RenderTexture（シーン用／色だけ の2種類）, Mesh,
                 MeshShader, SkinnedMeshShader, SkinnedRenderModel,
                 ShaderCommon（共通の HLSL と b0 の形）, ShadingParams（描き方のデータ）,
                 EngineSkybox, EngineShadowMap, SceneLighting,
                 PostProcess, PostProcessSettings
  Assets/        GltfMeshLoader, MeshCache, GltfSkinnedLoader, SkinnedModelCache,
                 TinyGltfImpl（tinygltf / stb_image の実装の埋め込み。プロジェクトで1か所だけ）
  Animation/     Skeleton, AnimationClip, SkinnedModel, AnimationPose（ポーズ評価）
  Scene/         EntityId, Entity, World, Component,
                 Transform / MeshRenderer / HP / SkinnedMesh / Shader / PrefabInstance の各Component
  Game/          GameContext, InputState, GameFlow,
                 Player / Opponent / GameRule / ScreenUI / Hitbox / Spawner の各Component
  AI/            EngineNavGrid（A*）, NavObstacleComponent
  Physics/       GroundQuery（メッシュへのレイ）, HitShapes（球・カプセルへのレイ）
  Systems/       RenderSystem
  Serialization/ StageSerializer（Scene・Scene一覧・Prefab）
  Editor/        ImGuiLayer, ViewportPanel, EditorPanels, EditorCamera,
                 CommandHistory, EntityCommands, TransformEdit
  Config/        EngineConfig
```

### 基本の考え方
- **Entity + Component。** `Component` は `virtual void Update(GameContext&)` と、純粋仮想の `Clone()` を持つ。
  描画・保存などはエンジン側（System / Serializer）が担当し、ゲームの動きは Component の `Update` に書く（UE寄りのハイブリッド）。
- **`EntityId` は index + generation。** 消したEntityの古いIDは無効になる。
- **編集用World と Play用World は別。** Play 開始時に `World::Clone()` で複製し、`playWorld` を動かす。Stop すれば編集内容は無傷。
  → **Play中に Inspector で変えた値は、実行中のWorldには反映されない**（次の Play から）。

### 1フレームの流れ（`Engine::Run`）
1. メニューバー（保存・Play切替・Scene切替／追加／リネーム）
2. Play中なら、前フレームに出された Scene遷移リクエストを処理（`LoadPlayScene`）
3. `ViewportPanel::DrawUI`：スキンドの時間を進める →（Play中）NavGrid を bake → 全 Component の `Update` → カメラ決定 → 画像表示 → オーバーレイ
4. 各パネル（Hierarchy / Inspector / Asset Browser / Environment / Animation / NavGrid）。Environment はライト（`SceneLighting`）とポストプロセス（`PostProcessSettings`）の両方を編集する。どちらも `Engine` が1つずつ持つ
5. Prefab の Apply があれば全Sceneへ反映
6. `ViewportPanel::RenderScene`：**シャドウパス（静的 → スキンド）→ 空 → 静的メッシュ → スキンドメッシュ → ポストプロセス**（詳しくは8章「描画の流れ」）

※ 3 の「画像表示」は 6 より先に呼ばれるが、ImGui が実際に描くのはフレームの最後なので、その時点の画像が出る。**表示用テクスチャの作り直し（リサイズ）は `DrawUI` の中だけで行う**（`RenderScene` で作り直すと、ImGui に渡したポインタが無効になる）。

### Play中の更新ループの決まり
- **添字で回している**（`for (size_t i = 0; i < world.Slots().size(); ++i)`）。Spawner が途中で Entity を増やしても壊れないようにするため。範囲for に戻さない。
- Component の `Update` の中で **自分自身の Entity を Destroy しない**。
- `GameContext` の中身：`world, self, meshCache, device, dt, input, nav, flow`

---

## 5. Component 一覧

| Component | 役割 | 主な項目 |
|---|---|---|
| Transform | 位置・回転（度）・スケール | `position, rotationEuler, scale` |
| MeshRenderer | 静的メッシュの描画。壁・床・地面の判定にも使う | `assetId, color, visible, hiddenSubmesh, submeshRole, firstPerson` |
| SkinnedMesh | アニメ付きメッシュ | `assetPath, animPath, clipIndex, time, speed, playing, loop, visible, color` |
| Shader | その Entity の MeshRenderer / SkinnedMesh を**どう描くか**。付けていなければ既定（Lit）。中身は `ShadingParams` 1つ | `mode`(Lit / Unlit / Toon), `receiveShadow`, `metallic`, `roughness`, `specularStrength`, `rim*`, `toon*`（しきい値・ぼかし・陰の色・テクスチャの単純化）, `outline*`（背面法の輪郭線） |
| HP | 体力 | `hp, maxHp` |
| Player | 操作キャラ。**これを持つEntityがプレイヤー** | `moveSpeed, eyeHeight, eyeForward, groundY, radius` |
| Opponent | 敵。NavGrid の経路でプレイヤーを追い、接触で攻撃 | `moveSpeed, stopRange, attackRange, attackDamage, attackInterval` |
| Hitbox | 当たり判定（胴体カプセル＋頭の球） | `bodyBottom, bodyTop, bodyRadius, headHeight, headRadius, headMultiplier` |
| NavObstacle | NavGrid を塞ぐ箱 | `halfX, halfZ` |
| Spawner | Prefab から敵をウェーブで湧かせる | `prefabPath, waveCount, baseCount, countIncrease, spawnInterval, firstDelay, waveDelay, spawnRadius` |
| GameRule | 勝敗判定 → Scene遷移 | `winScene, loseScene, endDelay` |
| ScreenUI | Title / Result 画面 | `title, subtitle, buttonText, nextScene, showResult` |
| PrefabInstance | どの Prefab から作ったか | `path` |

### 今のゲームの仕様
- 操作：右ドラッグで視点、WASD 移動、Space ジャンプ、左クリックで射撃
- 射撃：基本ダメージ 34。ヒットボックスの頭は倍率（既定2倍）。壁の向こうには当たらない。ヒットボックスが無くHPを持つ物はメッシュで判定。
- 敵：0.25秒ごとに A* で経路を再計算。HP が 0 で Destroy。
- NavGrid（Play中）：中心 (0,0,0)、セル 1m、60×60 で固定。塞ぐのは NavObstacle を付けたEntityだけ。
- 流れ：Title →(START)→ Stage →(全ウェーブ撃破 or 死亡)→ Result →(RETRY)→ Stage
  - クリア条件：全 Spawner が最終ウェーブまで終了、かつ敵が0体
  - Scene遷移は `GameFlow::requestedScene` に名前を入れる（次のフレームの頭で切り替わる）
- 当たり判定に使う地面・壁は「Visible な MeshRenderer」。スキンドのキャラは MeshRenderer を非表示にして使う。

---

## 6. 新しい Component を足すときのチェックリスト

1. **ファイル名の重複を確認**（`Source/` 全体を検索）
2. ヘッダを作る：`Component` を継承し、`Clone()` を実装（`return std::make_unique<T>(*this);`）。動きが必要なら `Update(GameContext&)`
3. ユーザーが **.sln に追加**（`.cpp` があればそれも）
4. `EditorPanels.cpp`
   - include を追加
   - **Add Component のポップアップの中**（`BeginPopup` 〜 `EndPopup` の間）に `MenuItem` を1行
   - Inspector に編集UIのブロック（`PushID` / `BulletText` / `Remove` ボタン / 各項目 / `PopID`）。Inspector の `ImGui::End();` より前。文字列を編集するなら `editStr` ラムダの定義より後ろ。
5. `StageSerializer.cpp`（保存したい場合）
   - include を追加
   - `kSchemaLatest` を +1
   - `EntityDTO` にメンバを追加し、`serialize` に `if (g_loadSchema >= N) ar(...);` を追加
   - `ToDTO` と `FromDTO` に追加（`PrefabInstanceComponent` の行より前）
6. Prefab のインスタンスに付けた場合は **Apply to Prefab** で全体に反映

※ Delete→Undo と Duplicate は `Clone()` で全 Component を複製するので、追加の作業は不要。

---

## 7. 保存形式

- Scene：`<Scene名>.json`（プロジェクト直下）。一覧は `Scenes.json`。起動時は `Scenes.json` の一覧で読み込み、無いときだけデモの2Sceneを作る。
- Prefab：`Data/Prefabs/<名前>.prefab.json`（Entity 1個ぶん。位置は 0 で保存）
- **schemaVersion は現在 16。** ファイルの版に存在する項目だけを読む方式なので、古いファイルもそのまま読める。

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

- **項目を置き換えたときの読み方（版 16 の例）**：`shSpecPower` は `if (g_loadSchema >= 12 && g_loadSchema <= 15)` で古い版のときだけ読む。`shRoughness` は初期値 −1（ファイルに無かった印）にしておき、`FromDTO` でマイナスなら `RoughnessFromSpecularPower` で換算する。cereal の JSON は名前で探すので、読む順番がファイルの並びと違っても読める。
- `ShadingMode` は数値で保存している。**並び順を変えない**（増やすときは末尾）。
- Prefab のインスタンスが個別に持つのは **名前と Transform だけ**。それ以外は Scene を読むたびに Prefab の内容で作り直す（Prefab が見つからない場合だけ、Scene に保存した内容で復元）。インスタンスに付けた `ShaderComponent` も、Apply to Prefab をしないと残らない。
- 保存されない項目（今のところ）：ライト設定、ポストプロセスの設定（彩度・エッジ検出・表示モード）、Opponent の攻撃系の値、Player の各値、SkinnedMesh の再生位置。

---

## 8. 描画・数学の決まり

- 左手系、Y が上。回転はオイラー角（度）。ワールド行列は `S * R * T`。
- DirectXMath の**行ベクトル形式**。シェーダへは `XMMatrixTranspose` して渡し、HLSL では `mul(ベクトル, 行列)`。
- glTF の行列は `m[i][j] = data[i * 4 + j]` で読む（これで行ベクトル形式として正しくなる）。
- スキニング：`palette[i] = inverseBind[i] * globalPose[i]`。ボーンは最大 128。4ボーンまで。
- `SkinnedVertex` は 80 バイト（position 0 / normal 12 / color 24 / uv 40 / bones(uint×4) 48 / weights 64）。
- シャドウマップ：2048、平行投影 30×30（**原点を中心に約 ±15m だけ影が出る**）、バイアス 0.0025、3×3 PCF
- カメラ：FOV 60度、near 0.1 / far 100
- スキン付きGLB：パーツごとに分かれたモデル（複数の skin+mesh ノード）に対応。アニメが別ファイルの場合は `animPath` に指定し、**ボーン名**で対応付ける。
- 面の向き：**外から見て時計回りが表**（D3D の既定）。既定の立方体も、手持ちの GLB も、この向きで確認済み。カメラ行列はすべて左手系（`...LH`）。

### シェーダの持ち方
- **HLSL は C++ 内の文字列**を、起動時に `D3DCompile` でコンパイルする（`vs_5_0` / `ps_5_0`）。`.hlsl` ファイルは無い。コンパイルは `ShaderCommon.h` の `CompileShaderSource` を通す（失敗すると VS の「出力」に `[Shader] compile failed: ...` とエラー内容が出る）。
- **HLSL の文字列の中に日本語コメントを書かない。** ソースが Shift-JIS なので、文字によっては2バイト目が `\` になり、行末にあると次の行までコメント扱いになる。
- **メッシュ用のシェーダは「共通部分＋各自の頂点シェーダ」を連結してコンパイルする。**
  - 共通部分＝`ShaderCommon.h` の `kShaderCommonHLSL`：`b0` の宣言、`MakePSIn` / `MakeOutlinePSIn`、影、ライティング（`ShadeSurface`）、ピクセルシェーダ（`PSMain` / `PSOutline`）。
  - 各自の部分＝`MeshShader.cpp` の `kMeshVS`、`SkinnedMeshShader.cpp` の `kSkinnedVS`：`VSIn` と `ToWorld`（ワールド位置と法線を求める。スキンドはここでスキニング）、`VSMain` / `VSOutline`。
  - **見た目の式を変えるときは `ShaderCommon.h` だけ直せば、静的・スキンドの両方に効く。**
- スロット：`b0`＝`ObjectCB`（オブジェクト＋ライト＋カメラ位置＋描き方）、`b1`＝ボーン、`t0`＝テクスチャ、`t1`＝シャドウマップ、`s0`＝通常（線形・繰り返し）、`s1`＝影用の比較サンプラ。ポストプロセスは別で、`b0`＝`PostCB`、`t0`＝色、`t1`＝法線、`t2`＝深度、`s0`＝ポイント・端で止める。
- **描き方のデータの流れ**：`ShaderComponent`（Scene）が `ShadingParams`（Graphics）を1つ持つ → `RenderSystem` が取り出して `Draw` に渡す → `FillShading` が `ObjectCB` に詰める。Graphics 側は Component を知らない。
- **描き方の項目を足すときに直す場所**：`ShadingParams.h` → `ShaderCommon.h`（`ObjectCB`・`FillShading`・HLSL の `cbuffer` と式の3つをそろえる）→ `EditorPanels.cpp`（Inspector）→ `StageSerializer.cpp`（版を +1）。`MeshShader` / `SkinnedMeshShader` / `RenderSystem` は触らなくてよい。

### ライティングと材質（`ShadeSurface`）
- **Lit**：`下地 ×（アンビエント＋平行光×影）` ＋ スペキュラ（Blinn-Phong）＋ リムライト ＋ 金属の映り込み。
- **Unlit**：テクスチャ × 色そのまま。
- **Toon**：ハーフランバートを `toonThreshold` で2色に分ける（陰には `toonShadeColor` を掛ける）。シャドウマップの影も同じ「陰の色」になる。スペキュラ・リムは 0.5 を境に2値化。任意でテクスチャの単純化（周り8点との平均 → 明るさを段階に丸める。色相は保つ）。
- **metallic / roughness**（簡易版。PBR ではない）：metallic で下地の色（拡散）が消え、ハイライトと映り込みに下地の色が付く。roughness はハイライトの大きさ（`power = 2 / roughness^4 − 2`）と映り込みのぼけ。
- **映り込みは空の色を式で計算している**（キューブマップは使っていない）。空の3色は `EngineSkybox.cpp` と `ShaderCommon.h` の2か所に同じ値を書いている。
- **アウトライン（背面法）**：`outlineWidth > 0` の Entity は、本描画の直後に同じ `Draw` の中でもう1回描く。法線方向に押し出した形を `CULL_FRONT`（裏面だけ）で単色描画。定数バッファもボーンも本描画のものを使う。太さはワールドの長さ。

### 描画の流れ（`ViewportPanel::RenderScene`）
1. **シャドウパス**：ライトから見た深度をシャドウマップに書く（静的 → スキンド）。
2. **`rt.Begin`**：シーン用の描画先（色・法線・深度）をクリアし、**色だけ**を描画先にする。
3. **空**：色だけを描く（空のシェーダは法線を書かないため）。
4. **`rt.BindSceneTargets`**：色＋法線の2枚を描画先にする（MRT）。
5. **静的メッシュ → スキンドメッシュ**：`PSMain` / `PSOutline` が色（`SV_TARGET0`）とワールド法線（`SV_TARGET1`。`×0.5+0.5`、a=1 が「物がある」印）を同時に書く。
6. **ポストプロセス**：`postProcess.Apply(context, rt, finalRT, postSettings, proj)`。`rt` の色・法線・深度を読んで `finalRT` に書く。**Viewport に表示するのは `finalRT`。**

- **`RenderTexture` は2種類**（`Create` の4つ目の引数 `forScene`）。`true`＝シーン用（色＋法線＋深度。3つとも SRV で読める。深度は `R24G8_TYPELESS` で作り、DSV と SRV で形式を分けている）、`false`＝色だけ（深度なし。`finalRT`）。
- **同じテクスチャを読みながら書くことはできない。** だから描画先が2枚要る。`Apply` の最後で SRV を外している（次のフレームでまた描画先にするため）。
- **ポストプロセスの中身**（`PostProcess.cpp`。画面を覆う三角形を1枚描く）：彩度 → エッジ検出の輪郭線（任意）。確認用の表示モード（Final / Normal / Depth / Edge）がある。
- **エッジ検出**：周り4方向と比べる。深度は「2階の差分（両隣の合計 − 中心の2倍）」を使う。深度バッファの生の値は平らな面の上で一定の割合で変わるので、床を浅い角度で見ても 0 になる。段差は距離に対する割合に直してからしきい値と比べる。法線は `1 − dot`。遠くの線は薄くして消す。
- 深度から距離への変換は `proj._43 / (深度 − proj._33)`（near / far を別に渡さず、射影行列の要素から求める）。
- グリッド・ギズモ・HP バーなどは ImGui で画像の上に重ねているので、ポストプロセスは掛からない。

---

## 9. 既知の制限・気づいている問題

- 使われていないファイルが3つ残っている：`Graphics/CubeRenderer.cpp` / `.h`（初期の名残り）、`Editor/TransformSnapshot.h`（`TransformEdit.h` と中身が同じ複製。両方 include すると二重定義になる）。消してよい。
- `Game.vcxproj` に、存在しないファイル `External\SphereCast\Include\SphereCast.h` の登録が残っている（ビルドには影響しない）。
- 外部ライブラリの Tracy / DirectXTex、`imgui_demo.cpp` は、今のエンジンからは使われていない。
- `GraphicsDevice.cpp` の `Present(0, 0)` は VSync オフだが、横のコメントは「VSync ON」のまま。
- **描画まわり**
  - `ShaderComponent` は Entity 単位。サブメッシュごと（鎧だけ金属、など）には分けられない。
  - glTF のメタリック／ラフネスは読んでいない。手持ちのモデルの値はほぼ変換ツールの既定値（0.2 / 0.8、または未指定）なので、読んでも部位の差は出ない。
  - 金属の映り込みは空だけ（周りの物は映らない。屋内でも空が映る。影の中でも暗くならない）。空の色を2か所に書いている。
  - 背面法のアウトラインは、太さがワールドの長さなので遠くで細くなる。角で法線が分かれているメッシュ（立方体）は角で線が途切れる。スケールがマイナスの Entity は塗りつぶされる。
  - エッジ検出の線にアンチエイリアスは無い。色の違いは見ていない。面ごとに法線が分かれたローポリは線が多く出る。
  - Toon の陰は2色だけ。`toonThreshold` を 0.5 より下げるなら `receiveShadow` を外す（自分の影で境目がガタつく）。
  - ライティングは LDR・ガンマ補正なしのまま。強さを上げすぎると白く飛ぶ。
  - ライトとポストプロセスの設定は保存されない。
  - `ViewportPanel::Initialize` は各シェーダの `Initialize` の戻り値を見ていない（失敗しても起動は続き、その部分が描かれないだけ）。
  - `EngineSkybox.cpp` の HLSL の文字列に日本語コメントが1行残っている（今は問題なく動いている）。
- `Data/Sprite/IBL/` に IBL 用の DDS が7つある（約 285MB、Git 管理下）が、**使っていない**。中身は屋外の実写 HDRI から作った正しい一式（キューブ 1024²・RGBA16F・ミップ付き、`specular_pmrem` は事前計算済み）。本格的な PBR をやるなら使えるが、Toon 路線とは絵が合わない。布用の3つ（`sheen_pmrem` / `lut_charlie` / `lut_sheen_e`）は使い道がない。
- ヒットボックスは体の中心線に固定。ボーン（頭など）に追従しない。
- スキンドのキャラは、影用と本描画用で姿勢の計算を1フレームに2回している。
- アニメは1クリップの再生だけ。ブレンド、状態の切り替え（待機／走り／攻撃）は無い。アニメの別ファイルは1つまで。
- NavGrid の範囲と、シャドウマップの範囲が固定（原点中心）。
- プレイヤーの移動・攻撃は `PlayerComponent` 1つに全部入っている（キャラごとの差は未対応）。
- Play中にコンポーネントを足した／消した結果は、編集用Worldにしか入らない。
- Prefab の Apply / Revert は Undo できない。

## 10. これからやる予定（セッションを分けて進める）

- ~~本格的なシェーダ~~ → **一区切り済み**（共通化、`ShaderComponent`、スペキュラ・リム、Toon、背面法アウトライン、テクスチャ単純化、メタリック／ラフネス、ポストプロセスとエッジ検出の輪郭線）。残っている候補：
  - 影の範囲をカメラに追従させる（今は原点から ±15m 固定）
  - ライトとポストプロセスの設定を保存する
  - 部位ごとの材質（`ShaderComponent` をサブメッシュ単位に）
  - HLSL を `Data/Shader/*.hlsl` に出して実行時に読む（ホットリロード）
- アニメーション（ブレンド、ステートマシンなど）
- 当たり判定（ボーン追従のヒットボックスなど）
- プレイヤーのキャラごとの攻撃方法・移動方法
