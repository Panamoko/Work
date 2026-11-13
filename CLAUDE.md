# UE-Like Engine（就職作品）作業ガイド

**協力型 PvE FPS**（Left 4 Dead 2 の協力プレイ ＋ Overwatch のロール制）を、自作の UE ライクなエンジン（C++17 / DirectX11）で作るプロジェクト。2026-11-09 に「Overwatch 風 FPS」から方針を変えた（0章）。
このファイルは、別セッションでも同じ前提で作業するための「ルール」と「現状」のまとめ。
将来、AI の挙動を本格的にするために AI Editor（Behavior Tree・Utility AI・AI Perception・Pathfinding・EQS・AI Debugger）を入れる計画がある（12章。2026-11-13）。

---

## 0. 作っているゲーム（協力型 PvE）

**基準の資料**：`C:\Users\2240192\Desktop\Claude用_協力型PvE_FPS_プロジェクト共有資料.docx`（2026-11-09）。ゲームの仕様・ロードマップ・未決定事項はこの資料に従う。資料の「既存エンジンの現状」は AI 共有用の資料から写したものなので、**実装の前に必ず実際のコードで確かめる**。

### 決まっていること
- **プレイヤー1人＋AI 仲間2人**（オンラインではない）。プレイヤーが **Tank / DPS / Support** から1つ選び、残り2ロールを AI が担当。ステージ中は変えない。
- FPS 視点（TPS は必須にしない）。1ステージは通常約10分、苦戦時15分程度。
- **ウェーブ制**：ウェーブの敵を全滅させると次へ、最後にボス戦。
- **武器**：ロールごとの専用武器。弾は実質無限だが、マガジン単位でリロードする。今のヒットスキャンの射撃を使い回し、武器ごとのダメージ・連射間隔・拡散・装弾数・リロードを足す。
- **アビリティ**：通常アビリティ1つ（クールダウン制）。ULT は通常より少し強い程度で、ダメージ・回復でゲージがたまる案。**先に通常攻撃と通常アビリティでロールの差を作り、ULT は後回しでよい**。
- **ダウン・蘇生**：HP 0 で Downed（移動・攻撃不可）。プレイヤー・AI・AI 同士で蘇生できる。制限時間を過ぎると Dead。3人とも戦えない、またはプレイヤーがダウンしたまま時間切れでゲームオーバー。**蘇生と通常の回復は別の仕組みにする**。
- **回復**は Support のアビリティが基本（マップの回復アイテムは主にしない）。AI の Support は味方の HP や危険度で回復する相手を選ぶ。
- **ボス**：通常・突進・特殊攻撃が目標（最初は少数のパターンでよい）。行動を「予備動作 → 攻撃 → 硬直」に分け、Tank が引き受け・DPS が攻撃・Support が回復、という分担を感じられるようにする。
- **AI 仲間は段階的に**：Phase 1 追従・攻撃・Support の回復 → 2 蘇生・危険回避 → 3 ロール別の連携 → 4 状況判断 → 5 プレイヤーの「集合」指示（将来）。
- **見せ場**は AI 仲間の行動・連携と、ゲームとしての完成度・遊びやすさ。自作エンジンは技術的な強みとして補足する。採用担当向けに 3〜5 分の短いデモ（ロール選択、AI の追従・攻撃・回復・蘇生、ボス戦の連携）を用意する。
- キャラの見た目は未定（人型・ロボット・スタイライズを固定しない）。

### ロードマップ（資料の 9章）
| 段階 | 内容 | 終わりの目安 | 状況 |
|---|---|---|---|
| 0 現状確認 | Player / Opponent / HP / Spawner / GameRule / NavGrid のコードを確認 | 拡張する所と依存関係が分かる | **済み**（2026-11-09。下の「コードを調べて分かったこと」） |
| 1 3人チーム | ロール選択、プレイヤー＋AI 2人、AI の追従 | どのロールでも3人そろう | **済み**（2026-11-13）。1-1 AI 仲間1体の追従 → 1-1b AllyComponent の保存（版 20）→ 1-2 陣営（a 味方撃ちを止める・b 敵が AI 仲間も狙う・c AI 仲間の戦闘不能）→ 1-3 ロール選択画面（版 21）→ 1-4 Stage 開始時に3人を配置（ロールの Component・PartySetup・名札。版 22） |
| 2 基本の AI 戦闘 | AI の攻撃、Support の回復 | 追従・攻撃・回復が動く | 作業中。2-1 射撃を共通の処理に（`Game/WeaponTrace`。**済み**）→ 2-2 AI 仲間の攻撃（a 狙う・撃つ：渡し済み → b 弾の線の表示 → c Inspector と保存）→ 2-3 Support の回復（線でつないで少しずつ） |
| 3 武器 | ロール専用武器、リロード | 3ロールの戦い方が違う | |
| 4 ダウン・蘇生 | 相互蘇生、制限時間、復帰時の HP | プレイヤーも AI も蘇生できる | |
| 5 ウェーブ・勝敗 | 全滅で次のウェーブ、ボス、クリア・ゲームオーバー | Title から Result まで一周 | |
| 6 アビリティ | 通常アビリティ、クールダウン、ULT | ロールの能力が戦闘に出る | |
| 7 ボス・敵 | 攻撃パターン、敵の種類 | 役割分担でボスを倒せる | |
| 8 UI・演出・調整 | HUD、音、エフェクト、難易度 | 説明なしで遊び方が分かる | |
| 9 AI の高度化 | 蘇生の判断、危険回避、連携、集合 | AI を仲間と感じられる | AI Editor（BT・Utility・Perception・EQS・Debugger）の計画は12章。いつ始めるかは未定 |

### 決めた設計
- **「誰が操作するか」と「何ができるか（ロール）」を別の Component に分ける**（2026-11-09 決定）。
  - 操作する側：`PlayerComponent`（プレイヤーの入力）、`AllyComponent`（AI 仲間の判断。全ロール共通。追従・狙う敵の選択・蘇生など）
  - ロール：`TankComponent` / `DPSComponent` / `SupportComponent`（そのロールで「できること」＝武器の値・アビリティの動作だけ。「いつ使うか」は操作する側が決める）。作るのは段階 3（武器）と 6（アビリティ）
  - 組み合わせ：プレイヤーの DPS ＝ `PlayerComponent` ＋ `DPSComponent`、AI の Support ＝ `AllyComponent` ＋ `SupportComponent`。3ロール × 2操作を 3＋2 個の部品で作れる。
  - 現場での対応：UE の PlayerController / AIController（同じ Pawn をどちらも操作できる）と、Gameplay Ability System の Ability。
  - AI のロールごとの判断の違い（Tank は前に出る、Support は HP の低い味方を優先、など）は、`AllyComponent` が付いているロールの Component を見て変える（AI の Phase 3「役割連携」。ロードマップでは段階 9）。
- **陣営は「操作する側」から決める**（1-2。`Game/Team.h`。ヘッダだけ）。`GetTeam(e)`：Player・Ally を持てば `Players`、Opponent を持てば `Enemies`、どれも無ければ `None`。`GetAttitude(a, b)`：Friendly / Neutral / Hostile。UE の `FGenericTeamId`・`ETeamAttitude`（陣営は Controller が持つ）に合わせた。Entity ごとに変えたくなったら `TeamComponent` を足し、`GetTeam` がそれを先に見る（呼ぶ側は変わらない）。
  - **味方に当たった弾は、そこで止まる**（ダメージ・ヒットマーカーなし。奥の敵に届かない）。ユーザーが選んだ（2026-11-10）。戦闘不能の味方は弾を止めない。
  - HP 0 でその場で消えるのは陣営 `None` の物（的・箱）だけ。敵は自分で倒れてから消え、味方は消えない。
  - **敵の狙い**：HP が残っている敵対陣営の中で、いちばん近い相手。0.5 秒ごとに選び直し（`retargetInterval`）、今の相手より 1m 以上近いときだけ乗り換える（`retargetMargin`）。HPComponent の無い物は狙わない。
  - **AI 仲間の HP 0 は、その場で戦闘不能**（ダウン・蘇生ができるまでの仮。ユーザーが選んだ）。止まって `Dead` をアニメに渡し、敵に狙われない。HP が戻れば動き出す。ゲームオーバーの判定はまだプレイヤーの HP 0 だけ（段階 4 で直す）。
