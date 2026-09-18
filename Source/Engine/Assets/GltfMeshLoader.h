#pragma once
#include <memory>
#include <string>

struct ID3D11Device;

namespace uel {
	class Mesh;

	// GLB(.glb) を読み込み、全プリミティブを統合した1つのMeshを返す（失敗時 nullptr）。
	std::shared_ptr<Mesh> LoadGltfMesh(ID3D11Device* device, const std::string& path);

} // namespace uel