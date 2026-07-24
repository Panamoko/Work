#include <algorithm>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#include "Core/Misc.h"
#include "Graphics/GpuResourceUtils.h"
#include "Graphics/Model/ModelRenderer.h"
#include "Graphics/ShaderClass/BasicShader.h"
#include "Graphics/ShaderClass/LambertShader.h"
#include "Graphics/ShaderClass/PbrShader.h"
#include "Graphics/ShaderClass/ToonShader.h"

// Pick a node's world transform: per-instance pose if provided, else the
// model's own (shared) node the mesh/bone points at.
static const DirectX::XMFLOAT4X4& BoneWorld(const Model::Bone& bone, const std::vector<Model::Node>* poseNodes)
{
	return poseNodes ? (*poseNodes)[bone.nodeIndex].worldTransform : bone.node->worldTransform;
}
static const DirectX::XMFLOAT4X4& MeshNodeWorld(const Model::Mesh& mesh, const std::vector<Model::Node>* poseNodes)
{
	return poseNodes ? (*poseNodes)[mesh.nodeIndex].worldTransform : mesh.node->worldTransform;
}

static const char* kShadowHLSL = R"(
cbuffer CbSkeleton : register(b6)
{
	row_major float4x4 boneTransforms[256];
};
cbuffer CbShadow : register(b0)
{
	row_major float4x4 lightViewProjection;
};

float4 SkinPos(float4 p, float4 w, uint4 idx)
{
	float4 o = float4(0, 0, 0, 0);
	[unroll] for (int i = 0; i < 4; i++) o += w[i] * mul(p, boneTransforms[idx[i]]);
	return o;
}

float4 VSMain(
	float4 position : POSITION,
	float3 normal : NORMAL,
	float4 tangent : TANGENT,
	float2 texcoord : TEXCOORD,
	float4 boneWeights : BONE_WEIGHTS,
	uint4  boneIndices : BONE_INDICES) : SV_POSITION
{
	float4 wp = SkinPos(position, boneWeights, boneIndices);
	return mul(wp, lightViewProjection);
}
)";

ModelRenderer::ModelRenderer(ID3D11Device* device)
{
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbScene), sceneConstantBuffer.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbSkeleton), skeletonConstantBuffer.GetAddressOf());

	shaders[static_cast<int>(ShaderId::Basic)] = std::make_unique<BasicShader>(device);
	shaders[static_cast<int>(ShaderId::Lambert)] = std::make_unique<LambertShader>(device);
	shaders[static_cast<int>(ShaderId::Pbr)] = std::make_unique<PbrShader>(device);

	shaders[static_cast<int>(ShaderId::Toon)] = std::make_unique<ToonShader>(device);
	toonOutlineShader = std::make_unique<ToonOutlineShader>(device);

	InitShadow(device);
}

void ModelRenderer::InitShadow(ID3D11Device* device)
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
	D3D11_TEXTURE2D_DESC td{};
	td.Width = shadowMapSize;
	td.Height = shadowMapSize;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R32_TYPELESS;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	HRESULT hr = device->CreateTexture2D(&td, nullptr, tex.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
	dsvd.Format = DXGI_FORMAT_D32_FLOAT;
	dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	hr = device->CreateDepthStencilView(tex.Get(), &dsvd, shadowDSV.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
	srvd.Format = DXGI_FORMAT_R32_FLOAT;
	srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Texture2D.MipLevels = 1;
	hr = device->CreateShaderResourceView(tex.Get(), &srvd, shadowSRV.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbShadow), shadowConstantBuffer.GetAddressOf());

	UINT flags = 0;
#if defined(_DEBUG)
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, errBlob;
	hr = D3DCompile(kShadowHLSL, strlen(kShadowHLSL), "Shadow", nullptr, nullptr,
		"VSMain", "vs_5_0", flags, 0, vsBlob.GetAddressOf(), errBlob.GetAddressOf());
	if (FAILED(hr))
	{
		if (errBlob) OutputDebugStringA(static_cast<const char*>(errBlob->GetBufferPointer()));
		_ASSERT_EXPR(false, L"Shadow VS compile failed");
		return;
	}
	device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, shadowVS.GetAddressOf());
	device->CreateInputLayout(Model::InputElementDescs.data(), static_cast<UINT>(Model::InputElementDescs.size()),
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), shadowInputLayout.GetAddressOf());

	DirectX::XMStoreFloat4x4(&lightViewProjection, DirectX::XMMatrixIdentity());
}

