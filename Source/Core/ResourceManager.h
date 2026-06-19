#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include "Graphics/Graphics.h"
#include "Graphics/Model/Model.h"

// リソース（モデルやテクスチャ）を一元管理するシングルトンクラス
// 企業レベルで必須となる「Flyweightパターン」の実装
class ResourceManager
{
public:
	static ResourceManager& Instance()
	{
		static ResourceManager instance;
		return instance;
	}

	std::shared_ptr<Model> LoadModel(const std::string& filename)
	{
		// すでに読み込み済みのモデルかキャッシュ（連想配列）を検索
		auto it = modelCache.find(filename);
		if (it != modelCache.end())
		{
			// キャッシュヒット：既存のポインタを返すだけ（超高速・VRAM節約）
			return it->second;
		}

		// キャッシュミス：新規にモデルを読み込んでVRAMに転送
		ID3D11Device* device = Graphics::Instance().GetDevice();
		std::shared_ptr<Model> newModel = std::make_shared<Model>(device, filename.c_str());

		// 次回以降のためにキャッシュに登録
		modelCache.emplace(filename, newModel);

		return newModel;
	}

	// シーン遷移時などに不要なメモリを解放する
	void Clear()
	{
		modelCache.clear();
	}

private:
	ResourceManager() = default;
	~ResourceManager() = default;

	// コピー生成の禁止（シングルトンの徹底）
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;

	std::unordered_map<std::string, std::shared_ptr<Model>> modelCache;
};