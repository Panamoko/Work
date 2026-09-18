#pragma once
#include <memory>
#include <string>
#include <unordered_map>

struct ID3D11Device;

namespace uel {
    class Mesh;

    class MeshCache {
    public:
        // 読み込み済みなら返す。未読なら読み込んでキャッシュ（失敗は nullptr をキャッシュし再試行しない）。
        std::shared_ptr<Mesh> GetOrLoad(ID3D11Device* device, const std::string& path);

    private:
        std::unordered_map<std::string, std::shared_ptr<Mesh>> cache;
    };

} // namespace uel