- **ロール**（1-3。`Game/Role.h`）：`enum class Role { Tank, DPS, Support }`（並び順を変えない）、`RoleName` / `RoleDescription`。選んだロールは `GameFlow::playerRole`（UE の GameInstance に当たる）。Play を始めるたびに既定の **DPS** に戻す（`Engine::StartPlay`）。
  - ロール選択の画面は `ScreenUIComponent` の `roleSelect`（オンでボタン1つの代わりに3つのロール。1 / 2 / 3 キー、Enter は今のロール）。
  - 流れ：Title →(START)→ **RoleSelect** →(ロールを選ぶ)→ Stage →… → Result →(RETRY)→ **RoleSelect**（RETRY でロール選択へ戻る。ユーザーが選んだ）。
- **3人の配置**（1-4）：ロールの Component は `RoleComponent`（`GetRole()` だけの親）と `TankComponent` / `DPSComponent` / `SupportComponent`。今は中身が無く、保存もしない（Play 開始時に付ける）。`PartySetupComponent`（UE の GameMode に当たる）を Stage の GameRule の Entity に付けておくと、Play の最初のフレームに1回だけ、プレイヤーに選んだロールを付け、残り2ロールの AI 仲間を `allyPrefab`（既定 `Data/Prefabs/Ally.prefab.json`）から作ってプレイヤーの左後ろ・右後ろ（`spawnBack` 2m、`spawnSide` 1.5m）に置き、名前を「Ally (Tank)」のようにする。AI 仲間は Prefab から出す（Scene に置かない）。ユーザーが選んだ。
  - AI 仲間の頭の上に名札（ロールの名前をロールの色で。Tank 青・DPS 橙・Support 緑、戦闘不能は灰色）。ユーザーが選んだ。Prefab が読めないと HUD に `PartySetup: ally prefab not found`。
- **射撃は共通の処理**（2-1。`Game/WeaponTrace.h/.cpp`）：`TraceShot(ctx, 位置, 向き)`（UE の `LineTraceSingleByChannel`。いちばん手前の当たり＝`ShotHit`。撃った本人・倒れている最中の敵・戦闘不能の味方は素通り）と `ApplyShotDamage(ctx, 当たり, ダメージ)`（UE の `ApplyPointDamage`。味方・HP の無い物には入らない。戻り値はダメージが入ったか）。プレイヤーも AI 仲間もこれで撃つ。
- **AI 仲間の攻撃**（2-2a）：0.3 秒ごとに射程（25m）の中のいちばん近い敵を選ぶ（今の敵より 1m 以上近いときだけ乗り換え）。乗り換える前と撃つ直前に `TraceShot` で見えるか（間に壁・味方がいないか）を調べ、見えない敵は 1 秒候補から外す。新しい敵には 0.3 秒構えてから撃つ。34 ダメージを 0.8 秒ごと、2 度のずれ（10m でほぼ全部当たり、20m で約 6 割）。追いかけながらでも撃ち、止まっているときは敵を向く。強さはユーザーの答え「プレイヤーより少し弱い」に合わせた。武器の値は段階 3 でロールの Component へ移す。

### コードを調べて分かったこと（新しいゲームで効いてくる所）
- **「プレイヤー＝`PlayerComponent` を持つ最初の Entity」という前提が5か所**：カメラ（`ViewportPanel::DrawUI`）、HUD（同 オーバーレイ）、敵が狙う相手（`OpponentComponent`）、負けの判定（`GameRuleComponent`）、Play 開始時の一人称の設定（`Engine.cpp`）。**AI 仲間には `PlayerComponent` を付けない**。
- **敵はプレイヤーしか狙わない**（追跡も攻撃も）。AI 仲間も狙わせるには陣営（味方／敵）の考え方が要る。
- **プレイヤーの射撃は「Hitbox を持つ物すべて」に当たる**（味方撃ちが起きる）。また、**敵以外で HP が 0 になった物はその場で `DestroyEntity` される**（`PlayerComponent.cpp` の射撃の処理）。AI 仲間に HP・Hitbox を付ける前に、ここを直す（ダウンの仕組みとぶつかる）。
- **プレイヤーの HP が 0 で即ゲームオーバー**（`GameRuleComponent`）。ダウンを入れるときに変える。
- **射撃の処理は `PlayerComponent::Update` に直接書いてある**。AI も同じ撃ち方をするなら、先に共通の処理として外に出す。→ 2-1 で `Game/WeaponTrace` に出した。
- **`SpawnerComponent` は「全滅で次のウェーブ」までできている**（資料の「追加が必要」は誤り）。足りないのは、ウェーブごとに違う敵を出すこと（今は1つの Spawner に Prefab 1種類）と、最後のボス。
- `HPComponent` は `hp` と `maxHp` だけで、敵も使う。ダウン・死亡・蘇生の状態は別の部品に分けるのが安全。
- 選んだロールは `GameFlow`（Scene をまたいで残る）に持たせて、ロール選択の画面から Stage へ渡せる。
- 経路探索のグリッドは Play 中に毎フレーム作り直している（原点中心の 60m 四方で固定）。AI の追従には使えるが、ステージを広くするなら直す。

### 決まっていないこと（資料の 12章）
キャラの見た目、各ロールの武器・能力の中身、通常の敵の種類と数、蘇生時間・ダウンの制限時間・復帰時の HP、蘇生中に攻撃されたときの扱い、Dead からの復帰（ステージ終了まで戻らない案）、装弾数・リロード時間、ULT のゲージの量と増え方、ウェーブ数・敵の数・休憩時間、ボスの攻撃の中身、集合の指示（将来）、TPS（必要になったら）。**決まっていないことは勝手に決めず、案を出してユーザーに選んでもらう。**

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

### 設計・提案の方針
- **企業（ゲーム会社）で実際に使われている技術・設計を使う。** 自己流の仕組みより、業界で通じる手法（UE や Unity にある考え方、よく知られた設計パターンなど）を選び、「実際の現場ではこう呼ばれている／こう作られている」も添える。
- **動作に支障が出ない範囲で、最初から軽量化を含んだコードを出す。** 毎フレーム無駄な処理をしない（毎フレームの検索・文字列の生成・メモリ確保を避ける、結果を使い回す、間隔をあけて処理する、など）。ただし読みやすさや正しさを崩してまで細かく最適化しない。軽量化した所と理由は説明に書く。
- **設計の理由・良い点・悪い点・制限を説明する。** ユーザー本人が面接で設計の意図を説明できることを重視する。
- **実装済み・再利用する物・新しく作る物・決まっていない物を区別して書く。**
- **大きい機能は、最初に全体の計画と小さな実装の順番を示し、ユーザーが合意してからコードを出す。** 一度に大量のコードを出さない。
- 最初から高度な AI や複雑なボスを作らない。3人チームで戦闘が成り立つ最小の形を完成させてから広げる。

### 複数のセッションで作業するときの決まり
ユーザーは分野ごとにセッションを分けて、**同じフォルダ（同じ作業ツリー）**を触っている（2026-11-09 時点：ゲームの「リポジトリ確認」、「Shader コーディング」、「Animation コーディング」）。セッションは、ほかのセッションでの変更を自動では知らない。CLAUDE.md を読むのもセッションの開始時だけ。

- **担当のフォルダ**
  | セッション | 主に触る所 |
  |---|---|
  | ゲーム | `Components/`、`Game/`、ゲーム用の Prefab・Scene |
  | Shader | `Shader/`、`Graphics/` |
  | Animation | `Animation/`、`Systems/AnimationSystem`、アニメの編集画面（`AnimEditorPanels` と `AnimGraph*`）、`Serialization/AnimGraphSerializer` |
