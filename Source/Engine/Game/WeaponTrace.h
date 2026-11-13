#pragma once
#include <DirectXMath.h>

namespace uel {
    class Entity;
    struct GameContext;

    // 弾の線が何に当たったか
    struct ShotHit {
        Entity* entity = nullptr;       // いちばん手前で当たった物（nullptr = 何にも当たらなかった）
        float   distance = 0.0f;        // 撃った位置からの距離（m）
        bool    headshot = false;       // ヒットボックスの頭に当たった
        float   damageMultiplier = 1.0f;// 頭なら Hitbox の倍率、それ以外は 1
    };

    // 撃った位置 origin から、向き dir（長さ 1）へ弾の線を伸ばし、いちばん手前の物を返す（UE の LineTraceSingleByChannel に当たる）。
    //   調べる物：ヒットボックス（胴体カプセル＋頭の球）と、ヒットボックスを持たないメッシュ（壁・床など）。近いほうに当たる
    //   無視する物：撃った本人（ctx.self）、倒れている最中の敵、戦闘不能の味方
    //   味方も「当たる」（弾がそこで止まる）。ダメージを入れるかどうかは ApplyShotDamage が決める
    ShotHit TraceShot(GameContext& ctx, const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDistance = 1000.0f);

    // 当たった物にダメージを与える（UE の ApplyPointDamage に当たる）。撃った本人は ctx.self。
    //   味方（同じ陣営）と、HP を持たない物にはダメージが入らない（戻り値 false）。
    //   当たった物を赤くする。陣営の無い物（的・箱など）は HP 0 でその場で消す（敵は自分で倒れてから消え、味方は消えない）
    //   戻り値：ダメージが入ったか（ヒットマーカーを出すかどうかに使う）
    bool ApplyShotDamage(GameContext& ctx, const ShotHit& hit, float baseDamage);

} // namespace uel
