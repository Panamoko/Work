#pragma once
struct ID3D11Device;

namespace uel {
    class EngineNavGrid; class World; class MeshCache;

    // 経路探索のグリッドを、今の Scene から作る（5-4c）。Play を始めたとき・Scene が変わったときだけ呼ぶ（ViewportPanel）。
    // NavBoundsComponent があれば、その箱の範囲で、地面を見て作る（UE の NavMeshBoundsVolume と NavMesh の自動生成に当たる）：
    //   地面になるのは表示中の MeshRenderer（当たり判定なしのサブメッシュは除く。キャラの地面の決まりと同じ）。
    // 無ければ今までどおり、原点を真ん中にした 60m 四方で、NavObstacle だけを見る。
    // グリッド（AI/EngineNavGrid）はメッシュや Component を知らないので、ここで「Scene から地面の高さを調べる関数」を作って渡す
    void BakeNavForWorld(EngineNavGrid& nav, World& world, MeshCache& cache, ID3D11Device* device);

} // namespace uel