- **共通のファイル**（`InspectorPanel.cpp`、`StageSerializer.cpp`、`ViewportPanel.cpp`、`Engine.cpp`、`EngineConfig.h`、`CLAUDE.md`）を触る作業は、**同時に2つ進めない**。
- **作業を始める前に、今のファイルを読み直す**（`git status` と `git log` も見る）。セッションの途中でも、ユーザーがほかのセッションの変更を貼っていることがある。覚えている中身を前提に指示を書かない。
- **1つの変更を貼ってもらったら、すぐコミットしてもらう**（コミットはユーザーが行う）。ほかのセッションの作業に移る前にコミットされていると、どの変更がどこから来たか分かり、戻すこともできる。
- **保存の版**（Scene の `kSchemaLatest`、Environment の `kEnvSchemaLatest`、グラフの `kAnimGraphVersion`）**を上げるのは、一度に1つのセッションだけ。** 上げる前に今の値をファイルで確かめ、上げたらコミットしてもらってから次の作業へ。2つのセッションが同じ番号を使うと、違う項目が同じ版になり、古いファイルの読み込みが壊れる。
- **CLAUDE.md は、直す直前に読み直す。** 各セッションは自分の章だけを直す（ゲームは0章・5章・7章・12章（AI Editor）、Animation は6章、Shader は9章。共通の章（1〜4、8、10、11）は、必要な行だけを足す）。ほかのセッションが書いた内容を消さない。
- **ほかのセッションでできた物に気づいたら**（CLAUDE.md に無いファイルや機能、未コミットの変更など）、前提をユーザーに確かめ、CLAUDE.md の該当の章に足す（2026-11-09 に、LayeredBlend・エイムオフセットが CLAUDE.md に書かれていなかったことがある）。
- ユーザーがほかのセッションに戻るときは、最初に「CLAUDE.md の〇章を読み直して。前回から〇〇が変わった」と伝えてもらうとよい。

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
| `Illegal character in shader file`・`unexpected #endif`・「型 〇〇 を解決できません」 | VS で `.hlsl` を新規作成して中身を貼り付けたら、UTF-8 BOM 付き（や Shift-JIS）で保存された | コードは本文に全部載せる（ユーザーは VS で追加してコピペする）。HLSL は「貼った後、エンコード付きで保存 → UTF-8 シグネチャなし」まで手順に書き、確かめ方も添える（9章） |
| `'lod' が宣言されていません`・`no matching 1 parameter function`（M-4） | 「`return ...` の1行を差し替え」と指示したが、すぐ下に似た `return` 行があり、違うほうを差し替えた | 似た行が近くにあるときは1行の差し替えにせず、**関数ごと**差し替える |
| 「上半身のレイヤーは無い」と思い込んだ | ほかのセッション（Animation）で作った機能が、CLAUDE.md に書かれていなかった | 作業の前に実際のファイルを読む。新しい物に気づいたら CLAUDE.md に足す（2章「複数のセッションで作業するときの決まり」） |
| 保存して起動し直したら、AI 仲間の AllyComponent が消え、PlayerComponent と HitboxComponent が戻った（1-1b） | Player を複製した Entity が Prefab のインスタンスのままだった。インスタンスは読み込みのたびに Prefab の内容で作り直される（8章） | Prefab のインスタンスの Component を変える手順では、**保存の前に「リンクを解除」**（または「Prefab に反映」）を書く。確かめ方：Inspector に青い字の `Prefab：...` が出ていないこと |
| 「ROLE: …」が撃った直後しか出ない（1-3a） | 「`if (pc && ...) {` の行の**直前**に足す」と書いたが、`{` の**後ろ**（ブロックの中）に貼られた | `{` で終わる行の前後に足す指示では、**足した後の形（前後の行ごと）**を見せる。ファイルの変更が2か所以上なら全体で渡す |
| 渡したファイルの日本語（コメントと画面の文字）が全部壊れた（1-3b の `ViewportPanel.cpp`） | Shift-JIS のファイルを**メモ帳で開いて**中身をコピーした。今のメモ帳は BOM の無いファイルを UTF-8 として開くので、化けたまま貼られた | **手を入れたファイルは UTF-8（BOM 付き）で渡す**（3章）。メモ帳でも VS でも正しく開ける |

---

## 3. 環境・前提

- C++17 / DirectX 11 / Visual Studio 2022 (v143) / x64 / `Game.sln`・`Game.vcxproj`
- 外部ライブラリ（`External/`）：imgui-docking（**1.80 WIP**）、ImGuizmo、tinygltf、cereal、DirectXTex、Tracy
- **文字コードはファイルごとに違う。手を入れないファイルは、今の文字コードのまま保つ。**
  - **新しいファイルと、手を入れて渡すファイルは UTF-8（BOM 付き）・CRLF にする**（2026-11-10 にユーザーと決めた）。ユーザーは渡されたファイルをメモ帳で開いてコピーし、VS に貼ることがある。今のメモ帳は BOM の無いファイルを UTF-8 として開くので、Shift-JIS の日本語は壊れたまま貼られる（1-3b の `ViewportPanel.cpp` で起きた）。BOM 付きならメモ帳でも VS でも正しく開け、MSVC もそのままビルドできる（`u8"..."` も、普通の文字列も、今までと同じに動く）。
  - VS で新しく作って貼ったファイルは Shift-JIS で保存されることがある（`AllyComponent.*`、`Role.h` がそうなった）。次に手を入れるときに UTF-8 BOM にする。
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
  SkyColor.hlsli       空のグラデーション SkyColor(dir)（画像が無いときの代わり）、空の回転 RotateSkyDir(dir, cs)、HDR → 画面の色 HdrToScreen(hdr, exposure)
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
                 Transform / MeshRenderer / SkinnedMesh / Animator / Shader / HP / Player / Ally / Opponent /
                 Hitbox / NavObstacle / Spawner / GameRule / ScreenUI / PrefabInstance /
                 Role（ロールの親）/ Tank / DPS / Support / PartySetup
  Scene/         EntityId, Entity, World
  Game/          GameContext, InputState, GameFlow（Component が使う、ゲーム側の受け渡し用の型）,
                 Team（陣営。ヘッダだけ）, Role（ロール。ヘッダだけ）, WeaponTrace（弾の線とダメージ。プレイヤーと AI 仲間で共用）
  Animation/     Skeleton, AnimationClip, SkinnedModel, AnimationPose（ポーズの計算・ブレンド）,
                 AnimParams, AnimNode（基底・AnimNodePtr・AnimClipNode）, AnimBlendSpaceNode,
                 AnimStateMachineNode, AnimLayeredBlendNode（骨ごとに2つの姿勢を混ぜる）,
                 AnimMontage, AnimAimOffset（視点の角度で背骨を曲げる）
  Systems/       RenderSystem（描画）, AnimationSystem（毎フレームの姿勢の計算）
  Graphics/      GraphicsDevice, RenderTexture（シーン用／色だけ の2種類）, Mesh,
                 MeshShader, SkinnedMeshShader, SkinnedRenderModel,
                 ShaderCommon（b0 の C++ 側の形 ObjectCB と FillShading）, ShaderLoader（.cso の読み込み）,
                 ShadingParams（描き方のデータ）,
                 EngineSkybox, EngineShadowMap, SceneLighting, PostProcess, PostProcessSettings,
                 DdsTexture（DDS の読み込み。自作）, EnvironmentMap（空・映り込み用のキューブマップと LUT。Data/Sprite/IBL_PureSky）,
                 ShaderHotReload（実行中に .hlsl を保存するとコンパイルし直す。作業中：コミット済みだが、2026-11-09 時点では Engine からまだ呼ばれていない）
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
| Player | 操作キャラ。**これを持つEntityがプレイヤー**（AI 仲間には付けない。0章） | `moveSpeed, eyeHeight, eyeForward, groundY, radius`。アニメへ `Speed / MoveX / MoveY / Grounded / Jump / Attack / Dead / AimPitch` を渡す |
| Ally | AI 仲間の「頭」（UE の AIController。全ロール共通の判断）。追従：PlayerComponent を持つ Entity を A* の経路で追い、`followDistance` まで近づいたら止まる。`resumeDistance` より離れたら動き出す（ヒステリシス）。経路は `repathInterval` ごと、しかも相手が 0.5m 以上動いたときだけ計算し直す。地面の判定は動いたとき・空中にいるときだけ（1-1）。HP 0 で戦闘不能（止まる。`IsIncapacitated()`。1-2c）。攻撃：射程の中のいちばん近い見える敵を撃つ。止まっている間は敵（いなければプレイヤー）を向く（2-2a。0章） | `moveSpeed, followDistance, resumeDistance, repathInterval`（4つとも保存する）。攻撃の `attackRange, attackDamage, attackInterval, aimSpread, aimHeight, searchInterval, reactionTime`（まだ保存しない）。アニメへ `Speed / Dead / Attack` を渡す（`Dead` は変わったときだけ書く）。HUD 用に `GetTarget()`、最後の弾の線 `GetShotFrom/To/Time()` |
| Role / Tank / DPS / Support | ロール（「何ができるか」。UE の GAS の Ability）。`RoleComponent::GetRole()`。今は中身なし（段階 3・6 で武器・アビリティ）。保存しない（PartySetup が付ける） | なし |
| PartySetup | Stage の開始時に3人をそろえる（UE の GameMode）。0章「3人の配置」 | `allyPrefab, spawnBack, spawnSide`（保存する）。`HasError()` |
| Opponent | 敵。NavGrid の経路で、HP の残っている敵対陣営（プレイヤー・AI 仲間）のいちばん近い相手を追い、近づいたら攻撃（1-2b）。HP 0 で倒れてから消える。撃たれるとモンタージュ `HitReact` でひるむ（定義が無ければひるまない） | `moveSpeed, stopRange, attackRange, attackDamage, attackInterval, attackHitDelay, attackMoveLock, deathDelay`。アニメへ `Speed / Attack / Dead` を渡し、通知 `Hit` でダメージ |
| Hitbox | 当たり判定（胴体カプセル＋頭の球） | `bodyBottom, bodyTop, bodyRadius, headHeight, headRadius, headMultiplier` |
| NavObstacle | NavGrid を塞ぐ箱 | `halfX, halfZ` |
| Spawner | Prefab から敵をウェーブで湧かせる | `prefabPath, waveCount, baseCount, countIncrease, spawnInterval, firstDelay, waveDelay, spawnRadius` |
| GameRule | 勝敗判定 → Scene遷移 | `winScene, loseScene, endDelay` |
| ScreenUI | Title / Result / ロール選択の画面 | `title, subtitle, buttonText, nextScene, showResult, roleSelect`（オンで3つのロールのボタン。1-3b） |
| PrefabInstance | どの Prefab から作ったか | `path` |

