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
4. **新しいファイル名は、既存（旧コードを含む）と重複させない。**
   - 同名の `.cpp` があると obj が衝突する（MSB8027 / LNK4042 / LNK2019）。
   - 提案前に `Source/` 全体を検索して確認する。衝突しそうなら `Engine` を頭に付ける（例：`EngineSkybox`、`EngineShadowMap`、`EngineNavGrid`）。
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
- 説明は日本語。コード中のコメントも日本語でよい。
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

新しいエンジンは `Source/Engine/`（名前空間 `uel`）。
`Source/` 直下の `AI` `Camera` `Core` `GameObject` `Graphics` `Physics` `Scene` `UI` は**旧コード**で、参照用に残してある（ビルドには入るが使っていない）。旧コードは触らない。

```
Source/Engine/
  Core/          Engine（メインループ、Scene管理、Play開始/停止/Scene遷移）、Main
  Platform/      Window
  Graphics/      GraphicsDevice, RenderTexture, Mesh, MeshShader, EngineSkybox,
                 EngineShadowMap, SkinnedRenderModel, SkinnedMeshShader, SceneLighting
  Assets/        GltfMeshLoader, MeshCache, GltfSkinnedLoader, SkinnedModelCache
  Animation/     Skeleton, AnimationClip, SkinnedModel, AnimationPose（ポーズ評価）
  Scene/         EntityId, Entity, World, Component,
                 Transform / MeshRenderer / HP / SkinnedMesh / PrefabInstance の各Component
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
4. 各パネル（Hierarchy / Inspector / Asset Browser / Environment / Animation / NavGrid）
5. Prefab の Apply があれば全Sceneへ反映
6. `ViewportPanel::RenderScene`：シャドウパス（静的 → スキンド）→ 空 → 静的メッシュ → スキンドメッシュ

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
- **schemaVersion は現在 10。** ファイルの版に存在する項目だけを読む方式なので、古いファイルもそのまま読める。

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

- Prefab のインスタンスが個別に持つのは **名前と Transform だけ**。それ以外は Scene を読むたびに Prefab の内容で作り直す（Prefab が見つからない場合だけ、Scene に保存した内容で復元）。
- 保存されない項目（今のところ）：ライト設定、Opponent の攻撃系の値、Player の各値、SkinnedMesh の再生位置。

---

## 8. 描画・数学の決まり

- 左手系、Y が上。回転はオイラー角（度）。ワールド行列は `S * R * T`。
- DirectXMath の**行ベクトル形式**。シェーダへは `XMMatrixTranspose` して渡し、HLSL では `mul(ベクトル, 行列)`。
- glTF の行列は `m[i][j] = data[i * 4 + j]` で読む（これで行ベクトル形式として正しくなる）。
- スキニング：`palette[i] = inverseBind[i] * globalPose[i]`。ボーンは最大 128。4ボーンまで。
- `SkinnedVertex` は 80 バイト（position 0 / normal 12 / color 24 / uv 40 / bones(uint×4) 48 / weights 64）。
- シェーダは C++ 内の文字列を `D3DCompile` で実行時にコンパイル（`vs_5_0` / `ps_5_0`）。
- スロット：`b0`＝オブジェクト＋ライト、`b1`＝ボーン、`t0`＝テクスチャ、`t1`＝シャドウマップ、`s0`＝通常、`s1`＝影用の比較サンプラ
- シャドウマップ：2048、平行投影 30×30（**原点を中心に約 ±15m だけ影が出る**）、バイアス 0.0025、3×3 PCF
- ライティングは「アンビエント＋平行光×影」のシンプルな形（`MeshShader` と `SkinnedMeshShader` で同じ式）
- カメラ：FOV 60度、near 0.1 / far 100
- スキン付きGLB：パーツごとに分かれたモデル（複数の skin+mesh ノード）に対応。アニメが別ファイルの場合は `animPath` に指定し、**ボーン名**で対応付ける。

---

## 9. 既知の制限・気づいている問題

- `MeshShader` の通常サンプラ（`s0`）が**作られないまま**バインドされている（既定の設定で動いているだけ）。シェーダを作り直すときに直す。
- `GraphicsDevice.cpp` の `Present(0, 0)` は VSync オフだが、横のコメントは「VSync ON」のまま。
- ヒットボックスは体の中心線に固定。ボーン（頭など）に追従しない。
- スキンドのキャラは、影用と本描画用で姿勢の計算を1フレームに2回している。
- アニメは1クリップの再生だけ。ブレンド、状態の切り替え（待機／走り／攻撃）は無い。アニメの別ファイルは1つまで。
- NavGrid の範囲と、シャドウマップの範囲が固定（原点中心）。
- プレイヤーの移動・攻撃は `PlayerComponent` 1つに全部入っている（キャラごとの差は未対応）。
- Play中にコンポーネントを足した／消した結果は、編集用Worldにしか入らない。
- Prefab の Apply / Revert は Undo できない。

## 10. これからやる予定（セッションを分けて進める）

- 本格的なシェーダ
- アニメーション（ブレンド、ステートマシンなど）
- 当たり判定（ボーン追従のヒットボックスなど）
- プレイヤーのキャラごとの攻撃方法・移動方法