void ModelRenderer::RenderShadowDepth(const RenderContext& rc)
{
	if (!shadowDSV || !shadowVS) return;
	// Skip regenerating the shadow map on tiny passes (e.g. a single-model pass);
	// keep the world shadow map so that model is still shaded by the world.
	if (drawInfos.size() < 2) return;

	ID3D11DeviceContext* dc = rc.deviceContext;

	static LightManager defaultLightManager;
	const LightManager* lightManager = rc.lightManager ? rc.lightManager : &defaultLightManager;
	const DirectionalLight& dl = lightManager->GetDirectionalLight();

	DirectX::XMVECTOR L = DirectX::XMVector3Normalize(DirectX::XMVectorSet(dl.direction.x, dl.direction.y, dl.direction.z, 0.0f));
	const DirectX::XMFLOAT3& eye = rc.camera->GetEye();
	DirectX::XMVECTOR focus = DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f);

	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	float dotUp = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(L, up)));
	if (dotUp > 0.99f) up = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	DirectX::XMVECTOR lightEye = DirectX::XMVectorSubtract(focus, DirectX::XMVectorScale(L, shadowDistance));
	DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(lightEye, focus, up);
	DirectX::XMMATRIX proj = DirectX::XMMatrixOrthographicLH(shadowRadius * 2.0f, shadowRadius * 2.0f, shadowNear, shadowFar);
	DirectX::XMStoreFloat4x4(&lightViewProjection, view * proj);

	ID3D11RenderTargetView* savedRTV = nullptr;
	ID3D11DepthStencilView* savedDSV = nullptr;
	dc->OMGetRenderTargets(1, &savedRTV, &savedDSV);
	UINT numVP = 1;
	D3D11_VIEWPORT savedVP{};
	dc->RSGetViewports(&numVP, &savedVP);

	ID3D11ShaderResourceView* nullSrv[1] = { nullptr };
	dc->PSSetShaderResources(8, 1, nullSrv);

	dc->OMSetRenderTargets(0, nullptr, shadowDSV.Get());
	dc->ClearDepthStencilView(shadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	D3D11_VIEWPORT vp{};
	vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
	vp.Width = static_cast<float>(shadowMapSize);
	vp.Height = static_cast<float>(shadowMapSize);
	vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
	dc->RSSetViewports(1, &vp);

	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::ShadowDepth));
	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

	dc->IASetInputLayout(shadowInputLayout.Get());
	dc->VSSetShader(shadowVS.Get(), nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);

	CbShadow cb{};
	cb.lightViewProjection = lightViewProjection;
	dc->UpdateSubresource(shadowConstantBuffer.Get(), 0, 0, &cb, 0, 0);
	dc->VSSetConstantBuffers(0, 1, shadowConstantBuffer.GetAddressOf());
	dc->VSSetConstantBuffers(6, 1, skeletonConstantBuffer.GetAddressOf());

	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (DrawInfo& drawInfo : drawInfos)
	{
		const std::vector<Model::Node>* poseNodes = drawInfo.poseNodes;
		const std::vector<bool>* mv = drawInfo.meshVisible;
		const std::vector<Model::Mesh>& meshes = drawInfo.model->GetMeshes();
		for (size_t mi = 0; mi < meshes.size(); ++mi)
		{
			if (mv && mi < mv->size() && !(*mv)[mi]) continue; // per-instance hidden mesh
			const Model::Mesh& mesh = meshes[mi];

			if (mesh.material != nullptr && mesh.material->alphaMode == Model::AlphaMode::Blend) continue;

			UINT stride = sizeof(Model::Vertex);
			UINT offset = 0;
			dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
			dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			CbSkeleton cbSkeleton{};
			if (mesh.bones.size() > 0)
			{
				for (size_t i = 0; i < mesh.bones.size(); ++i)
				{
					const Model::Bone& bone = mesh.bones.at(i);
					DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&BoneWorld(bone, poseNodes));
					DirectX::XMMATRIX O = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
					DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], O * W);
				}
			}
			else
			{
				cbSkeleton.boneTransforms[0] = MeshNodeWorld(mesh, poseNodes);
			}
			dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);

			dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
		}
	}

	dc->OMSetRenderTargets(1, &savedRTV, savedDSV);
	dc->RSSetViewports(1, &savedVP);
	if (savedRTV) savedRTV->Release();
	if (savedDSV) savedDSV->Release();
}

void ModelRenderer::Draw(ShaderId shaderId, std::shared_ptr<Model> model)
{
	DrawInfo& drawInfo = drawInfos.emplace_back();
	drawInfo.shaderId = shaderId;
	drawInfo.model = model;
	drawInfo.poseNodes = nullptr;
}

void ModelRenderer::Draw(ShaderId shaderId, std::shared_ptr<Model> model,
	const std::vector<Model::Node>* poseNodes,
	const std::vector<bool>* meshVisible)
{
	DrawInfo& drawInfo = drawInfos.emplace_back();
	drawInfo.shaderId = shaderId;
	drawInfo.model = model;
	drawInfo.poseNodes = poseNodes;
	drawInfo.meshVisible = meshVisible;
}