### 今のゲームの仕様（実装済みの部分。新しい方針は0章）
今の実装は「1人のプレイヤーが、1種類の敵のウェーブを倒す」形。0章の協力型 PvE へ、ロードマップに沿って作り変えていく。
- 操作：右ドラッグで視点、WASD 移動、Space ジャンプ、左クリックで射撃
- 射撃：基本ダメージ 34。ヒットボックスの頭は倍率（既定2倍）。壁の向こうには当たらない。ヒットボックスが無くHPを持つ物はメッシュで判定。倒れている最中の敵には当たらない。
- 敵：0.25秒ごとに A* で経路を再計算。攻撃は `attackInterval` ごと。ダメージは、グラフに通知 `Hit` があればその瞬間、無ければ `attackHitDelay` 秒後。HP が 0 になったら `Dead` を立て、`deathDelay` 秒後に `DestroyLater`。
- NavGrid（Play中）：中心 (0,0,0)、セル 1m、60×60 で固定。塞ぐのは NavObstacle を付けたEntityだけ。
- 流れ：Title →(START)→ RoleSelect →(ロールを選ぶ)→ Stage →(全ウェーブ撃破 or 死亡)→ Result →(RETRY)→ RoleSelect
- HUD（ImGui。`ViewportPanel` のオーバーレイ）：照準、ヒットマーカー、左上に「ROLE: …」、左下にプレイヤーの HP、その上に AI 仲間の HP（緑。名前と数値）、上の中央にウェーブ、AI 仲間の頭の上に名札。明るい背景でも読めるよう、文字に黒い影を付けている（`shadowText`）
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
  3. エイムオフセットで背骨を曲げる（`an->aim.enabled` のときだけ）
  4. 一人称なら `fpHideBone` を潰す
  5. 通知の記録（`events`、表示用の `lastEvent`）
  6. 行列にして `boneGlobal` / `palette` へ
- Trigger パラメータは、グラフの計算が終わったら毎フレーム 0 に戻す（1フレームだけ有効）。

### グラフ（ノードの木）
- ノードの基底は `AnimNode`（`Update` で時間を進め、`Evaluate` で姿勢を出す。`Clone` で子ごと複製）。ノードの持ち主は `AnimNodePtr`（コピーすると中身を Clone する。だから Animator の `Clone()` は普通のコピーで済む）。
- ノードの種類（`TypeName()` の文字列は保存にも使う。変えない）
  - **Clip**（`AnimClipNode`）：クリップを**名前で**指定して再生。`loop`、`speed`、`speedParam`（Float パラメータを速度に掛ける）、`notifies`（時刻と名前）
  - **BlendSpace1D**（`AnimBlendSpace1DNode`）：Float パラメータ1つを軸に、近い2つのクリップを混ぜる。2つは**同じ割合の位置（位相）**で再生する（歩きと走りの足がずれない）。`smoothTime` で入力をなめらかに追う
  - **StateMachine**（`AnimStateMachineNode`）：状態（中身は Clip か BlendSpace1D）と遷移。遷移は**上から順に調べて最初に成り立ったもの**を使う。条件はすべて成り立ったとき（Float は `> / <`、Bool は true / false、Trigger は立っていれば）。`from = -1` は「Any（どの状態からでも）」。`waitForEnd` は「移る前の状態のクリップが終わってから」（ループしないクリップのときだけ効く）
  - **LayeredBlend**（`AnimLayeredBlendNode`。UE の Layered blend per bone）：`basePose`（全身。移動のステートマシンなど）の上に、`blendPose`（構え・攻撃など）を `maskBone`（例：`"spine"`）から先の骨にだけ重ねる。`maskBone` が空なら全身。`weight` と `weightParam`（Float パラメータを掛ける）、境目を何段の骨でなじませるかの `maskBlendDepth`。重みが 0 のときも両方の時間を進める
    - **置けるのは根だけ**（エディタの制限）。2つの入力に置けるのは Clip / BlendSpace1D / StateMachine。
    - 通知は両方の入力から出る。`HasNotify` は子まで探す（だから `OpponentComponent` の「グラフに `Hit` があるか」は入力の中まで見る）。
    - 「再生が終わったか」（`IsFinished`）は下地（`basePose`）で判断する。
    - マスクは骨の名前・段数・骨格が変わったときだけ作り直す（毎フレームは作らない）。
- 状態の中身にステートマシンは入れられない（入れ子なし）。
- **エイムオフセット**（`AnimAimOffset`。`AnimatorComponent::aim`。UE の Aim Offset）：**ノードではなく、グラフのアセットに一緒に保存する設定**（UE ではノードだが、根に箱を置くとエディタで中のステートマシンを開けなくなるため、こうした）。視点の上下・左右の角度のパラメータ（`pitchParam` 既定 `"AimPitch"`、`yawParam`。度）に合わせて、`startBone` から `endBone` までの背骨を少しずつ曲げる。上下を向いたクリップは要らない（骨を回して作る手続き型）。`maxPitch` / `maxYaw`（既定 60 度）で制限、`invertPitch` / `invertYaw` で向きを反転。グラフとモンタージュの後にかける。プレイヤーは `AimPitch` を渡している（グラフにそのパラメータとエイムの設定が無ければ何も起きない）。
  - `enabled` の既定は false（使うときに Anim Graph の「エイム」の欄でオンにする）。
  - 回す軸は**モデルの空間**（上下は X 軸、左右は Y 軸）。骨ごとのローカルの軸の向きがばらばらなモデルでも同じ向きに曲がる。左手系なので、上を + にするため X 軸まわりは符号を逆にしている。
  - 角度は曲げる骨の本数で等分して、根元の骨から順に足していく（腰から急に折れず、背中全体で反る）。
- `CondOp` と `AnimParamType` は数値で保存している。**並び順を変えない**（増やすときは末尾）。

### パラメータ・通知・モンタージュ
- **ゲーム側はクリップを知らない。** パラメータに値を書くだけで、どのクリップを出すかはグラフが決める。
  - 書くときは `params.SetIfExists("Speed", v)`（グラフにその名前が無ければ何もしない。勝手に増やさないため）。
  - 名前は Component の定数で持つ（`PlayerComponent::kAnimSpeed` など）。Player と Opponent で `Speed / Attack / Dead` は同じ名前。
- **通知（Notify）**：Clip ノード（とモンタージュ）の時刻に名前を置くと、再生位置がそこを通ったフレームに `events` に入る。ゲーム側は**次のフレームに** `an->WasFired("Hit")` で受け取る。
- **モンタージュ**：グラフの上に1回だけ重ねて再生するクリップ（UE の Anim Montage）。定義（名前・クリップ・`blendIn`・`blendOut`・`rate`・通知、重ねる範囲の `maskBone` / `maskBlendDepth`）はグラフのアセットに保存する。`maskBone` が空なら全身、骨の名前なら上半身などだけに重ねる。ゲーム側は `PlayMontage("HitReact")` と名前で呼ぶ。下のグラフは止めずに進むので、抜けると「今の」動きに戻る。再生中に別のモンタージュを呼ぶと、今の姿勢から混ぜて切り替える。同時に再生できるのは1つ。

