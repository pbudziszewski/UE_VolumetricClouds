#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "RenderGraphUtils.h"
#include "ScreenPass.h"
#include "PipelineStateCache.h"
#include "SceneViewExtension.h"

// ================================================================================================

struct FColorVertex
{
public:
	FVector2f Position;
	FVector4f Color;
};

// ================================================================================================

class FTriangleVertexBuffer : public FVertexBuffer
{
public:
	void InitRHI(FRHICommandListBase& RHICmcList) override {

		// This is a Numa Array type, technically I am just using this object to obtain the correct size the RHI CMD will require.
		TResourceArray<FColorVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(3);
 
		Vertices[0].Position = FVector2f(0.0f,0.75f);
		Vertices[0].Color = FVector4f(1, 0, 0, 1);

		Vertices[1].Position = FVector2f(0.75,-0.75);
		Vertices[1].Color = FVector4f(0, 1, 0, 1);

		Vertices[2].Position = FVector2f(-0.75,-0.75);
		Vertices[2].Color = FVector4f(0, 0, 1, 1);

		FRHIResourceCreateInfo CreateInfo(TEXT("FScreenRectangleVertexBuffer"), &Vertices);

		VertexBufferRHI = RHICmcList.CreateVertexBuffer(Vertices.GetResourceDataSize(),EBufferUsageFlags::Static,CreateInfo);
	}
};

extern TGlobalResource<FTriangleVertexBuffer> GTriangleVertexBuffer;

// ================================================================================================

class FTriangleIndexBuffer : public FIndexBuffer
{
public:
	void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		const uint32 Indices[] = { 0, 1, 2 };

		TResourceArray<uint32, INDEXBUFFER_ALIGNMENT> IndexBuffer;
		uint32 NumIndices = UE_ARRAY_COUNT(Indices);
		IndexBuffer.AddUninitialized(NumIndices);
		FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint32));

		FRHIResourceCreateInfo CreateInfo(TEXT("FTriangleIndexBuffer"), &IndexBuffer);
		IndexBufferRHI = RHICmdList.CreateIndexBuffer(sizeof(uint32), IndexBuffer.GetResourceDataSize(),BUF_Static,CreateInfo);
	}
};

extern TGlobalResource<FTriangleIndexBuffer> GTriangleIndexBuffer;

// ================================================================================================

class FTriangleVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FTriangleVertexDeclaration() {}

	virtual void InitRHI(FRHICommandListBase& RHICmdList)
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FColorVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FColorVertex, Position), VET_Float2, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FColorVertex, Color), VET_Float4, 1, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

extern TGlobalResource<FTriangleVertexDeclaration> GTriangleVertexDeclaration;

// ================================================================================================

BEGIN_SHADER_PARAMETER_STRUCT(FCloudVSParams,)
	//SHADER_PARAMETER_STRUCT_ARRAY(FCloudVertParams,Verticies)
	//RENDER_TARGET_BINDING_SLOTS()
	SHADER_PARAMETER(FMatrix44f, Transform)
END_SHADER_PARAMETER_STRUCT()

class FCloudVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCloudVS);
	SHADER_USE_PARAMETER_STRUCT(FCloudVS, FGlobalShader)
	using FParameters = FCloudVSParams;
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}	
};

// ================================================================================================

BEGIN_SHADER_PARAMETER_STRUCT(FCloudPSParams,)
	RENDER_TARGET_BINDING_SLOTS()
	SHADER_PARAMETER(FVector4f, Offset)
END_SHADER_PARAMETER_STRUCT()

class FCloudPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCloudPS);
	using FParameters = FCloudPSParams;
	SHADER_USE_PARAMETER_STRUCT(FCloudPS, FGlobalShader)
};

// ================================================================================================

class FCloudSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FCloudSceneViewExtension(const FAutoRegister& AutoRegister);
	~FCloudSceneViewExtension();

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override {};
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual void PostRenderBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {};
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

	//~ End FSceneViewExtensionBase Interface
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

	// A delegate that is called when the Tone mapper pass finishes
	FScreenPassTexture TrianglePass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);

public:
	static void RenderTriangle
	(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* ViewShaderMap,
		const FIntRect& View,
		const FScreenPassTexture& InSceneColor,
		const FMatrix& ViewProjMatrix);

};
