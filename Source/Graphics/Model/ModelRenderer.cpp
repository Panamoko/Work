#include <algorithm>
#include "Core/Misc.h"
#include "Graphics/GpuResourceUtils.h"
#include "Graphics/Model/ModelRenderer.h"
#include "Graphics/ShaderClass/BasicShader.h"
#include "Graphics/ShaderClass/LambertShader.h"
#include "Graphics/ShaderClass/PbrShader.h"

ModelRenderer::ModelRenderer(ID3D11Device* device)
{
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbScene), sceneConstantBuffer.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbSkeleton), skeletonConstantBuffer.GetAddressOf());

	shaders[static_cast<int>(ShaderId::Basic)] = std::make_unique<BasicShader>(device);
	shaders[static_cast<int>(ShaderId::Lambert)] = std::make_unique<LambertShader>(device);
	shaders[static_cast<int>(ShaderId::Pbr)] = std::make_unique<PbrShader>(device);
}

void ModelRenderer::Draw(ShaderId shaderId, std::shared_ptr<Model> model)
{
	DrawInfo& drawInfo = drawInfos.emplace_back();
	drawInfo.shaderId = shaderId;
	drawInfo.model = model;
}

void ModelRenderer::Render(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	{
		static LightManager defaultLightManager;
		const LightManager* lightManager = rc.lightManager ? rc.lightManager : &defaultLightManager;

		CbScene cbScene{};
		DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.camera->GetView());
		DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.camera->GetProjection());
		DirectX::XMStoreFloat4x4(&cbScene.viewProjection, V * P);
		const DirectionalLight& dl = lightManager->GetDirectionalLight();
		cbScene.lightDirection.x = dl.direction.x;
		cbScene.lightDirection.y = dl.direction.y;
		cbScene.lightDirection.z = dl.direction.z;
		cbScene.lightColor.x = dl.color.x;
		cbScene.lightColor.y = dl.color.y;
		cbScene.lightColor.z = dl.color.z;
		const DirectX::XMFLOAT3& eye = rc.camera->GetEye();
		cbScene.cameraPosition.x = eye.x;
		cbScene.cameraPosition.y = eye.y;
		cbScene.cameraPosition.z = eye.z;
		dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, 0, &cbScene, 0, 0);
	}

	ID3D11Buffer* vsConstantBuffers[] = { skeletonConstantBuffer.Get(), sceneConstantBuffer.Get() };
	ID3D11Buffer* psConstantBuffers[] = { sceneConstantBuffer.Get() };
	dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
	dc->PSSetConstantBuffers(7, _countof(psConstantBuffers), psConstantBuffers);

	// s0 = LinearWrap（マテリアルテクスチャ）, s1 = LinearClamp（IBL）
	ID3D11SamplerState* samplers[] =
	{
		rc.renderState->GetSamplerState(SamplerState::LinearWrap),
		rc.renderState->GetSamplerState(SamplerState::LinearClamp),
	};
	dc->PSSetSamplers(0, _countof(samplers), samplers);

	// IBLテクスチャを t5/t6/t7 にバインド
	if (enableIBL)
	{
		ID3D11ShaderResourceView* iblSRVs[] = { iblIrradiance, iblPrefiltered, iblBrdfLut };
		dc->PSSetShaderResources(5, _countof(iblSRVs), iblSRVs);
	}
	else
	{
		ID3D11ShaderResourceView* nullIBL[] = { nullptr, nullptr, nullptr };
		dc->PSSetShaderResources(5, _countof(nullIBL), nullIBL);
	}
	auto drawMesh = [&](const Model::Mesh& mesh, Shader* shader)
		{
			UINT stride = sizeof(Model::Vertex);
			UINT offset = 0;
			dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
			dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			CbSkeleton cbSkeleton{};
			if (mesh.bones.size() > 0)
			{
				for (size_t i = 0; i < mesh.bones.size(); ++i)
				{
					const Model::Bone& bone = mesh.bones.at(i);
					DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&bone.node->worldTransform);
					DirectX::XMMATRIX O = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
					DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], O * W);
				}
			}
			else
			{
				cbSkeleton.boneTransforms[0] = mesh.node->worldTransform;
			}
			dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);

			shader->Update(rc, mesh);
			dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
		};

	DirectX::XMVECTOR CameraPosition = DirectX::XMLoadFloat3(&rc.camera->GetEye());
	DirectX::XMVECTOR CameraFront = DirectX::XMLoadFloat3(&rc.camera->GetFront());

	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

	for (DrawInfo& drawInfo : drawInfos)
	{
		Shader* shader = shaders[static_cast<int>(drawInfo.shaderId)].get();
		shader->Begin(rc);

		for (const Model::Mesh& mesh : drawInfo.model->GetMeshes())
		{
			if (mesh.material->alphaMode == Model::AlphaMode::Blend ||
				(mesh.material->baseColor.w > 0.01f && mesh.material->baseColor.w < 0.99f))
			{
				TransparencyDrawInfo& tdi = transparencyDrawInfos.emplace_back();
				tdi.mesh = &mesh;
				tdi.shaderId = drawInfo.shaderId;
				DirectX::XMVECTOR pos = DirectX::XMVectorSet(
					mesh.node->worldTransform._41,
					mesh.node->worldTransform._42,
					mesh.node->worldTransform._43, 0);
				tdi.distance = DirectX::XMVectorGetX(
					DirectX::XMVector3Dot(CameraFront, DirectX::XMVectorSubtract(pos, CameraPosition)));
				continue;
			}
			drawMesh(mesh, shader);
		}
		shader->End(rc);
	}
	drawInfos.clear();

	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);

	std::sort(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
		[](const TransparencyDrawInfo& a, const TransparencyDrawInfo& b)
		{ return a.distance > b.distance; });

	for (const TransparencyDrawInfo& tdi : transparencyDrawInfos)
	{
		Shader* shader = shaders[static_cast<int>(tdi.shaderId)].get();
		shader->Begin(rc);
		drawMesh(*tdi.mesh, shader);
		shader->End(rc);
	}
	transparencyDrawInfos.clear();

	// バインド解除
	for (auto& cb : vsConstantBuffers) cb = nullptr;
	for (auto& cb : psConstantBuffers) cb = nullptr;
	dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
	dc->PSSetConstantBuffers(7, _countof(psConstantBuffers), psConstantBuffers);

	for (auto& s : samplers) s = nullptr;
	dc->PSSetSamplers(0, _countof(samplers), samplers);

	ID3D11ShaderResourceView* nullIBL[] = { nullptr, nullptr, nullptr };
	dc->PSSetShaderResources(5, _countof(nullIBL), nullIBL);
}