### グラフのアセット（`AnimGraphSerializer`）
- `Data/Anim/<名前>.animgraph.json`。中身：パラメータの一覧（今の値が初期値。Trigger は 0 で保存）、ノードの木、モンタージュの定義、エイムオフセットの設定。
- 版は `kAnimGraphVersion = 7`（1 最初 / 2 エディタ上の位置 / 3 通知 / 4 モンタージュ / 5 モンタージュの重ねる範囲 / 6 LayeredBlend / 7 エイムオフセット）。Scene と同じく「その版以降だけ読む」。
- Scene に保存するのは `graphPath` だけ（`anGraph`）。読み込み時にグラフのファイルを読む。**グラフの変更は「保存」ボタンを押すまでファイルに残らない。**
- 名前だけ（`Enemy`）を入れると `AnimGraphPathFromName` が `Data/Anim/Enemy.animgraph.json` にする。
- 今あるグラフ（2026-11-09 時点）：
  - `Enemy`：版 7。根が LayeredBlend（下地＝Idle / Run / Death のステートマシン、重ねる側＝Guard / Attack / Death のステートマシン、`spine` から先）。エイムを使っている。モンタージュ `HitReact`
  - `Players`：版 6。根が LayeredBlend（入力は2つともステートマシン）。エイムは無し
  - `Character` / `Large` / `Player1`：版 2。根がステートマシンだけ

### エディタ
- **Inspector の Animator 欄**：クリップ1つの再生、グラフのファイルの保存／読み込み、グラフの根が Clip / BlendSpace1D / LayeredBlend ならその詳細（LayeredBlend は入力の中身も入れ子で出る）、パラメータの編集。
- **Anim Graph パネル**：上にファイルの行、左にパラメータ・モンタージュ・エイム、中央にキャンバス、右に選んだものの詳細。
  - ポーズグラフ（根のノード → 出力ポーズ）。右クリックでノードの追加・置き換え・削除。StateMachine はダブルクリックで中へ。
  - 右クリックの「Layered Blend（上半身に重ねる）」で根を置き換えると、**今までの根のノードが自動で下地に入る**（重ねる範囲は、名前に `spine` を含む最初の骨が初期値）。
  - 根が LayeredBlend のときは、左に入力の箱が2つ（下地・重ねる側）出る。箱をクリックで右に中身、右クリックで入力の置き換え・削除、StateMachine ならダブルクリックで開く（上の行は `AnimGraph > 下地の State Machine` のように出る。開いているのがどれかは `g.smSlot`：0 = 根、1 = 下地、2 = 重ねる側）。
  - 「全体を表示（F）」は、箱全体が入る大きさ（最大 100%）に合わせる。別のグラフを開いたときも自動で合わせる。
  - 左の欄の「エイム」：使う／使わない、曲げ始め・曲げ終わりの骨、上下・左右のパラメータ（無ければ「作る」ボタン）、最大の角度、反転。曲げる骨の本数と今の角度も出る。
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
- Environment（ライトとポストプロセスの設定）：`Json/Environment.json`。**Scene を保存（Ctrl+S）したときに一緒に保存**し、起動時に読む。版は `kEnvSchemaLatest = 2`（1 最初 / 2 空の明るさ `skyExposure` と回転 `skyRotation`）。Scene と同じく「その版以降だけ読む」ので、版 1 のファイルは空の2つが既定値（1 / 0）になる。確認用の表示モード（`debugView`）だけは保存しない。
- Prefab：`Data/Prefabs/<名前>.prefab.json`（Entity 1個ぶん。位置は 0 で保存）
- アニメーショングラフ：`Data/Anim/<名前>.animgraph.json`（6章）
- **Scene の schemaVersion は現在 22。** ファイルの版に存在する項目だけを読む方式なので、古いファイルもそのまま読める。

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
| 20 | Ally（`hasAlly, alMoveSpeed, alFollowDist, alResumeDist, alRepath`） |
| 21 | ScreenUI の `roleSelect`（`uiRoleSelect`） |
| 22 | PartySetup（`hasParty, ptPrefab, ptBack, ptSide`） |

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
  - 新しい `.hlsl` を足す手順：ユーザーが VS で追加して中身を貼る → 「エンコード付きで保存」で UTF-8（シグネチャなし）にする → プロパティで種類を設定。
  - `.cso` が見つからないと、VS の「出力」に `[Shader] not found: Data/Shader/X.cso (...)` と出て、その部分が描かれない（空なら背景はクリア色のまま）。HLSL の文法エラーは**ビルドエラー**として出る（実行時ではない）。
  - `Data/Shader/*.cso` は Git 管理下（ビルドで上書きされる）。