void ModelRenderer::Render(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	if (enableShadow)
	{
		RenderShadowDepth(rc);
	}

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

		cbScene.lightViewProjection = lightViewProjection;
		cbScene.shadowParams = {
			shadowStrength,
			shadowBias,
			enableShadow ? 1.0f : 0.0f,
			1.0f / static_cast<float>(shadowMapSize)
		};

		dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, 0, &cbScene, 0, 0);
	}

	ID3D11Buffer* vsConstantBuffers[] = { skeletonConstantBuffer.Get(), sceneConstantBuffer.Get() };
	ID3D11Buffer* psConstantBuffers[] = { sceneConstantBuffer.Get() };
	dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
	dc->PSSetConstantBuffers(7, _countof(psConstantBuffers), psConstantBuffers);

	ID3D11SamplerState* samplers[] =
	{
		rc.renderState->GetSamplerState(SamplerState::LinearWrap),
		rc.renderState->GetSamplerState(SamplerState::LinearClamp),
	};
	dc->PSSetSamplers(0, _countof(samplers), samplers);

	// Shadow comparison sampler at s2.
	ID3D11SamplerState* shadowSamp = rc.renderState->GetSamplerState(SamplerState::ShadowCompare);
	dc->PSSetSamplers(2, 1, &shadowSamp);

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

	// Shadow map at t8.
	ID3D11ShaderResourceView* shadowSrvBind = (enableShadow ? shadowSRV.Get() : nullptr);
	dc->PSSetShaderResources(8, 1, &shadowSrvBind);

	auto drawMesh = [&](const Model::Mesh& mesh, Shader* shader, const std::vector<Model::Node>* poseNodes)
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
					DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&BoneWorld(bone, poseNodes));
					DirectX::XMMATRIX O = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
					DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], O * W);
				}
			}
			else
			{
				cbSkeleton.boneTransforms[0] = MeshNodeWorld(mesh, poseNodes);
			}
			dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);

			shader->Update(rc, mesh);
			dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
		};

	DirectX::XMVECTOR CameraPosition = DirectX::XMLoadFloat3(&rc.camera->GetEye());
	DirectX::XMVECTOR CameraFront = DirectX::XMLoadFloat3(&rc.camera->GetFront());

	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));

	for (DrawInfo& drawInfo : drawInfos)
	{
		const std::vector<Model::Node>* poseNodes = drawInfo.poseNodes;
		const std::vector<bool>* mv = drawInfo.meshVisible;
		const std::vector<Model::Mesh>& meshes = drawInfo.model->GetMeshes();

		if (drawInfo.shaderId == ShaderId::Toon)
		{
			dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullFront));
			toonOutlineShader->Begin(rc);

			for (size_t mi = 0; mi < meshes.size(); ++mi)
			{
				if (mv && mi < mv->size() && !(*mv)[mi]) continue; // per-instance hidden mesh
				const Model::Mesh& mesh = meshes[mi];

				if (mesh.material == nullptr) continue;

				if (mesh.material->alphaMode == Model::AlphaMode::Blend ||
					(mesh.material->baseColor.w > 0.01f && mesh.material->baseColor.w < 0.99f)) continue;

				drawMesh(mesh, toonOutlineShader.get(), poseNodes);
			}
			toonOutlineShader->End(rc);

			dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
		}

		Shader* shader = shaders[static_cast<int>(drawInfo.shaderId)].get();
		shader->Begin(rc);

		for (size_t mi = 0; mi < meshes.size(); ++mi)
		{
			if (mv && mi < mv->size() && !(*mv)[mi]) continue; // per-instance hidden mesh
			const Model::Mesh& mesh = meshes[mi];

			if (mesh.material == nullptr)
			{
				drawMesh(mesh, shader, poseNodes);
				continue;
			}

			if (mesh.material->alphaMode == Model::AlphaMode::Blend ||
				(mesh.material->baseColor.w > 0.01f && mesh.material->baseColor.w < 0.99f))
			{
				TransparencyDrawInfo& tdi = transparencyDrawInfos.emplace_back();
				tdi.mesh = &mesh;
				tdi.shaderId = drawInfo.shaderId;
				tdi.poseNodes = poseNodes;
				const DirectX::XMFLOAT4X4& mw = MeshNodeWorld(mesh, poseNodes);
				DirectX::XMVECTOR pos = DirectX::XMVectorSet(mw._41, mw._42, mw._43, 0);
				tdi.distance = DirectX::XMVectorGetX(
					DirectX::XMVector3Dot(CameraFront, DirectX::XMVectorSubtract(pos, CameraPosition)));
				continue;
			}
			drawMesh(mesh, shader, poseNodes);
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
		drawMesh(*tdi.mesh, shader, tdi.poseNodes);
		shader->End(rc);
	}
	transparencyDrawInfos.clear();

	for (auto& cb : vsConstantBuffers) cb = nullptr;
	for (auto& cb : psConstantBuffers) cb = nullptr;
	dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
	dc->PSSetConstantBuffers(7, _countof(psConstantBuffers), psConstantBuffers);

	for (auto& s : samplers) s = nullptr;
	dc->PSSetSamplers(0, _countof(samplers), samplers);

	ID3D11ShaderResourceView* nullIBL[] = { nullptr, nullptr, nullptr };
	dc->PSSetShaderResources(5, _countof(nullIBL), nullIBL);

	// Unbind the shadow map so it can be used as a depth target next frame.
	ID3D11ShaderResourceView* nullSm = nullptr;
	dc->PSSetShaderResources(8, 1, &nullSm);
}