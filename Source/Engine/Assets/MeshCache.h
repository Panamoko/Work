#pragma once
#include <memory>
#include <string>
#include <unordered_map>

struct ID3D11Device;

namespace uel {
    class Mesh;

    class MeshCache {
    public:
        std::shared_ptr<Mesh> GetOrLoad(ID3D11Device* device, const std::string& path);
        std::shared_ptr<Mesh> GetCube(ID3D11Device* device);   // šÕ“Ë—p‚Ìè‘±‚«—§•û‘Ì
    private:
        std::unordered_map<std::string, std::shared_ptr<Mesh>> cache;
        std::shared_ptr<Mesh> cube;   // š


    };

} // namespace uel