- **`.hlsl` / `.hlsli` の文字コードは UTF-8（BOM なし）**。fxc で確かめた結果：
  - **BOM 付き** → `error X3000: Illegal character in shader file`。`.hlsli` に BOM があると、それを include した側で `unexpected #endif` や「型を解決できません」になる。
  - **Shift-JIS** → 日本語コメントの最後の文字の2バイト目が `\`（`表` `ソ` `能` など）だと、**次の行まで黙ってコメントになる**（エラーが出ないぶん危ない）。
  - **一覧には「UTF-8 シグネチャ付き」（＝BOM 付き。ダメ）と「UTF-8 シグネチャなし」（OK）が並んでいて、間違えやすい**（M-3 で `Common.hlsli` が BOM 付きになり `Illegal character` が出た）。手順には必ず「シグネチャ**なし**」を強調して書く。
  - **VS で新規作成して貼り付けると BOM 付きか Shift-JIS になる。** 貼った後に「ファイル → 名前を付けて保存 →『上書き保存』の▼ → エンコード付きで保存」で「Unicode (UTF-8 シグネチャなし) - コードページ 65001」を選んで保存してもらう（コードは本文に載せ、ユーザーがコピペする流れ）。
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
  - `EngineSkybox.cpp` の `SkyCB` ⇔ `Skybox.hlsli`（最後が `skyParams`）、`PostProcess.cpp` の `PostCB` ⇔ `PostProcess.hlsli`
  - `ObjectCB` の最後は `sky`（空の明るさ・回転の cos / sin）。項目を足すときは `sky` の後ろに、C++ と HLSL の両方へ
  - 入力レイアウト（`MeshShader.cpp` / `SkinnedMeshShader.cpp` / `EngineShadowMap.cpp`）⇔ 各 `VSIn` のセマンティクス。レイアウトは読み込んだ `.cso` のバイトコードで作る。
- スロット：`b0`＝`ObjectCB`（オブジェクト＋ライト＋カメラ位置＋描き方）、`b1`＝ボーン、`t0`＝テクスチャ、`t1`＝シャドウマップ、`t2`＝映り込み用のぼかした空（`specular_pmrem`）、`t3`＝映り込みの強さの表（`lut_ggx`）、`s0`＝通常（線形・繰り返し）、`s1`＝影用の比較サンプラ、`s2`＝映り込み用（線形・端で止める）。`t2`/`t3` があるかは `params.w`（`SetEnvironment` で両方そろったら 1）。ポストプロセスは別で、`b0`＝`PostCB`、`t0`＝色、`t1`＝法線、`t2`＝深度、`s0`＝ポイント・端で止める。
- **描き方のデータの流れ**：`ShaderComponent`（Scene）が `ShadingParams`（Graphics）を1つ持つ → `RenderSystem` が取り出して `Draw` に渡す → `FillShading` が `ObjectCB` に詰める。Graphics 側は Component を知らない。
- **描き方の項目を足すときに直す場所**：`ShadingParams.h` → `ShaderCommon.h`（`ObjectCB`・`FillShading`）→ `Shader/Common.hlsli`（`cbuffer ObjectCB`）→ `Shader/Lighting.hlsli`（式）→ `InspectorPanel.cpp`（Inspector）→ `StageSerializer.cpp`（版を +1）。`MeshShader` / `SkinnedMeshShader` / `RenderSystem` は触らなくてよい。

### ライティングと材質（`ShadeSurface`）
- **Lit**：`下地 ×（アンビエント＋平行光×影）` ＋ スペキュラ（GGX）＋ リムライト ＋ 映り込み（IBL）。
- **Unlit**：テクスチャ × 色そのまま。
- **Toon**：ハーフランバートを `toonThreshold` で2色に分ける（陰には `toonShadeColor` を掛ける）。シャドウマップの影も同じ「陰の色」になる。スペキュラ・リムは 0.5 を境に2値化。任意でテクスチャの単純化（周り8点との平均 → 明るさを段階に丸める。色相は保つ）。
- **metallic / roughness**（M-2・M-3 で直した。PBR の式を LDR・ガンマ空間のまま使っている）：
  - **ハイライトは GGX**（D：GGX、G：Smith / Schlick-GGX、F：Schlick のフレネル）。光の総量が roughness によらず同じなので、なめらか＝小さく強い、粗い＝大きく弱い。前の Blinn-Phong は粗いほど全体が白っぽく光っていた。拡散光が π で割っていないので、ハイライトも π 倍して釣り合わせている。roughness の下限は 0.05。
  - 反射率 `f0` は非金属 0.04、金属は下地の色。metallic で拡散（下地の色）が消える。
  - **強さ `specScale = lerp(specularStrength, 1, metallic)`**：非金属は「ハイライトの強さ」（1 が現実の強さ、0 なら出ない）、金属は必ず 1。映り込みにも同じ倍率が掛かる。
  - **映り込み（IBL、スプリットサム）**：`EnvironmentColor(r, rough)`＝`specular_pmrem` を `lod = rough × (ミップ数 − 1)` で読んで `HdrToScreen`、`EnvironmentBrdf`＝`f0 × A + B`（`lut_ggx` を `(N・V, rough)` で読む。r = A、g = B。上の行が rough 0 であることを中身で確認済み）。景色が無いとき（`params.w` = 0）は前のグラデーションの空と `f0`。
  - **Toon**：ハイライトの形は GGX の「真ん中に対する割合」を 0.5 で2値化（大きさは roughness）、粗い面は `0.0625 / α²` で暗くする（roughness 0.5 以下は最大）。金属の映り込みは景色を roughness 0.5 以上でぼかして明るさを3段に丸める（`PosterizeBrightness`）。非金属には映り込みを足さない。
- グラデーションの空（3色と式）は `SkyColor.hlsli` の1か所だけ（空の描画 `SkyboxPS` と `Lighting.hlsli` の両方が include する）。今は空の画像が読めなかったときの代わり。

### 空（`EngineSkybox` と `EnvironmentMap`）
- **空は HDR のキューブマップ**（`Data/Sprite/IBL_PureSky/skybox.dds`。1024²・RGBA16F・ミップ 11・6面）。どのフォルダを使うかは `EnvironmentMap.cpp` の `kIblFolder`（`"IBL"` にすると野原の写真）。`ViewportPanel` が `EnvironmentMap envMap` を持ち、`skybox.Render(..., envMap.Sky())` に渡す。映り込み用の `envMap.Specular()`（`specular_pmrem.dds`）と `envMap.BrdfLut()`（`lut_ggx.dds`、2D）は `shader.SetEnvironment(...)` / `skinnedShader.SetEnvironment(...)` で渡す。
- 読めなかったら `Sky()` が nullptr になり、空は前のグラデーションで描かれる（`SkyCB.camPos.w` が 0）。VS の「出力」に `[DDS] ...` と理由が出る。
- `SkyboxPS` は `HdrToScreen`（露出 → ACES の近似のトーンマップ → 1/2.2 のガンマ）で画面の色にする。HDR の値は上限が無い（曇り空は最大 5 くらい、`IBL` の太陽は約 57000）。
- **空の明るさと回転**（M-4）：`SceneLighting::skyExposure`（露出の倍率、既定 1）と `skyRotation`（Y 軸まわり、度、既定 0）。Environment パネルの「空」の欄で編集し、`Environment.json` に保存する。`SkyShaderParams(lighting)` が `(明るさ, cos, sin, 0)` を作り、空（`SkyCB.skyParams`）と映り込み（`ObjectCB.sky`。`MeshShader` / `SkinnedMeshShader` が `SetLight` で受け取る）の両方に同じ値を渡すので、空と映り込みは必ず一緒に回り、一緒に暗くなる。回転は `RotateSkyDir`（空を +θ 回す＝見る向きを −θ 回して読む）。
- **DDS は自作の `DdsTexture` で読む。** DirectXTex は使わない：`DirectXTex.lib` は /MDd（DLL 版の実行時ライブラリ）、Game は /MTd（静的版）なので、リンクすると LNK2038 になる。`DdsTexture` が読めるのは圧縮していない形式（RGBA16F / RGBA32F / RG16F / RG32F / R32F / RGBA8 / BGRA8）、2D とキューブ、ミップ付き、旧形式と DX10 のヘッダ。
- キューブの面の向きは D3D の標準（+X, −X, +Y, −Y, +Z, −Z）のまま、ワールドの向きで読む（上下は確認済み。左右が写真と鏡になっているかは未確認だが、空なので目立たない）。
- **アウトライン（背面法）**：`outlineWidth > 0` の Entity は、本描画の直後に同じ `Draw` の中でもう1回描く。法線方向に押し出した形を `CULL_FRONT`（裏面だけ）で単色描画。定数バッファもボーンも本描画のものを使う。太さはワールドの長さ。

### 描画の流れ（`ViewportPanel::RenderScene`）
1. **シャドウパス**：カメラに追従するライト行列を作り、ライトから見た深度をシャドウマップに書く（静的 → スキンド）。
2. **`rt.Begin`**：シーン用の描画先（色・法線・深度）をクリアし、**色だけ**を描画先にする。
3. **空**：色だけを描く（空のシェーダは法線を書かないため）。空のキューブマップは `t0`／サンプラは `s0`（空専用。線形・端で止める）。
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
- **日本語のコメントが壊れているファイルが2つある**（2026-11-09 に発見）：`Components/PlayerComponent.cpp`（コミット `53f5734` から）と `Systems/RenderSystem.cpp`（`cce8561` から）。Shift-JIS のファイルを UTF-8（BOM 付き）として保存し直したときに、日本語の部分が空白と記号に置き換わった（コードは無事で、ビルドも通る）。元のコメントは、その直前のコミットから読める（例：`git show b7bf732:Source/Engine/Components/PlayerComponent.cpp`）。PlayerComponent は 1-2a で直した。**`Editor/ViewportPanel.cpp` もコミット `da24c3c` で同じように壊れた**（メモ帳で開いて貼ったため。2章のミスの表）。直したファイル（UTF-8 BOM）は 2026-11-10 に渡し済み。
- **グラフ `Player1`（Player と Ally が使う）の倒れる遷移のパラメータが `Death`**。コードが渡すのは `Dead` なので、倒れるアニメにならない。Anim Graph パネルでパラメータの名前を `Dead` に変えて保存すれば直る（名前を変えると遷移の条件も付け替わる）。2026-11-10 にユーザーへ伝えた。
- AI 仲間の HitboxComponent は付け直したときに初期値になり、見た目（Player と同じロボット）より小さい。Player の値（胴 0〜1.42、太さ 0.35、頭 2.0・0.39）に合わせるとよい。
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
  - シェーダのホットリロード（`ShaderHotReload`、`ViewportPanel::ReloadShaders`）は作業中。コミットはされているが、2026-11-09 時点では `Engine` が `ShaderHotReload` を持っているだけで、`Initialize` / `Update` も `ReloadShaders` も呼ばれていない（まだ動かない）。それまでは `.hlsl` を変えたらビルドして起動し直す。
- IBL 用の DDS は2組（どちらも Git 管理下。1組 約 270MB）：`Data/Sprite/IBL/`（野原の写真）と `Data/Sprite/IBL_PureSky/`（曇り空。元の `overcast_soil_puresky_2k.hdr` も入っている）。中身はどちらもキューブ 1024²・RGBA16F・ミップ付き、`specular_pmrem` は事前計算済み、`lut_ggx` は 1024² の RGBA8。**今使っているのは `IBL_PureSky/` の `skybox.dds`（空の描画）・`specular_pmrem.dds` と `lut_ggx.dds`（映り込み）**。`diffuse_iem`（空から来る拡散光）はまだ使っていない（環境光は一様な色のまま）。布用の `sheen_pmrem` / `lut_charlie` / `lut_sheen_e` は使い道がない。
- 空を回しても、光の向き（`lightDir`）は一緒に回らない（太陽と影の向きを合わせるなら手で直す）。空の回転は Y 軸まわりだけ。「空の明るさ」は空と映り込みにだけ効き、メッシュの環境光・光の強さとは別。
- 曇り空なので太陽ははっきりしない。`IBL`（野原）の太陽の向きは (0.533, 0.753, 0.386)（高さ約 49°）、`IBL_PureSky` のいちばん明るい所は (0.805, 0.569, −0.167)（約 35°）。光の向き（`SceneLighting::lightDir`）は空とは別に設定する（自動では合わせない）。
- **アニメーション**
  - Blend Space は1軸だけ（2D は無い）。ルートモーション、IK は無い。
  - 状態の中にステートマシンは入れられない。モンタージュは同時に1つ。
  - 通知をゲーム側が受け取るのは1フレーム後。
  - Animation パネルのタイムラインは、グラフを使っていない Entity のときだけ。
  - Anim Graph パネルは選んだ Entity のグラフを直接編集する。同じグラフのファイルを使うほかの Entity には、保存して読み込み直すまで反映されない。
- ヒットボックスは体の中心線に固定。ボーン（頭など）に追従しない。
- NavGrid の範囲が固定（原点中心、60×60）。Play 中は毎フレーム作り直している。
- プレイヤーの移動・攻撃・カメラ・射撃は `PlayerComponent` 1つに全部入っている（ロールごとの差は未対応）。
- 新しいゲームで足りない土台（0章も参照）：ダウン・蘇生の状態、武器・リロード・アビリティ・クールダウン、親子付け（Attach。`Entity` に `parent` / `children` の欄はあるが未使用。武器を手に持たせるのに要る）、物理（剛体・衝突・トリガ）、ゲーム用の UI（今の HUD は ImGui）、音、エフェクト。
- Play中にコンポーネントを足した／消した結果は、編集用Worldにしか入らない。
- Prefab の Apply / Revert、Component の追加／削除は Undo できない。
- **エディタ**
  - Inspector は、画面外にはみ出た項目も毎フレーム処理している（Debug ではキャラを選ぶと少し重くなる。Component ごとに折りたためるようにすると軽くなる）。
  - `GlobalSkinnedCache().GetOrLoad` は呼ぶたびにキーの文字列を作る（Inspector などで毎フレーム数回呼ばれる。今は問題になっていない）。
  - パラメータ欄など一部は、文字の幅を数値（px）で決めている。フォントを変えたら見直す。

---

## 11. これからやる予定（セッションを分けて進める）

**最優先は 0章のロードマップ**（協力型 PvE のゲーム作り）。段階 1「3人チーム」は済み。今は段階 2「基本の AI 戦闘」の途中（2-1 射撃の共通化は済み。2-2a AI 仲間の攻撃を渡し済み。次は 2-2b 弾の線の表示）。各段階はさらに小さく分け、1つずつビルドと動作確認をする。資料の優先度は、ロール選択・チームと AI 仲間が最優先、武器・HP と蘇生・ウェーブ・HUD が高、アビリティが中〜高、敵・ボスと演出が中、AI の集合指示が低〜中（基本の AI ができてから）。

**AI Editor**（12章）：AI を本格的にするための計画（Blackboard → Pathfinding の確認 → Perception → BT の Runtime → BT のエディタ → Utility AI → EQS → Debugger）。始める時期と、ゲームのロードマップとの順番は未定（12章の最後の「ぶつかる所」を先にユーザーと決める）。

以下はエンジン側の残り（ゲーム作りで必要になったときに取り組む）：

- ~~本格的なシェーダ~~ → **一区切り済み**。残っている候補：
  - 部位ごとの材質（`ShaderComponent` をサブメッシュ単位に）
  - ~~HLSL をファイルに出す~~ → **済み**（`Shader/*.hlsl` → ビルドで `.cso`。コミット `f5ac873`）。ホットリロードは作業中
  - ~~metallic / roughness の見え方を直す~~ → **M-1〜M-3 で済み**（空のキューブマップ、GGX のハイライト、IBL の映り込み）。M-4（Environment に「空の明るさ」「空の回転」）も済み。残りの候補：`diffuse_iem` で環境光を空から取る、Toon の金属の段の調整
- ~~アニメーション（ブレンド、ステートマシンなど）~~ → **一区切り済み**（Animator、クロスフェード、ノードの木、Blend Space 1D、ステートマシン、LayeredBlend、エイムオフセット、ノードエディタ、通知、モンタージュ、グラフのアセット）。残っている候補：Blend Space 2D、ルートモーション
- ~~ファイルの分割・フォルダ整理・エディタの日本語化・FPS 固定~~ → **済み**
- 当たり判定（ボーン追従のヒットボックスなど）
- ~~プレイヤーのキャラごとの攻撃方法・移動方法~~ → 0章のロードマップ（段階 3 武器・段階 6 アビリティ）に入った
- （必要になったら）ゲームの更新を固定ステップにする（物理を作るとき、60FPS を下回るほど重くなったとき）。入力の「押した1フレーム」を取りこぼさない工夫が要る
- （必要になったら）Inspector の Component を折りたためるようにする（`CollapsingHeader`）

---

## 12. AI Editor（将来の計画。2026-11-13 にユーザーから受け取った）

**基準の資料**：`C:\Users\2240192\Desktop\AI_Editor_Project_Specification_for_Claude.docx`（2026-11-13）。AI の挙動を本格的にするために、エディタと AI の技術をエンジンに入れていく計画。細かい所はこの資料に従い、**資料の「既存の状態」は実際のコードで確かめる**（0章の資料と同じ扱い）。

### 目的
ゲームの敵 AI・味方 AI を、コードを書き換えるだけでなく、エディタで目で見ながら設定・編集・確認できるようにする。次の5つを、別々の道具にせず、**共通の AI Runtime・Blackboard・データ形式**でつなげる。一度に全部は作らず、段階的に足す。

| システム | 役割 | エディタでしたいこと |
|---|---|---|
| Behavior Tree | 行動の流れ | ノードの配置・接続・条件・Task の設定、実行中のノードの表示 |
| Utility AI | 複数の行動の優先度をスコアで比べる | Action の追加、評価条件、スコア・カーブ、今のスコアと選ばれた Action の表示 |
| AI Perception | 視覚・聴覚・ダメージで周りを知る | 方式のオン・オフ、距離・角度・しきい値、視界コーン・認識した相手・音の範囲の表示 |
| Pathfinding | 障害物を避けて目的地へ | 歩ける所・障害物・経路・Waypoint・目的地・再計算の表示 |
| EQS（Environment Query System） | 周りの候補地点を評価して選ぶ | Generator・Test・重み、候補とスコアの 3D 表示、選んだ地点 |

### 全体の形（層）
- **AI Editor**（作る・編集・保存・読み込み・デバッグ表示）→ **AI Data / Asset**（BT・Utility・Perception・EQS の設定）→ **AI Runtime**（データを読んでゲーム中に AI を動かす）→ **Game / Engine Systems**（Entity・Component・Transform・当たり判定・Navigation・Animation）。
- エディタの UI と AI の実行の処理を密にくっつけない。エディタ無しでもゲームが動く形にする（今の Anim Graph と同じ考え方：アセット＋Runtime＋編集画面）。
- **Blackboard**：AI が判断に使う情報の Key-Value の入れ物。BT・Utility・Perception・EQS の受け渡しの土台。Key の例：`TargetActor`（Entity ID）、`TargetLocation`、`HasTarget`、`LastKnownLocation`、`Health`、`MoveLocation`、`BestCoverLocation`。型の安全さ、未登録の Key、値が変わったときの通知、Entity の参照の寿命（`EntityId` の generation が使える）、保存の方法は、エンジンに合わせて決める。

### 各システムの要件（資料の 4章）
- **Behavior Tree**：最初のノードは Root / Selector / Sequence / Condition / Task。結果は Success / Failure / Running。後で Decorator・Service・Abort（条件が変わったら実行中を止める）。エディタ：ノードの追加・削除・移動・接続、選んだノードの詳細の編集、保存・読み込み、実行中のノードを光らせる。
- **Utility AI**：攻撃・回復・逃げるなどの Action に、体力・距離・弾・味方の状態からスコアを付け、高いものを選ぶ。BT の戦闘中の Task から呼んで選択を任せる案（BT と役割を重ねない）。
- **AI Perception**：Sight（距離・視野角・遮蔽物）、Hearing（音の位置・大きさ・減衰）、Damage（攻撃された・攻撃者・位置）。結果を Blackboard に書く。
- **Pathfinding**：**まず今の A* / NavGrid を調べてから**。広げるか NavMesh（Recast & Detour も候補。決定ではない）にするかを比べる。
- **EQS**：Generator（候補を作る）・Context（自分・相手・味方など基準）・Test（距離・視線・歩いて行けるか）・Weight / Score。初めの例：遮蔽物を探す、プレイヤーと適当な距離をとる、撃ちやすい位置を探す。候補が経路で行けるかも見る。
- **AI Debugger**（重要）：実行中に AI の Entity を選んで、BT の実行中のノードと結果・失敗の理由、Utility の候補とスコア、Perception の視界と認識した相手・最後に見た位置、経路と目的地、EQS の候補とスコアを見る。最初は大きな専用ウィンドウでなく、Inspector と Viewport への表示から始めてよい。

### 資料の開発の順番（6章。今のコードを見てから調整してよい）
| 段階 | 内容 | 終わりの目安 |
|---|---|---|
| 0 | 今のエンジン・AI・エディタの調査 | 関係するクラス・データ形式・機能・課題の一覧 |
| 1 | AI Core / Blackboard | AI が共通の情報を保存・取得できる |
| 2 | Pathfinding の確認・拡張 | 敵が障害物を避けて目的地へ行ける |
| 3 | AI Perception | 視覚・聴覚・ダメージを検知して情報を更新 |
| 4 | Behavior Tree の Runtime | Selector / Sequence / Condition / Task が動く |
| 5 | Behavior Tree のエディタ | ノードを編集・保存・読み込みしてゲームで動く |
| 6 | Utility AI | 複数の Action をスコアで選ぶ |
| 7 | EQS | 候補地点を作って評価し、いちばん良い所を選ぶ |
| 8 | 統合 AI Debugger | 5つの状態を実行中に見られる |
| 9 | 敵・味方の AI で検証 | 複数の AI の種類で使い回せる |

- **ノードの UI だけを先に作らない。** まず Runtime で最小の AI を動かし、その後でエディタから編集できるようにする。
- **最初の完成目標（MVP）**：敵 AI が①プレイヤーを視界で見つける ②見つけた情報を Blackboard に入れる ③BT で巡回・追跡・攻撃を切り替える ④経路で障害物を避ける ⑤エディタで BT のノードを編集・保存・読み込みできる ⑥実行中の BT と経路を確認できる。これが安定してから Utility AI と EQS。

### 保存（資料の 9章）
- エディタで作った AI の設定は、ゲームで読めるアセットとして保存する。JSON が候補だが、今の Prefab・Serialization を先に調べる（参考：`AnimGraphSerializer` の「版を持ち、その版以降だけ読む」形）。
- 中身：ノードの種類・ID・つながり・パラメータ・参照する Task / Action。読むときに、知らない種類・無い参照・型の違い・輪になったつながりを調べて、分かりやすくエラーを出す。データの版を持つ。

### 資料のフォルダの案（8章。今の構成と名前に合わせて変えてよい）
`Engine/AI/` の下に `Core/`（AIController, Blackboard）、`BehaviorTree/`（BehaviorTree, BTNode, BTSelector, BTSequence, BTCondition, BTTask）、`UtilityAI/`（UtilityAI, UtilityAction, UtilityConsideration）、`Perception/`（AIPerception, SightSensor, HearingSensor, DamageSensor）、`Navigation/`（NavigationSystem, Pathfinding）、`EQS/`（EnvironmentQuery, EQSGenerator, EQSTest）。`Editor/AIEditor/` に AIEditor, BehaviorTreeEditor, UtilityAIEditor, PerceptionEditor, NavigationDebugView, EQSEditor, AIDebugger。
- **`.cpp` の名前は `Source/` 全体で重複させない**（1章の 4）。たとえば `Pathfinding.cpp` などを作る前に検索する。

### 今回は対象外（資料の 10章）
機械学習・強化学習、LLM の会話・生成 AI、大規模な群衆、高度な Squad AI・Director AI、EQS の Generator / Test を全部そろえること、汎用の AI フレームワーク化。**まずルールベースの AI を安定して作り・編集し・デバッグできること。**

### 資料の作業のルール（0章・11章。この CLAUDE.md のルールと同じ向き）
- 最初に今のコード・フォルダ・エディタを確かめ、資料だけで「できている」と決めつけない。今のクラス名・命名・設計を尊重する。
- 小さく実装 → ビルド → 動作確認。ファイルのパス・場所・理由を書く。実装済み・今回・将来を分ける。選択肢は推奨案と良い点・悪い点。
- 資料を受けて最初にすること：関係するフォルダ・クラス・AI の処理・エディタを調べ、「使える／広げる必要／無い／要確認」に分ける（特に A* / NavGrid、ImGui エディタ、Entity / Component、Serialization / Prefab との接続）。資料の構成と今のコードの差を整理し、最初の小さな作業を1つに絞って、変えるファイル・内容・テストのしかたを出す。了承を得てから実装する。

### 資料の決まっていないこと（13章。勝手に決めず、調べてから案を出す）
今の A* / NavGrid を広げるか NavMesh か、AI の設定を今の Serialization に入れるか独立した JSON アセットにするか、BT の動かし方（毎フレーム・一定間隔・イベント）、Utility のスコアの正規化・カーブ・同点の扱い、Perception の視線を今の Raycast にどうつなぐか、EQS の候補の作り方・Test・到達できるかの確認、エディタの Undo/Redo・コピー＆ペースト・複数の AI の選択をいつ作るか。

### 今のコードで関係しそうな物（2026-11-13 時点で分かっている所。着手するときに調べ直す）
- **Pathfinding**：`AI/EngineNavGrid`（A*。Play 中に毎フレーム作り直し、原点中心 60×60・セル 1m で固定。塞ぐのは NavObstacle だけ）。敵（`OpponentComponent`）と AI 仲間（`AllyComponent`）が使っている。デバッグ表示は NavGrid パネルと、敵の経路の線（`ViewportPanel`）。
- **AIController に当たる物**：`AllyComponent`（AI 仲間）と `OpponentComponent`（敵）。判断を直接 C++ で書いている。陣営は `Game/Team.h`。
- **Perception（Sight）の芽**：`AllyComponent` の「見えるか」（`CanSee`。`Game/WeaponTrace` の `TraceShot` で間に壁・味方がいないか調べる。距離だけで候補を選び、線は選んだ1体だけ）。敵は距離だけで狙っている（視野角・遮蔽は見ていない）。
- **Damage の芽**：敵は「HP が減った」でひるむ（`OpponentComponent`。攻撃者は記録していない）。
- **ノードエディタの土台**：Anim Graph のキャンバス（`AnimGraphCanvas` / `AnimGraphStateMachineView`。拡大縮小・ドラッグ・右クリックの追加・全体表示）と、アセットの保存（`AnimGraphSerializer`）。BT エディタに使い回せるかを調べる。
- **昔の Behavior Tree**：削除前のコミット `2df15d7` に `Source/AI/BehaviorTree.*`・`BehaviorNode.*`・`BehaviorStatus.h`・`BehaviorAction.h`・`BehaviorJudgment.h`・`EnemyBehaviors.*` がある（`git show 2df15d7:Source/AI/BehaviorTree.cpp`）。参考になるかは中身を見て判断する。

### この CLAUDE.md とぶつかる所（着手する前にユーザーに確かめる）
- **エディタの文字**：資料は「Editor の UI は英語が基本」、2章は「エディタの説明・ボタン・メッセージは日本語（`u8"..."`）」。AI Editor の部分だけ英語にするか、ユーザーに聞く。
- **`.vcxproj`**：資料は「理由を説明せずに変えない」、1章は「変えない（ユーザーが VS で足す）」。**1章のほうに従う。**
- **ゲームのロードマップとの順番**：資料の MVP は「敵 AI（巡回・追跡・攻撃）」から。0章のロードマップは AI 仲間が中心で、AI の高度化は段階 9。いつ・どちらから始めるか（たとえば段階 2〜5 のゲームの形ができてから、敵と AI 仲間の判断を BT へ移す、など）はユーザーと決める。
