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
	FVector3f Position;
	FVector4f Color;
};

// ================================================================================================

class FTriangleVertexBuffer : public FVertexBuffer
{
public:
	void InitRHI(FRHICommandListBase& RHICmcList) override {

		// Cube Vertices
		TArray<FVector3f> VertexPositions = {
			FVector3f(-200, -200, -200), // 0: Bottom-left-back
			FVector3f( 200, -200, -200), // 1: Bottom-right-back
			FVector3f( 200,  200, -200), // 2: Top-right-back
			FVector3f(-200,  200, -200), // 3: Top-left-back
			FVector3f(-200, -200,  200), // 4: Bottom-left-front
			FVector3f( 200, -200,  200), // 5: Bottom-right-front
			FVector3f( 200,  200,  200), // 6: Top-right-front
			FVector3f(-200,  200,  200)  // 7: Top-left-front
		};

		uint32 NumVertices = VertexPositions.Num();
		TResourceArray<FColorVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(NumVertices);

		for (size_t i = 0; i < NumVertices; i++)
		{
			Vertices[i].Position = VertexPositions[i] + FVector3f(3000.0, -1000.0, 70.0);
			Vertices[i].Color = FVector4f(1.0, 0.0, 0.0, 1.0);
		}

		FRHIResourceCreateInfo CreateInfo(TEXT("FCloudVertexBuffer"), &Vertices);
		VertexBufferRHI = RHICmcList.CreateVertexBuffer(Vertices.GetResourceDataSize(), EBufferUsageFlags::Static, CreateInfo);
	}
};

extern TGlobalResource<FTriangleVertexBuffer> GCloudVertexBuffer;

// ================================================================================================

class FTriangleIndexBuffer : public FIndexBuffer
{
public:
	void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		// Cube Indices (Two Triangles Per Face)
		const uint32 Indices[] = {
			// Back Face
			0, 1, 2,  0, 2, 3,

			// Front Face
			4, 6, 5,  4, 7, 6,

			// Left Face
			4, 0, 3,  4, 3, 7,

			// Right Face
			1, 5, 6,  1, 6, 2,

			// Bottom Face
			4, 5, 1,  4, 1, 0,

			// Top Face
			3, 2, 6,  3, 6, 7
		};

		TResourceArray<uint32, INDEXBUFFER_ALIGNMENT> IndexBuffer;
		uint32 NumIndices = UE_ARRAY_COUNT(Indices);
		IndexBuffer.AddUninitialized(NumIndices);
		FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint32));

		FRHIResourceCreateInfo CreateInfo(TEXT("FCloudIndexBuffer"), &IndexBuffer);
		IndexBufferRHI = RHICmdList.CreateIndexBuffer(sizeof(uint32), IndexBuffer.GetResourceDataSize(),BUF_Static,CreateInfo);
	}
};

extern TGlobalResource<FTriangleIndexBuffer> GCloudIndexBuffer;

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

extern TGlobalResource<FTriangleVertexDeclaration> GCloudVertexDeclaration;

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
		const FMatrix& WorldProjMatrix);
};
